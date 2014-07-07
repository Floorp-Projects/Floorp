/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef GONK_CAMERA_SOURCE_H_

#define GONK_CAMERA_SOURCE_H_

#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>
#include <camera/CameraParameters.h>
#include <utils/List.h>
#include <utils/RefBase.h>
#include <utils/String16.h>

#include "GonkCameraHwMgr.h"

namespace android {

class IMemory;

class GonkCameraSource : public MediaSource, public MediaBufferObserver {
public:

    static GonkCameraSource *Create(const sp<GonkCameraHardware>& aCameraHw,
                                    Size videoSize,
                                    int32_t frameRate,
                                    bool storeMetaDataInVideoBuffers = false);

    virtual ~GonkCameraSource();

    virtual status_t start(MetaData *params = NULL);
    virtual status_t stop() { return reset(); }
    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options = NULL);

    /**
     * Check whether a GonkCameraSource object is properly initialized.
     * Must call this method before stop().
     * @return OK if initialization has successfully completed.
     */
    virtual status_t initCheck() const;

    /**
     * Returns the MetaData associated with the GonkCameraSource,
     * including:
     * kKeyColorFormat: YUV color format of the video frames
     * kKeyWidth, kKeyHeight: dimension (in pixels) of the video frames
     * kKeySampleRate: frame rate in frames per second
     * kKeyMIMEType: always fixed to be MEDIA_MIMETYPE_VIDEO_RAW
     */
    virtual sp<MetaData> getFormat();

    /**
     * Tell whether this camera source stores meta data or real YUV
     * frame data in video buffers.
     *
     * @return true if meta data is stored in the video
     *      buffers; false if real YUV data is stored in
     *      the video buffers.
     */
    bool isMetaDataStoredInVideoBuffers() const;

    virtual void signalBufferReturned(MediaBuffer* buffer);

protected:

    enum CameraFlags {
        FLAGS_SET_CAMERA = 1L << 0,
        FLAGS_HOT_CAMERA = 1L << 1,
    };

    int32_t  mCameraFlags;
    Size     mVideoSize;
    int32_t  mNumInputBuffers;
    int32_t  mVideoFrameRate;
    int32_t  mColorFormat;
    status_t mInitCheck;

    sp<MetaData> mMeta;

    int64_t mStartTimeUs;
    int32_t mNumFramesReceived;
    int64_t mLastFrameTimestampUs;
    bool mStarted;
    int32_t mNumFramesEncoded;

    // Time between capture of two frames.
    int64_t mTimeBetweenFrameCaptureUs;

    GonkCameraSource(const sp<GonkCameraHardware>& aCameraHw,
                 Size videoSize, int32_t frameRate,
                 bool storeMetaDataInVideoBuffers = false);

    virtual int startCameraRecording();
    virtual void stopCameraRecording();
    virtual void releaseRecordingFrame(const sp<IMemory>& frame);

    // Returns true if need to skip the current frame.
    // Called from dataCallbackTimestamp.
    virtual bool skipCurrentFrame(int64_t timestampUs) {return false;}

    friend class GonkCameraSourceListener;
    // Callback called when still camera raw data is available.
    virtual void dataCallback(int32_t msgType, const sp<IMemory> &data) {}

    virtual void dataCallbackTimestamp(int64_t timestampUs, int32_t msgType,
            const sp<IMemory> &data);

private:

    Mutex mLock;
    Condition mFrameAvailableCondition;
    Condition mFrameCompleteCondition;
    List<sp<IMemory> > mFramesReceived;
    List<sp<IMemory> > mFramesBeingEncoded;
    List<int64_t> mFrameTimes;
    bool mRateLimit;

    int64_t mFirstFrameTimeUs;
    int32_t mNumFramesDropped;
    int32_t mNumGlitches;
    int64_t mGlitchDurationThresholdUs;
    bool mCollectStats;
    bool mIsMetaDataStoredInVideoBuffers;
    sp<GonkCameraHardware> mCameraHw;

    void releaseQueuedFrames();
    void releaseOneRecordingFrame(const sp<IMemory>& frame);

    status_t init(Size videoSize, int32_t frameRate,
                  bool storeMetaDataInVideoBuffers);
    status_t isCameraColorFormatSupported(const CameraParameters& params);
    status_t configureCamera(CameraParameters* params,
                    int32_t width, int32_t height,
                    int32_t frameRate);

    status_t checkVideoSize(const CameraParameters& params,
                    int32_t width, int32_t height);

    status_t checkFrameRate(const CameraParameters& params,
                    int32_t frameRate);

    void releaseCamera();
    status_t reset();

    GonkCameraSource(const GonkCameraSource &);
    GonkCameraSource &operator=(const GonkCameraSource &);
};

}  // namespace android

#endif  // GONK_CAMERA_SOURCE_H_
