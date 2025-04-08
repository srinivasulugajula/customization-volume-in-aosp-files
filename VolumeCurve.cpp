/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "APM::VolumeCurve"
#define LOG_NDEBUG 0

#include "VolumeCurve.h"
#include "TypeConverter.h"
#include <media/TypeConverter.h>

namespace android {

float VolumeCurve::volIndexToDb(int indexInUi, int volIndexMin, int volIndexMax) const
{
    ALOG_ASSERT(!mCurvePoints.isEmpty(), "Invalid volume curve");
    if (volIndexMin < 0 || volIndexMax < 0) {
        return NAN;
    }
    if (indexInUi < volIndexMin) {
        if (indexInUi == 0) {
            ALOGV("Custom VOLUME forcing mute for index 0 with min index %d", volIndexMin);
            return VOLUME_MIN_DB;
        }
        ALOGV("Custom VOLUME remapping index from %d to min index %d", indexInUi, volIndexMin);
        indexInUi = volIndexMin;
    } else if (indexInUi > volIndexMax) {
        ALOGV("Custom VOLUME remapping index from %d to max index %d", indexInUi, volIndexMax);
        indexInUi = volIndexMax;
    }

    size_t nbCurvePoints = mCurvePoints.size();
    int volIdx;
    if (volIndexMin == volIndexMax) {
        if (indexInUi == volIndexMin) {
            volIdx = volIndexMin;
        } else {
            ALOG_ASSERT(volIndexMin != volIndexMax, "Invalid volume index range & value: 0");
            return NAN;
        }
    } else {
        int nbSteps = 1 + mCurvePoints[nbCurvePoints - 1].mIndex - mCurvePoints[0].mIndex;
        volIdx = (nbSteps * (indexInUi - volIndexMin)) / (volIndexMax - volIndexMin);
    }

    size_t indexInUiPosition = mCurvePoints.orderOf(CurvePoint(volIdx, 0));
    if (indexInUiPosition >= nbCurvePoints) {
        return mCurvePoints[nbCurvePoints - 1].mAttenuationInMb / 100.0f;
    }
    if (indexInUiPosition == 0) {
        if (static_cast<uint32_t>(volIdx) != mCurvePoints[0].mIndex) {
            return VOLUME_MIN_DB;
        }
        return mCurvePoints[0].mAttenuationInMb / 100.0f;
    }

    // Logarithmic interpolation (custom modification)
    float minDb = mCurvePoints[indexInUiPosition - 1].mAttenuationInMb / 100.0f;
    float maxDb = mCurvePoints[indexInUiPosition].mAttenuationInMb / 100.0f;
    float minIdx = mCurvePoints[indexInUiPosition - 1].mIndex;
    float maxIdx = mCurvePoints[indexInUiPosition].mIndex;

    float normalized = (volIdx - minIdx) / (maxIdx - minIdx); // 0 to 1
    float logFactor = log10(1 + 9 * normalized) / log10(10);  // Logarithmic scaling (0 to 1)
    float decibels = minDb + logFactor * (maxDb - minDb);

    ALOGV("Custom VOLUME vol index=[%d %d %d], dB=[%.1f %.1f %.1f]",
          (int)minIdx, volIdx, (int)maxIdx, minDb, decibels, maxDb);

    return decibels;
}

void VolumeCurve::dump(String8 *dst, int spaces, bool curvePoints) const
{
    if (!curvePoints) {
        return;
    }
    dst->append(" {");
    for (size_t i = 0; i < mCurvePoints.size(); i++) {
        dst->appendFormat("%*s(%3d, %5d)", spaces, "", mCurvePoints[i].mIndex,
                          mCurvePoints[i].mAttenuationInMb);
        dst->appendFormat(i == (mCurvePoints.size() - 1) ? " }\n" : ", ");
    }
}

void VolumeCurves::dump(String8 *dst, int spaces, bool curvePoints) const
{
    if (!curvePoints) {
//        dst->appendFormat("%*s%02d      %s         %03d        %03d        ", spaces, "",
//                          mStream, mCanBeMuted ? "true " : "false", mIndexMin, mIndexMax);
        dst->appendFormat("%*s Can be muted  Index Min  Index Max  Index Cur [device : index]...\n",
                          spaces + 1, "");
        dst->appendFormat("%*s %s         %02d         %02d         ", spaces + 1, "",
                          mCanBeMuted ? "true " : "false", mIndexMin, mIndexMax);
        for (const auto &pair : mIndexCur) {
            dst->appendFormat("%04x : %02d, ", pair.first, pair.second);
        }
        dst->appendFormat("\n");
        return;
    }
    std::string streamNames;
    for (const auto &stream : mStreams) {
        streamNames += android::toString(stream) + "("+std::to_string(stream)+") ";
    }
    dst->appendFormat("%*sVolume Curves Streams/Attributes, Curve points Streams for device"
                      " category (index, attenuation in millibel)\n", spaces, "");
    dst->appendFormat("%*s Streams: %s \n", spaces, "", streamNames.c_str());
    if (!mAttributes.empty()) dst->appendFormat("%*s Attributes:", spaces, "");
    for (const auto &attributes : mAttributes) {
        std::string attStr = attributes == defaultAttr ? "{ Any }" : android::toString(attributes);
        dst->appendFormat("%*s %s\n", attributes == mAttributes.front() ? 0 : spaces + 13, "",
                          attStr.c_str());
    }
    for (size_t i = 0; i < size(); i++) {
        std::string deviceCatLiteral;
        DeviceCategoryConverter::toString(keyAt(i), deviceCatLiteral);
        dst->appendFormat("%*s %s :", spaces, "", deviceCatLiteral.c_str());
        valueAt(i)->dump(dst, 1, true);
    }
}

} // namespace android
