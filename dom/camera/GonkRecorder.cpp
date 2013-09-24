/*
 * Copyright (C) 2009 The Android Open Source Project
 * Copyright (C) 2013 Mozilla Foundation
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

#include "nsDebug.h"
#define DOM_CAMERA_LOG_LEVEL        3
#include "CameraCommon.h"
#include "GonkCameraSource.h"
#include "GonkRecorder.h"

#define RE_LOGD(fmt, ...) DOM_CAMERA_LOGA("[%s:%d]" fmt,__FILE__,__LINE__, ## __VA_ARGS__)
#define RE_LOGV(fmt, ...) DOM_CAMERA_LOGI("[%s:%d]" fmt,__FILE__,__LINE__, ## __VA_ARGS__)
#define RE_LOGI(fmt, ...) DOM_CAMERA_LOGI("[%s:%d]" fmt,__FILE__,__LINE__, ## __VA_ARGS__)
#define RE_LOGW(fmt, ...) DOM_CAMERA_LOGW("[%s:%d]" fmt,__FILE__,__LINE__, ## __VA_ARGS__)
#define RE_LOGE(fmt, ...) DOM_CAMERA_LOGE("[%s:%d]" fmt,__FILE__,__LINE__, ## __VA_ARGS__)

#include <binder/IPCThreadState.h>
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 18
# include <media/openmax/OMX_Audio.h>
#endif
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/AudioSource.h>
#include <media/stagefright/AMRWriter.h>
#include <media/stagefright/AACWriter.h>
#include <media/stagefright/MPEG2TSWriter.h>
#include <media/stagefright/MPEG4Writer.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/OMXClient.h>
#include <media/stagefright/OMXCodec.h>
#include <media/MediaProfiles.h>

#include <utils/Errors.h>
#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>

#include <cutils/properties.h>
#include <system/audio.h>

namespace android {

GonkRecorder::GonkRecorder()
    : mWriter(NULL),
      mOutputFd(-1),
      mAudioSource(AUDIO_SOURCE_CNT),
      mVideoSource(VIDEO_SOURCE_LIST_END),
      mStarted(false) {

    RE_LOGV("Constructor");
    reset();
}

GonkRecorder::~GonkRecorder() {
    RE_LOGV("Destructor");
    stop();
}

status_t GonkRecorder::init() {
    RE_LOGV("init");
    return OK;
}

status_t GonkRecorder::setAudioSource(audio_source_t as) {
    RE_LOGV("setAudioSource: %d", as);
    if (as < AUDIO_SOURCE_DEFAULT ||
        as >= AUDIO_SOURCE_CNT) {
        RE_LOGE("Invalid audio source: %d", as);
        return BAD_VALUE;
    }

    if (as == AUDIO_SOURCE_DEFAULT) {
        mAudioSource = AUDIO_SOURCE_MIC;
    } else {
        mAudioSource = as;
    }

    return OK;
}

status_t GonkRecorder::setVideoSource(video_source vs) {
    RE_LOGV("setVideoSource: %d", vs);
    if (vs < VIDEO_SOURCE_DEFAULT ||
        vs >= VIDEO_SOURCE_LIST_END) {
        RE_LOGE("Invalid video source: %d", vs);
        return BAD_VALUE;
    }

    if (vs == VIDEO_SOURCE_DEFAULT) {
        mVideoSource = VIDEO_SOURCE_CAMERA;
    } else {
        mVideoSource = vs;
    }

    return OK;
}

status_t GonkRecorder::setOutputFormat(output_format of) {
    RE_LOGV("setOutputFormat: %d", of);
    if (of < OUTPUT_FORMAT_DEFAULT ||
        of >= OUTPUT_FORMAT_LIST_END) {
        RE_LOGE("Invalid output format: %d", of);
        return BAD_VALUE;
    }

    if (of == OUTPUT_FORMAT_DEFAULT) {
        mOutputFormat = OUTPUT_FORMAT_THREE_GPP;
    } else {
        mOutputFormat = of;
    }

    return OK;
}

status_t GonkRecorder::setAudioEncoder(audio_encoder ae) {
    RE_LOGV("setAudioEncoder: %d", ae);
    if (ae < AUDIO_ENCODER_DEFAULT ||
        ae >= AUDIO_ENCODER_LIST_END) {
        RE_LOGE("Invalid audio encoder: %d", ae);
        return BAD_VALUE;
    }

    if (ae == AUDIO_ENCODER_DEFAULT) {
        mAudioEncoder = AUDIO_ENCODER_AMR_NB;
    } else {
        mAudioEncoder = ae;
    }

    return OK;
}

status_t GonkRecorder::setVideoEncoder(video_encoder ve) {
    RE_LOGV("setVideoEncoder: %d", ve);
    if (ve < VIDEO_ENCODER_DEFAULT ||
        ve >= VIDEO_ENCODER_LIST_END) {
        RE_LOGE("Invalid video encoder: %d", ve);
        return BAD_VALUE;
    }

    if (ve == VIDEO_ENCODER_DEFAULT) {
        mVideoEncoder = VIDEO_ENCODER_H263;
    } else {
        mVideoEncoder = ve;
    }

    return OK;
}

status_t GonkRecorder::setVideoSize(int width, int height) {
    RE_LOGV("setVideoSize: %dx%d", width, height);
    if (width <= 0 || height <= 0) {
        RE_LOGE("Invalid video size: %dx%d", width, height);
        return BAD_VALUE;
    }

    // Additional check on the dimension will be performed later
    mVideoWidth = width;
    mVideoHeight = height;

    return OK;
}

status_t GonkRecorder::setVideoFrameRate(int frames_per_second) {
    RE_LOGV("setVideoFrameRate: %d", frames_per_second);
    if ((frames_per_second <= 0 && frames_per_second != -1) ||
        frames_per_second > 120) {
        RE_LOGE("Invalid video frame rate: %d", frames_per_second);
        return BAD_VALUE;
    }

    // Additional check on the frame rate will be performed later
    mFrameRate = frames_per_second;

    return OK;
}

status_t GonkRecorder::setOutputFile(const char *path) {
    RE_LOGE("setOutputFile(const char*) must not be called");
    // We don't actually support this at all, as the media_server process
    // no longer has permissions to create files.

    return -EPERM;
}

status_t GonkRecorder::setOutputFile(int fd, int64_t offset, int64_t length) {
    RE_LOGV("setOutputFile: %d, %lld, %lld", fd, offset, length);
    // These don't make any sense, do they?
    CHECK_EQ(offset, 0ll);
    CHECK_EQ(length, 0ll);

    if (fd < 0) {
        RE_LOGE("Invalid file descriptor: %d", fd);
        return -EBADF;
    }

    if (mOutputFd >= 0) {
        ::close(mOutputFd);
    }
    mOutputFd = dup(fd);

    return OK;
}

// Attempt to parse an int64 literal optionally surrounded by whitespace,
// returns true on success, false otherwise.
static bool safe_strtoi64(const char *s, int64_t *val) {
    char *end;

    // It is lame, but according to man page, we have to set errno to 0
    // before calling strtoll().
    errno = 0;
    *val = strtoll(s, &end, 10);

    if (end == s || errno == ERANGE) {
        return false;
    }

    // Skip trailing whitespace
    while (isspace(*end)) {
        ++end;
    }

    // For a successful return, the string must contain nothing but a valid
    // int64 literal optionally surrounded by whitespace.

    return *end == '\0';
}

// Return true if the value is in [0, 0x007FFFFFFF]
static bool safe_strtoi32(const char *s, int32_t *val) {
    int64_t temp;
    if (safe_strtoi64(s, &temp)) {
        if (temp >= 0 && temp <= 0x007FFFFFFF) {
            *val = static_cast<int32_t>(temp);
            return true;
        }
    }
    return false;
}

// Trim both leading and trailing whitespace from the given string.
static void TrimString(String8 *s) {
    size_t num_bytes = s->bytes();
    const char *data = s->string();

    size_t leading_space = 0;
    while (leading_space < num_bytes && isspace(data[leading_space])) {
        ++leading_space;
    }

    size_t i = num_bytes;
    while (i > leading_space && isspace(data[i - 1])) {
        --i;
    }

    s->setTo(String8(&data[leading_space], i - leading_space));
}

status_t GonkRecorder::setParamAudioSamplingRate(int32_t sampleRate) {
    RE_LOGV("setParamAudioSamplingRate: %d", sampleRate);
    if (sampleRate <= 0) {
        RE_LOGE("Invalid audio sampling rate: %d", sampleRate);
        return BAD_VALUE;
    }

    // Additional check on the sample rate will be performed later.
    mSampleRate = sampleRate;
    return OK;
}

status_t GonkRecorder::setParamAudioNumberOfChannels(int32_t channels) {
    RE_LOGV("setParamAudioNumberOfChannels: %d", channels);
    if (channels <= 0 || channels >= 3) {
        RE_LOGE("Invalid number of audio channels: %d", channels);
        return BAD_VALUE;
    }

    // Additional check on the number of channels will be performed later.
    mAudioChannels = channels;
    return OK;
}

status_t GonkRecorder::setParamAudioEncodingBitRate(int32_t bitRate) {
    RE_LOGV("setParamAudioEncodingBitRate: %d", bitRate);
    if (bitRate <= 0) {
        RE_LOGE("Invalid audio encoding bit rate: %d", bitRate);
        return BAD_VALUE;
    }

    // The target bit rate may not be exactly the same as the requested.
    // It depends on many factors, such as rate control, and the bit rate
    // range that a specific encoder supports. The mismatch between the
    // the target and requested bit rate will NOT be treated as an error.
    mAudioBitRate = bitRate;
    return OK;
}

status_t GonkRecorder::setParamVideoEncodingBitRate(int32_t bitRate) {
    RE_LOGV("setParamVideoEncodingBitRate: %d", bitRate);
    if (bitRate <= 0) {
        RE_LOGE("Invalid video encoding bit rate: %d", bitRate);
        return BAD_VALUE;
    }

    // The target bit rate may not be exactly the same as the requested.
    // It depends on many factors, such as rate control, and the bit rate
    // range that a specific encoder supports. The mismatch between the
    // the target and requested bit rate will NOT be treated as an error.
    mVideoBitRate = bitRate;
    return OK;
}

// Always rotate clockwise, and only support 0, 90, 180 and 270 for now.
status_t GonkRecorder::setParamVideoRotation(int32_t degrees) {
    RE_LOGV("setParamVideoRotation: %d", degrees);
    if (degrees < 0 || degrees % 90 != 0) {
        RE_LOGE("Unsupported video rotation angle: %d", degrees);
        return BAD_VALUE;
    }
    mRotationDegrees = degrees % 360;
    return OK;
}

status_t GonkRecorder::setParamMaxFileDurationUs(int64_t timeUs) {
    RE_LOGV("setParamMaxFileDurationUs: %lld us", timeUs);

    // This is meant for backward compatibility for MediaRecorder.java
    if (timeUs <= 0) {
        RE_LOGW("Max file duration is not positive: %lld us. Disabling duration limit.", timeUs);
        timeUs = 0; // Disable the duration limit for zero or negative values.
    } else if (timeUs <= 100000LL) {  // XXX: 100 milli-seconds
        RE_LOGE("Max file duration is too short: %lld us", timeUs);
        return BAD_VALUE;
    }

    if (timeUs <= 15 * 1000000LL) {
        RE_LOGW("Target duration (%lld us) too short to be respected", timeUs);
    }
    mMaxFileDurationUs = timeUs;
    return OK;
}

status_t GonkRecorder::setParamMaxFileSizeBytes(int64_t bytes) {
    RE_LOGV("setParamMaxFileSizeBytes: %lld bytes", bytes);

    // This is meant for backward compatibility for MediaRecorder.java
    if (bytes <= 0) {
        RE_LOGW("Max file size is not positive: %lld bytes. "
             "Disabling file size limit.", bytes);
        bytes = 0; // Disable the file size limit for zero or negative values.
    } else if (bytes <= 1024) {  // XXX: 1 kB
        RE_LOGE("Max file size is too small: %lld bytes", bytes);
        return BAD_VALUE;
    }

    if (bytes <= 100 * 1024) {
        RE_LOGW("Target file size (%lld bytes) is too small to be respected", bytes);
    }

    mMaxFileSizeBytes = bytes;
    return OK;
}

status_t GonkRecorder::setParamInterleaveDuration(int32_t durationUs) {
    RE_LOGV("setParamInterleaveDuration: %d", durationUs);
    if (durationUs <= 500000) {           //  500 ms
        // If interleave duration is too small, it is very inefficient to do
        // interleaving since the metadata overhead will count for a significant
        // portion of the saved contents
        RE_LOGE("Audio/video interleave duration is too small: %d us", durationUs);
        return BAD_VALUE;
    } else if (durationUs >= 10000000) {  // 10 seconds
        // If interleaving duration is too large, it can cause the recording
        // session to use too much memory since we have to save the output
        // data before we write them out
        RE_LOGE("Audio/video interleave duration is too large: %d us", durationUs);
        return BAD_VALUE;
    }
    mInterleaveDurationUs = durationUs;
    return OK;
}

// If seconds <  0, only the first frame is I frame, and rest are all P frames
// If seconds == 0, all frames are encoded as I frames. No P frames
// If seconds >  0, it is the time spacing (seconds) between 2 neighboring I frames
status_t GonkRecorder::setParamVideoIFramesInterval(int32_t seconds) {
    RE_LOGV("setParamVideoIFramesInterval: %d seconds", seconds);
    mIFramesIntervalSec = seconds;
    return OK;
}

status_t GonkRecorder::setParam64BitFileOffset(bool use64Bit) {
    RE_LOGV("setParam64BitFileOffset: %s",
        use64Bit? "use 64 bit file offset": "use 32 bit file offset");
    mUse64BitFileOffset = use64Bit;
    return OK;
}

status_t GonkRecorder::setParamVideoCameraId(int32_t cameraId) {
    RE_LOGV("setParamVideoCameraId: %d", cameraId);
    if (cameraId < 0) {
        return BAD_VALUE;
    }
    mCameraId = cameraId;
    return OK;
}

status_t GonkRecorder::setParamTrackTimeStatus(int64_t timeDurationUs) {
    RE_LOGV("setParamTrackTimeStatus: %lld", timeDurationUs);
    if (timeDurationUs < 20000) {  // Infeasible if shorter than 20 ms?
        RE_LOGE("Tracking time duration too short: %lld us", timeDurationUs);
        return BAD_VALUE;
    }
    mTrackEveryTimeDurationUs = timeDurationUs;
    return OK;
}

status_t GonkRecorder::setParamVideoEncoderProfile(int32_t profile) {
    RE_LOGV("setParamVideoEncoderProfile: %d", profile);

    // Additional check will be done later when we load the encoder.
    // For now, we are accepting values defined in OpenMAX IL.
    mVideoEncoderProfile = profile;
    return OK;
}

status_t GonkRecorder::setParamVideoEncoderLevel(int32_t level) {
    RE_LOGV("setParamVideoEncoderLevel: %d", level);

    // Additional check will be done later when we load the encoder.
    // For now, we are accepting values defined in OpenMAX IL.
    mVideoEncoderLevel = level;
    return OK;
}

status_t GonkRecorder::setParamMovieTimeScale(int32_t timeScale) {
    RE_LOGV("setParamMovieTimeScale: %d", timeScale);

    // The range is set to be the same as the audio's time scale range
    // since audio's time scale has a wider range.
    if (timeScale < 600 || timeScale > 96000) {
        RE_LOGE("Time scale (%d) for movie is out of range [600, 96000]", timeScale);
        return BAD_VALUE;
    }
    mMovieTimeScale = timeScale;
    return OK;
}

status_t GonkRecorder::setParamVideoTimeScale(int32_t timeScale) {
    RE_LOGV("setParamVideoTimeScale: %d", timeScale);

    // 60000 is chosen to make sure that each video frame from a 60-fps
    // video has 1000 ticks.
    if (timeScale < 600 || timeScale > 60000) {
        RE_LOGE("Time scale (%d) for video is out of range [600, 60000]", timeScale);
        return BAD_VALUE;
    }
    mVideoTimeScale = timeScale;
    return OK;
}

status_t GonkRecorder::setParamAudioTimeScale(int32_t timeScale) {
    RE_LOGV("setParamAudioTimeScale: %d", timeScale);

    // 96000 Hz is the highest sampling rate support in AAC.
    if (timeScale < 600 || timeScale > 96000) {
        RE_LOGE("Time scale (%d) for audio is out of range [600, 96000]", timeScale);
        return BAD_VALUE;
    }
    mAudioTimeScale = timeScale;
    return OK;
}

status_t GonkRecorder::setParamGeoDataLongitude(
    int64_t longitudex10000) {

    if (longitudex10000 > 1800000 || longitudex10000 < -1800000) {
        return BAD_VALUE;
    }
    mLongitudex10000 = longitudex10000;
    return OK;
}

status_t GonkRecorder::setParamGeoDataLatitude(
    int64_t latitudex10000) {

    if (latitudex10000 > 900000 || latitudex10000 < -900000) {
        return BAD_VALUE;
    }
    mLatitudex10000 = latitudex10000;
    return OK;
}

status_t GonkRecorder::setParameter(
        const String8 &key, const String8 &value) {
    RE_LOGV("setParameter: key (%s) => value (%s)", key.string(), value.string());
    if (key == "max-duration") {
        int64_t max_duration_ms;
        if (safe_strtoi64(value.string(), &max_duration_ms)) {
            return setParamMaxFileDurationUs(1000LL * max_duration_ms);
        }
    } else if (key == "max-filesize") {
        int64_t max_filesize_bytes;
        if (safe_strtoi64(value.string(), &max_filesize_bytes)) {
            return setParamMaxFileSizeBytes(max_filesize_bytes);
        }
    } else if (key == "interleave-duration-us") {
        int32_t durationUs;
        if (safe_strtoi32(value.string(), &durationUs)) {
            return setParamInterleaveDuration(durationUs);
        }
    } else if (key == "param-movie-time-scale") {
        int32_t timeScale;
        if (safe_strtoi32(value.string(), &timeScale)) {
            return setParamMovieTimeScale(timeScale);
        }
    } else if (key == "param-use-64bit-offset") {
        int32_t use64BitOffset;
        if (safe_strtoi32(value.string(), &use64BitOffset)) {
            return setParam64BitFileOffset(use64BitOffset != 0);
        }
    } else if (key == "param-geotag-longitude") {
        int64_t longitudex10000;
        if (safe_strtoi64(value.string(), &longitudex10000)) {
            return setParamGeoDataLongitude(longitudex10000);
        }
    } else if (key == "param-geotag-latitude") {
        int64_t latitudex10000;
        if (safe_strtoi64(value.string(), &latitudex10000)) {
            return setParamGeoDataLatitude(latitudex10000);
        }
    } else if (key == "param-track-time-status") {
        int64_t timeDurationUs;
        if (safe_strtoi64(value.string(), &timeDurationUs)) {
            return setParamTrackTimeStatus(timeDurationUs);
        }
    } else if (key == "audio-param-sampling-rate") {
        int32_t sampling_rate;
        if (safe_strtoi32(value.string(), &sampling_rate)) {
            return setParamAudioSamplingRate(sampling_rate);
        }
    } else if (key == "audio-param-number-of-channels") {
        int32_t number_of_channels;
        if (safe_strtoi32(value.string(), &number_of_channels)) {
            return setParamAudioNumberOfChannels(number_of_channels);
        }
    } else if (key == "audio-param-encoding-bitrate") {
        int32_t audio_bitrate;
        if (safe_strtoi32(value.string(), &audio_bitrate)) {
            return setParamAudioEncodingBitRate(audio_bitrate);
        }
    } else if (key == "audio-param-time-scale") {
        int32_t timeScale;
        if (safe_strtoi32(value.string(), &timeScale)) {
            return setParamAudioTimeScale(timeScale);
        }
    } else if (key == "video-param-encoding-bitrate") {
        int32_t video_bitrate;
        if (safe_strtoi32(value.string(), &video_bitrate)) {
            return setParamVideoEncodingBitRate(video_bitrate);
        }
    } else if (key == "video-param-rotation-angle-degrees") {
        int32_t degrees;
        if (safe_strtoi32(value.string(), &degrees)) {
            return setParamVideoRotation(degrees);
        }
    } else if (key == "video-param-i-frames-interval") {
        int32_t seconds;
        if (safe_strtoi32(value.string(), &seconds)) {
            return setParamVideoIFramesInterval(seconds);
        }
    } else if (key == "video-param-encoder-profile") {
        int32_t profile;
        if (safe_strtoi32(value.string(), &profile)) {
            return setParamVideoEncoderProfile(profile);
        }
    } else if (key == "video-param-encoder-level") {
        int32_t level;
        if (safe_strtoi32(value.string(), &level)) {
            return setParamVideoEncoderLevel(level);
        }
    } else if (key == "video-param-camera-id") {
        int32_t cameraId;
        if (safe_strtoi32(value.string(), &cameraId)) {
            return setParamVideoCameraId(cameraId);
        }
    } else if (key == "video-param-time-scale") {
        int32_t timeScale;
        if (safe_strtoi32(value.string(), &timeScale)) {
            return setParamVideoTimeScale(timeScale);
        }
    } else {
        RE_LOGE("setParameter: failed to find key %s", key.string());
    }
    return BAD_VALUE;
}

status_t GonkRecorder::setParameters(const String8 &params) {
    RE_LOGV("setParameters: %s", params.string());
    const char *cparams = params.string();
    const char *key_start = cparams;
    for (;;) {
        const char *equal_pos = strchr(key_start, '=');
        if (equal_pos == NULL) {
            RE_LOGE("Parameters %s miss a value", cparams);
            return BAD_VALUE;
        }
        String8 key(key_start, equal_pos - key_start);
        TrimString(&key);
        if (key.length() == 0) {
            RE_LOGE("Parameters %s contains an empty key", cparams);
            return BAD_VALUE;
        }
        const char *value_start = equal_pos + 1;
        const char *semicolon_pos = strchr(value_start, ';');
        String8 value;
        if (semicolon_pos == NULL) {
            value.setTo(value_start);
        } else {
            value.setTo(value_start, semicolon_pos - value_start);
        }
        if (setParameter(key, value) != OK) {
            return BAD_VALUE;
        }
        if (semicolon_pos == NULL) {
            break;  // Reaches the end
        }
        key_start = semicolon_pos + 1;
    }
    return OK;
}

status_t GonkRecorder::setListener(const sp<IMediaRecorderClient> &listener) {
    mListener = listener;

    return OK;
}

status_t GonkRecorder::setClientName(const String16& clientName) {
    mClientName = clientName;

    return OK;
}

status_t GonkRecorder::prepare() {
    return OK;
}

status_t GonkRecorder::start() {
    CHECK_GE(mOutputFd, 0);

    // Get UID here for permission checking
    mClientUid = IPCThreadState::self()->getCallingUid();
    if (mWriter != NULL) {
        RE_LOGE("File writer is not avaialble");
        return UNKNOWN_ERROR;
    }

    status_t status = OK;

    switch (mOutputFormat) {
        case OUTPUT_FORMAT_DEFAULT:
        case OUTPUT_FORMAT_THREE_GPP:
        case OUTPUT_FORMAT_MPEG_4:
            status = startMPEG4Recording();
            break;

        case OUTPUT_FORMAT_AMR_NB:
        case OUTPUT_FORMAT_AMR_WB:
            status = startAMRRecording();
            break;

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 18
        case OUTPUT_FORMAT_AAC_ADIF:
        case OUTPUT_FORMAT_AAC_ADTS:
            status = startAACRecording();
            break;
#endif

        case OUTPUT_FORMAT_RTP_AVP:
            status = startRTPRecording();
            break;

        case OUTPUT_FORMAT_MPEG2TS:
            status = startMPEG2TSRecording();
            break;

        default:
            RE_LOGE("Unsupported output file format: %d", mOutputFormat);
            status = UNKNOWN_ERROR;
            break;
    }

    if ((status == OK) && (!mStarted)) {
        mStarted = true;
    }

    return status;
}

sp<MediaSource> GonkRecorder::createAudioSource() {
    sp<AudioSource> audioSource =
        new AudioSource(
                mAudioSource,
                mSampleRate,
                mAudioChannels);

    status_t err = audioSource->initCheck();

    if (err != OK) {
        RE_LOGE("audio source is not initialized");
        return NULL;
    }

    sp<MetaData> encMeta = new MetaData;
    const char *mime;
    switch (mAudioEncoder) {
        case AUDIO_ENCODER_AMR_NB:
        case AUDIO_ENCODER_DEFAULT:
            mime = MEDIA_MIMETYPE_AUDIO_AMR_NB;
            break;
        case AUDIO_ENCODER_AMR_WB:
            mime = MEDIA_MIMETYPE_AUDIO_AMR_WB;
            break;
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 18
        case AUDIO_ENCODER_AAC:
            mime = MEDIA_MIMETYPE_AUDIO_AAC;
            encMeta->setInt32(kKeyAACProfile, OMX_AUDIO_AACObjectLC);
            break;
        case AUDIO_ENCODER_HE_AAC:
            mime = MEDIA_MIMETYPE_AUDIO_AAC;
            encMeta->setInt32(kKeyAACProfile, OMX_AUDIO_AACObjectHE);
            break;
        case AUDIO_ENCODER_AAC_ELD:
            mime = MEDIA_MIMETYPE_AUDIO_AAC;
            encMeta->setInt32(kKeyAACProfile, OMX_AUDIO_AACObjectELD);
            break;
#endif
        default:
            RE_LOGE("Unknown audio encoder: %d", mAudioEncoder);
            return NULL;
    }
    encMeta->setCString(kKeyMIMEType, mime);

    int32_t maxInputSize;
    CHECK(audioSource->getFormat()->findInt32(
                kKeyMaxInputSize, &maxInputSize));

    encMeta->setInt32(kKeyMaxInputSize, maxInputSize);
    encMeta->setInt32(kKeyChannelCount, mAudioChannels);
    encMeta->setInt32(kKeySampleRate, mSampleRate);
    encMeta->setInt32(kKeyBitRate, mAudioBitRate);
    if (mAudioTimeScale > 0) {
        encMeta->setInt32(kKeyTimeScale, mAudioTimeScale);
    }

    // OMXClient::connect() always returns OK and abort's fatally if
    // it can't connect.
    OMXClient client;
    // CHECK_EQ causes an abort if the given condition fails.
    CHECK_EQ(client.connect(), (status_t)OK);
    sp<MediaSource> audioEncoder =
        OMXCodec::Create(client.interface(), encMeta,
                         true /* createEncoder */, audioSource);
    mAudioSourceNode = audioSource;

    return audioEncoder;
}

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 18
status_t GonkRecorder::startAACRecording() {
    // FIXME:
    // Add support for OUTPUT_FORMAT_AAC_ADIF
    CHECK_EQ(mOutputFormat, OUTPUT_FORMAT_AAC_ADTS);

    CHECK(mAudioEncoder == AUDIO_ENCODER_AAC ||
          mAudioEncoder == AUDIO_ENCODER_HE_AAC ||
          mAudioEncoder == AUDIO_ENCODER_AAC_ELD);
    CHECK(mAudioSource != AUDIO_SOURCE_CNT);

    mWriter = new AACWriter(mOutputFd);
    status_t status = startRawAudioRecording();
    if (status != OK) {
        mWriter.clear();
        mWriter = NULL;
    }

    return status;
}
#endif

status_t GonkRecorder::startAMRRecording() {
    CHECK(mOutputFormat == OUTPUT_FORMAT_AMR_NB ||
          mOutputFormat == OUTPUT_FORMAT_AMR_WB);

    if (mOutputFormat == OUTPUT_FORMAT_AMR_NB) {
        if (mAudioEncoder != AUDIO_ENCODER_DEFAULT &&
            mAudioEncoder != AUDIO_ENCODER_AMR_NB) {
            RE_LOGE("Invalid encoder %d used for AMRNB recording",
                    mAudioEncoder);
            return BAD_VALUE;
        }
    } else {  // mOutputFormat must be OUTPUT_FORMAT_AMR_WB
        if (mAudioEncoder != AUDIO_ENCODER_AMR_WB) {
            RE_LOGE("Invlaid encoder %d used for AMRWB recording",
                    mAudioEncoder);
            return BAD_VALUE;
        }
    }

    mWriter = new AMRWriter(mOutputFd);
    status_t status = startRawAudioRecording();
    if (status != OK) {
        mWriter.clear();
        mWriter = NULL;
    }
    return status;
}

status_t GonkRecorder::startRawAudioRecording() {
    if (mAudioSource >= AUDIO_SOURCE_CNT) {
        RE_LOGE("Invalid audio source: %d", mAudioSource);
        return BAD_VALUE;
    }

    status_t status = BAD_VALUE;
    if (OK != (status = checkAudioEncoderCapabilities())) {
        return status;
    }

    sp<MediaSource> audioEncoder = createAudioSource();
    if (audioEncoder == NULL) {
        return UNKNOWN_ERROR;
    }

    CHECK(mWriter != 0);
    mWriter->addSource(audioEncoder);

    if (mMaxFileDurationUs != 0) {
        mWriter->setMaxFileDuration(mMaxFileDurationUs);
    }
    if (mMaxFileSizeBytes != 0) {
        mWriter->setMaxFileSize(mMaxFileSizeBytes);
    }
    mWriter->setListener(mListener);
    mWriter->start();

    return OK;
}

status_t GonkRecorder::startRTPRecording() {
    return INVALID_OPERATION;
}

status_t GonkRecorder::startMPEG2TSRecording() {
    CHECK_EQ(mOutputFormat, OUTPUT_FORMAT_MPEG2TS);

    sp<MediaWriter> writer = new MPEG2TSWriter(mOutputFd);

    if (mAudioSource != AUDIO_SOURCE_CNT) {
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 18
        if (mAudioEncoder != AUDIO_ENCODER_AAC &&
            mAudioEncoder != AUDIO_ENCODER_HE_AAC &&
            mAudioEncoder != AUDIO_ENCODER_AAC_ELD) {
            return ERROR_UNSUPPORTED;
        }
#endif
        status_t err = setupAudioEncoder(writer);

        if (err != OK) {
            return err;
        }
    }

    if (mVideoSource < VIDEO_SOURCE_LIST_END) {
        if (mVideoEncoder != VIDEO_ENCODER_H264) {
            return ERROR_UNSUPPORTED;
        }

        sp<MediaSource> mediaSource;
        status_t err = setupMediaSource(&mediaSource);
        if (err != OK) {
            return err;
        }

        sp<MediaSource> encoder;
        err = setupVideoEncoder(mediaSource, mVideoBitRate, &encoder);

        if (err != OK) {
            return err;
        }

        writer->addSource(encoder);
    }

    if (mMaxFileDurationUs != 0) {
        writer->setMaxFileDuration(mMaxFileDurationUs);
    }

    if (mMaxFileSizeBytes != 0) {
        writer->setMaxFileSize(mMaxFileSizeBytes);
    }

    mWriter = writer;

    return mWriter->start();
}

void GonkRecorder::clipVideoFrameRate() {
    RE_LOGV("clipVideoFrameRate: encoder %d", mVideoEncoder);
    int minFrameRate = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.fps.min", mVideoEncoder);
    int maxFrameRate = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.fps.max", mVideoEncoder);
    if (mFrameRate < minFrameRate && minFrameRate != -1) {
        RE_LOGW("Intended video encoding frame rate (%d fps) is too small"
             " and will be set to (%d fps)", mFrameRate, minFrameRate);
        mFrameRate = minFrameRate;
    } else if (mFrameRate > maxFrameRate && maxFrameRate != -1) {
        RE_LOGW("Intended video encoding frame rate (%d fps) is too large"
             " and will be set to (%d fps)", mFrameRate, maxFrameRate);
        mFrameRate = maxFrameRate;
    }
}

void GonkRecorder::clipVideoBitRate() {
    RE_LOGV("clipVideoBitRate: encoder %d", mVideoEncoder);
    int minBitRate = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.bps.min", mVideoEncoder);
    int maxBitRate = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.bps.max", mVideoEncoder);
    if (mVideoBitRate < minBitRate && minBitRate != -1) {
        RE_LOGW("Intended video encoding bit rate (%d bps) is too small"
             " and will be set to (%d bps)", mVideoBitRate, minBitRate);
        mVideoBitRate = minBitRate;
    } else if (mVideoBitRate > maxBitRate && maxBitRate != -1) {
        RE_LOGW("Intended video encoding bit rate (%d bps) is too large"
             " and will be set to (%d bps)", mVideoBitRate, maxBitRate);
        mVideoBitRate = maxBitRate;
    }
}

void GonkRecorder::clipVideoFrameWidth() {
    RE_LOGV("clipVideoFrameWidth: encoder %d", mVideoEncoder);
    int minFrameWidth = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.width.min", mVideoEncoder);
    int maxFrameWidth = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.width.max", mVideoEncoder);
    if (mVideoWidth < minFrameWidth && minFrameWidth != -1) {
        RE_LOGW("Intended video encoding frame width (%d) is too small"
             " and will be set to (%d)", mVideoWidth, minFrameWidth);
        mVideoWidth = minFrameWidth;
    } else if (mVideoWidth > maxFrameWidth && maxFrameWidth != -1) {
        RE_LOGW("Intended video encoding frame width (%d) is too large"
             " and will be set to (%d)", mVideoWidth, maxFrameWidth);
        mVideoWidth = maxFrameWidth;
    }
}

status_t GonkRecorder::checkVideoEncoderCapabilities() {

    // Dont clip for time lapse capture as encoder will have enough
    // time to encode because of slow capture rate of time lapse.
    clipVideoBitRate();
    clipVideoFrameRate();
    clipVideoFrameWidth();
    clipVideoFrameHeight();
    setDefaultProfileIfNecessary();
    return OK;
}

// Set to use AVC baseline profile if the encoding parameters matches
// CAMCORDER_QUALITY_LOW profile; this is for the sake of MMS service.
void GonkRecorder::setDefaultProfileIfNecessary() {
    RE_LOGV("setDefaultProfileIfNecessary");

    camcorder_quality quality = CAMCORDER_QUALITY_LOW;

    int64_t durationUs   = mEncoderProfiles->getCamcorderProfileParamByName(
                                "duration", mCameraId, quality) * 1000000LL;

    int fileFormat       = mEncoderProfiles->getCamcorderProfileParamByName(
                                "file.format", mCameraId, quality);

    int videoCodec       = mEncoderProfiles->getCamcorderProfileParamByName(
                                "vid.codec", mCameraId, quality);

    int videoBitRate     = mEncoderProfiles->getCamcorderProfileParamByName(
                                "vid.bps", mCameraId, quality);

    int videoFrameRate   = mEncoderProfiles->getCamcorderProfileParamByName(
                                "vid.fps", mCameraId, quality);

    int videoFrameWidth  = mEncoderProfiles->getCamcorderProfileParamByName(
                                "vid.width", mCameraId, quality);

    int videoFrameHeight = mEncoderProfiles->getCamcorderProfileParamByName(
                                "vid.height", mCameraId, quality);

    int audioCodec       = mEncoderProfiles->getCamcorderProfileParamByName(
                                "aud.codec", mCameraId, quality);

    int audioBitRate     = mEncoderProfiles->getCamcorderProfileParamByName(
                                "aud.bps", mCameraId, quality);

    int audioSampleRate  = mEncoderProfiles->getCamcorderProfileParamByName(
                                "aud.hz", mCameraId, quality);

    int audioChannels    = mEncoderProfiles->getCamcorderProfileParamByName(
                                "aud.ch", mCameraId, quality);

    if (durationUs == mMaxFileDurationUs &&
        fileFormat == mOutputFormat &&
        videoCodec == mVideoEncoder &&
        videoBitRate == mVideoBitRate &&
        videoFrameRate == mFrameRate &&
        videoFrameWidth == mVideoWidth &&
        videoFrameHeight == mVideoHeight &&
        audioCodec == mAudioEncoder &&
        audioBitRate == mAudioBitRate &&
        audioSampleRate == mSampleRate &&
        audioChannels == mAudioChannels) {
        if (videoCodec == VIDEO_ENCODER_H264) {
            RE_LOGI("Force to use AVC baseline profile");
            setParamVideoEncoderProfile(OMX_VIDEO_AVCProfileBaseline);
        }
    }
}

status_t GonkRecorder::checkAudioEncoderCapabilities() {
    clipAudioBitRate();
    clipAudioSampleRate();
    clipNumberOfAudioChannels();
    return OK;
}

void GonkRecorder::clipAudioBitRate() {
    RE_LOGV("clipAudioBitRate: encoder %d", mAudioEncoder);

    int minAudioBitRate =
            mEncoderProfiles->getAudioEncoderParamByName(
                "enc.aud.bps.min", mAudioEncoder);
    if (minAudioBitRate != -1 && mAudioBitRate < minAudioBitRate) {
        RE_LOGW("Intended audio encoding bit rate (%d) is too small"
            " and will be set to (%d)", mAudioBitRate, minAudioBitRate);
        mAudioBitRate = minAudioBitRate;
    }

    int maxAudioBitRate =
            mEncoderProfiles->getAudioEncoderParamByName(
                "enc.aud.bps.max", mAudioEncoder);
    if (maxAudioBitRate != -1 && mAudioBitRate > maxAudioBitRate) {
        RE_LOGW("Intended audio encoding bit rate (%d) is too large"
            " and will be set to (%d)", mAudioBitRate, maxAudioBitRate);
        mAudioBitRate = maxAudioBitRate;
    }
}

void GonkRecorder::clipAudioSampleRate() {
    RE_LOGV("clipAudioSampleRate: encoder %d", mAudioEncoder);

    int minSampleRate =
            mEncoderProfiles->getAudioEncoderParamByName(
                "enc.aud.hz.min", mAudioEncoder);
    if (minSampleRate != -1 && mSampleRate < minSampleRate) {
        RE_LOGW("Intended audio sample rate (%d) is too small"
            " and will be set to (%d)", mSampleRate, minSampleRate);
        mSampleRate = minSampleRate;
    }

    int maxSampleRate =
            mEncoderProfiles->getAudioEncoderParamByName(
                "enc.aud.hz.max", mAudioEncoder);
    if (maxSampleRate != -1 && mSampleRate > maxSampleRate) {
        RE_LOGW("Intended audio sample rate (%d) is too large"
            " and will be set to (%d)", mSampleRate, maxSampleRate);
        mSampleRate = maxSampleRate;
    }
}

void GonkRecorder::clipNumberOfAudioChannels() {
    RE_LOGV("clipNumberOfAudioChannels: encoder %d", mAudioEncoder);

    int minChannels =
            mEncoderProfiles->getAudioEncoderParamByName(
                "enc.aud.ch.min", mAudioEncoder);
    if (minChannels != -1 && mAudioChannels < minChannels) {
        RE_LOGW("Intended number of audio channels (%d) is too small"
            " and will be set to (%d)", mAudioChannels, minChannels);
        mAudioChannels = minChannels;
    }

    int maxChannels =
            mEncoderProfiles->getAudioEncoderParamByName(
                "enc.aud.ch.max", mAudioEncoder);
    if (maxChannels != -1 && mAudioChannels > maxChannels) {
        RE_LOGW("Intended number of audio channels (%d) is too large"
            " and will be set to (%d)", mAudioChannels, maxChannels);
        mAudioChannels = maxChannels;
    }
}

void GonkRecorder::clipVideoFrameHeight() {
    RE_LOGV("clipVideoFrameHeight: encoder %d", mVideoEncoder);
    int minFrameHeight = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.height.min", mVideoEncoder);
    int maxFrameHeight = mEncoderProfiles->getVideoEncoderParamByName(
                        "enc.vid.height.max", mVideoEncoder);
    if (minFrameHeight != -1 && mVideoHeight < minFrameHeight) {
        RE_LOGW("Intended video encoding frame height (%d) is too small"
             " and will be set to (%d)", mVideoHeight, minFrameHeight);
        mVideoHeight = minFrameHeight;
    } else if (maxFrameHeight != -1 && mVideoHeight > maxFrameHeight) {
        RE_LOGW("Intended video encoding frame height (%d) is too large"
             " and will be set to (%d)", mVideoHeight, maxFrameHeight);
        mVideoHeight = maxFrameHeight;
    }
}

// Set up the appropriate MediaSource depending on the chosen option
status_t GonkRecorder::setupMediaSource(
                      sp<MediaSource> *mediaSource) {
    if (mVideoSource == VIDEO_SOURCE_DEFAULT
            || mVideoSource == VIDEO_SOURCE_CAMERA) {
        sp<GonkCameraSource> cameraSource;
        status_t err = setupCameraSource(&cameraSource);
        if (err != OK) {
            return err;
        }
        *mediaSource = cameraSource;
    } else if (mVideoSource == VIDEO_SOURCE_GRALLOC_BUFFER) {
        return BAD_VALUE;
    } else {
        return INVALID_OPERATION;
    }
    return OK;
}

status_t GonkRecorder::setupCameraSource(
        sp<GonkCameraSource> *cameraSource) {
    status_t err = OK;
    if ((err = checkVideoEncoderCapabilities()) != OK) {
        return err;
    }
    Size videoSize;
    videoSize.width = mVideoWidth;
    videoSize.height = mVideoHeight;
    bool useMeta = true;
    char value[PROPERTY_VALUE_MAX];
    if (property_get("debug.camcorder.disablemeta", value, NULL) &&
            atoi(value)) {
      useMeta = false;
    }

    *cameraSource = GonkCameraSource::Create(
                mCameraHw, videoSize, mFrameRate, useMeta);
    if (*cameraSource == NULL) {
        return UNKNOWN_ERROR;
    }

    if ((*cameraSource)->initCheck() != OK) {
        (*cameraSource).clear();
        *cameraSource = NULL;
        return NO_INIT;
    }

    // When frame rate is not set, the actual frame rate will be set to
    // the current frame rate being used.
    if (mFrameRate == -1) {
        int32_t frameRate = 0;
        CHECK ((*cameraSource)->getFormat()->findInt32(
                    kKeyFrameRate, &frameRate));
        RE_LOGI("Frame rate is not explicitly set. Use the current frame "
             "rate (%d fps)", frameRate);
        mFrameRate = frameRate;
    }

    CHECK(mFrameRate != -1);

    mIsMetaDataStoredInVideoBuffers =
        (*cameraSource)->isMetaDataStoredInVideoBuffers();

    return OK;
}

status_t GonkRecorder::setupVideoEncoder(
        sp<MediaSource> cameraSource,
        int32_t videoBitRate,
        sp<MediaSource> *source) {
    source->clear();

    sp<MetaData> enc_meta = new MetaData;
    enc_meta->setInt32(kKeyBitRate, videoBitRate);
    enc_meta->setInt32(kKeyFrameRate, mFrameRate);

    switch (mVideoEncoder) {
        case VIDEO_ENCODER_H263:
            enc_meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_H263);
            break;

        case VIDEO_ENCODER_MPEG_4_SP:
            enc_meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_MPEG4);
            break;

        case VIDEO_ENCODER_H264:
            enc_meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_AVC);
            break;

        default:
            CHECK(!"Should not be here, unsupported video encoding.");
            break;
    }

    sp<MetaData> meta = cameraSource->getFormat();

    int32_t width, height, stride, sliceHeight, colorFormat;
    CHECK(meta->findInt32(kKeyWidth, &width));
    CHECK(meta->findInt32(kKeyHeight, &height));
    CHECK(meta->findInt32(kKeyStride, &stride));
    CHECK(meta->findInt32(kKeySliceHeight, &sliceHeight));
    CHECK(meta->findInt32(kKeyColorFormat, &colorFormat));

    enc_meta->setInt32(kKeyWidth, width);
    enc_meta->setInt32(kKeyHeight, height);
    enc_meta->setInt32(kKeyIFramesInterval, mIFramesIntervalSec);
    enc_meta->setInt32(kKeyStride, stride);
    enc_meta->setInt32(kKeySliceHeight, sliceHeight);
    enc_meta->setInt32(kKeyColorFormat, colorFormat);
    if (mVideoTimeScale > 0) {
        enc_meta->setInt32(kKeyTimeScale, mVideoTimeScale);
    }
    if (mVideoEncoderProfile != -1) {
        enc_meta->setInt32(kKeyVideoProfile, mVideoEncoderProfile);
    }
    if (mVideoEncoderLevel != -1) {
        enc_meta->setInt32(kKeyVideoLevel, mVideoEncoderLevel);
    }

    // OMXClient::connect() always returns OK and abort's fatally if
    // it can't connect.
    OMXClient client;
    // CHECK_EQ causes an abort if the given condition fails.
    CHECK_EQ(client.connect(), (status_t)OK);

    uint32_t encoder_flags = 0;
    if (mIsMetaDataStoredInVideoBuffers) {
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 18
        encoder_flags |= OMXCodec::kStoreMetaDataInVideoBuffers;
#else
        encoder_flags |= OMXCodec::kHardwareCodecsOnly;
        encoder_flags |= OMXCodec::kStoreMetaDataInVideoBuffers;
        encoder_flags |= OMXCodec::kOnlySubmitOneInputBufferAtOneTime;
#endif
    }

    sp<MediaSource> encoder = OMXCodec::Create(
            client.interface(), enc_meta,
            true /* createEncoder */, cameraSource,
            NULL, encoder_flags);
    if (encoder == NULL) {
        RE_LOGW("Failed to create the encoder");
        // When the encoder fails to be created, we need
        // release the camera source due to the camera's lock
        // and unlock mechanism.
        cameraSource->stop();
        return UNKNOWN_ERROR;
    }

    *source = encoder;

    return OK;
}

status_t GonkRecorder::setupAudioEncoder(const sp<MediaWriter>& writer) {
    status_t status = BAD_VALUE;
    if (OK != (status = checkAudioEncoderCapabilities())) {
        return status;
    }

    switch(mAudioEncoder) {
        case AUDIO_ENCODER_AMR_NB:
        case AUDIO_ENCODER_AMR_WB:
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 18
        case AUDIO_ENCODER_AAC:
        case AUDIO_ENCODER_HE_AAC:
        case AUDIO_ENCODER_AAC_ELD:
#endif
            break;

        default:
            RE_LOGE("Unsupported audio encoder: %d", mAudioEncoder);
            return UNKNOWN_ERROR;
    }

    sp<MediaSource> audioEncoder = createAudioSource();
    if (audioEncoder == NULL) {
        return UNKNOWN_ERROR;
    }

    writer->addSource(audioEncoder);
    return OK;
}

status_t GonkRecorder::setupMPEG4Recording(
        int outputFd,
        int32_t videoWidth, int32_t videoHeight,
        int32_t videoBitRate,
        int32_t *totalBitRate,
        sp<MediaWriter> *mediaWriter) {
    mediaWriter->clear();
    *totalBitRate = 0;
    status_t err = OK;
    sp<MediaWriter> writer = new MPEG4Writer(outputFd);

    if (mVideoSource < VIDEO_SOURCE_LIST_END) {

        sp<MediaSource> mediaSource;
        err = setupMediaSource(&mediaSource);
        if (err != OK) {
            return err;
        }

        sp<MediaSource> encoder;
        err = setupVideoEncoder(mediaSource, videoBitRate, &encoder);
        if (err != OK) {
            return err;
        }

        writer->addSource(encoder);
        *totalBitRate += videoBitRate;
    }

    // Audio source is added at the end if it exists.
    // This help make sure that the "recoding" sound is suppressed for
    // camcorder applications in the recorded files.
    if (mAudioSource != AUDIO_SOURCE_CNT) {
        err = setupAudioEncoder(writer);
        if (err != OK) return err;
        *totalBitRate += mAudioBitRate;
    }

    if (mInterleaveDurationUs > 0) {
        reinterpret_cast<MPEG4Writer *>(writer.get())->
            setInterleaveDuration(mInterleaveDurationUs);
    }
    if (mLongitudex10000 > -3600000 && mLatitudex10000 > -3600000) {
        reinterpret_cast<MPEG4Writer *>(writer.get())->
            setGeoData(mLatitudex10000, mLongitudex10000);
    }
    if (mMaxFileDurationUs != 0) {
        writer->setMaxFileDuration(mMaxFileDurationUs);
    }
    if (mMaxFileSizeBytes != 0) {
        writer->setMaxFileSize(mMaxFileSizeBytes);
    }

    mStartTimeOffsetMs = mEncoderProfiles->getStartTimeOffsetMs(mCameraId);
    if (mStartTimeOffsetMs > 0) {
        reinterpret_cast<MPEG4Writer *>(writer.get())->
            setStartTimeOffsetMs(mStartTimeOffsetMs);
    }

    writer->setListener(mListener);
    *mediaWriter = writer;
    return OK;
}

void GonkRecorder::setupMPEG4MetaData(int64_t startTimeUs, int32_t totalBitRate,
        sp<MetaData> *meta) {
    (*meta)->setInt64(kKeyTime, startTimeUs);
    (*meta)->setInt32(kKeyFileType, mOutputFormat);
    (*meta)->setInt32(kKeyBitRate, totalBitRate);
    (*meta)->setInt32(kKey64BitFileOffset, mUse64BitFileOffset);
    if (mMovieTimeScale > 0) {
        (*meta)->setInt32(kKeyTimeScale, mMovieTimeScale);
    }
    if (mTrackEveryTimeDurationUs > 0) {
        (*meta)->setInt64(kKeyTrackTimeStatus, mTrackEveryTimeDurationUs);
    }

    char value[PROPERTY_VALUE_MAX];
    if (property_get("debug.camcorder.rotation", value, 0) > 0 && atoi(value) >= 0) {
        mRotationDegrees = atoi(value);
        RE_LOGI("Setting rotation to %d", mRotationDegrees );
    }

    if (mRotationDegrees != 0) {
        (*meta)->setInt32(kKeyRotation, mRotationDegrees);
    }
}

status_t GonkRecorder::startMPEG4Recording() {
    int32_t totalBitRate;
    status_t err = setupMPEG4Recording(
            mOutputFd, mVideoWidth, mVideoHeight,
            mVideoBitRate, &totalBitRate, &mWriter);
    if (err != OK) {
        return err;
    }

    //systemTime() doesn't give correct time because
    //HAVE_POSIX_CLOCKS is not defined for utils/Timers.cpp
    //so, using clock_gettime directly
#include <time.h>
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    int64_t startTimeUs = int64_t(t.tv_sec)*1000000000LL + t.tv_nsec;
    startTimeUs = startTimeUs / 1000;
    sp<MetaData> meta = new MetaData;
    setupMPEG4MetaData(startTimeUs, totalBitRate, &meta);

    err = mWriter->start(meta.get());
    if (err != OK) {
        return err;
    }

    return OK;
}

status_t GonkRecorder::pause() {
    RE_LOGV("pause");
    if (mWriter == NULL) {
        return UNKNOWN_ERROR;
    }
    mWriter->pause();

    if (mStarted) {
        mStarted = false;
    }


    return OK;
}

status_t GonkRecorder::stop() {
    RE_LOGV("stop");
    status_t err = OK;

    if (mWriter != NULL) {
        err = mWriter->stop();
        mWriter.clear();
    }

    if (mOutputFd >= 0) {
        ::close(mOutputFd);
        mOutputFd = -1;
    }

    if (mStarted) {
        mStarted = false;
    }


    return err;
}

status_t GonkRecorder::close() {
    RE_LOGV("close");
    stop();

    return OK;
}

status_t GonkRecorder::reset() {
    RE_LOGV("reset");
    stop();

    // No audio or video source by default
    mAudioSource = AUDIO_SOURCE_CNT;
    mVideoSource = VIDEO_SOURCE_LIST_END;

    // Default parameters
    mOutputFormat  = OUTPUT_FORMAT_THREE_GPP;
    mAudioEncoder  = AUDIO_ENCODER_AMR_NB;
    mVideoEncoder  = VIDEO_ENCODER_H263;
    mVideoWidth    = 176;
    mVideoHeight   = 144;
    mFrameRate     = -1;
    mVideoBitRate  = 192000;
    mSampleRate    = 8000;
    mAudioChannels = 1;
    mAudioBitRate  = 12200;
    mInterleaveDurationUs = 0;
    mIFramesIntervalSec = 1;
    mAudioSourceNode = 0;
    mUse64BitFileOffset = false;
    mMovieTimeScale  = -1;
    mAudioTimeScale  = -1;
    mVideoTimeScale  = -1;
    mCameraId        = 0;
    mStartTimeOffsetMs = -1;
    mVideoEncoderProfile = -1;
    mVideoEncoderLevel   = -1;
    mMaxFileDurationUs = 0;
    mMaxFileSizeBytes = 0;
    mTrackEveryTimeDurationUs = 0;
    mIsMetaDataStoredInVideoBuffers = false;
    mEncoderProfiles = MediaProfiles::getInstance();
    mRotationDegrees = 0;
    mLatitudex10000 = -3600000;
    mLongitudex10000 = -3600000;

    mOutputFd = -1;
    mCameraHw.clear();
    //TODO: May need to register a listener eventually
    //if someone is interested in recorder events for now
    //default to no listener registered
    mListener = NULL;

    return OK;
}

status_t GonkRecorder::getMaxAmplitude(int *max) {
    RE_LOGV("getMaxAmplitude");

    if (max == NULL) {
        RE_LOGE("Null pointer argument");
        return BAD_VALUE;
    }

    if (mAudioSourceNode != 0) {
        *max = mAudioSourceNode->getMaxAmplitude();
    } else {
        *max = 0;
    }

    return OK;
}

status_t GonkRecorder::dump(
        int fd, const Vector<String16>& args) const {
    RE_LOGV("dump");
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    if (mWriter != 0) {
        mWriter->dump(fd, args);
    } else {
        snprintf(buffer, SIZE, "   No file writer\n");
        result.append(buffer);
    }
    snprintf(buffer, SIZE, "   Recorder: %p\n", this);
    snprintf(buffer, SIZE, "   Output file (fd %d):\n", mOutputFd);
    result.append(buffer);
    snprintf(buffer, SIZE, "     File format: %d\n", mOutputFormat);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Max file size (bytes): %lld\n", mMaxFileSizeBytes);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Max file duration (us): %lld\n", mMaxFileDurationUs);
    result.append(buffer);
    snprintf(buffer, SIZE, "     File offset length (bits): %d\n", mUse64BitFileOffset? 64: 32);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Interleave duration (us): %d\n", mInterleaveDurationUs);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Progress notification: %lld us\n", mTrackEveryTimeDurationUs);
    result.append(buffer);
    snprintf(buffer, SIZE, "   Audio\n");
    result.append(buffer);
    snprintf(buffer, SIZE, "     Source: %d\n", mAudioSource);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Encoder: %d\n", mAudioEncoder);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Bit rate (bps): %d\n", mAudioBitRate);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Sampling rate (hz): %d\n", mSampleRate);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Number of channels: %d\n", mAudioChannels);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Max amplitude: %d\n", mAudioSourceNode == 0? 0: mAudioSourceNode->getMaxAmplitude());
    result.append(buffer);
    snprintf(buffer, SIZE, "   Video\n");
    result.append(buffer);
    snprintf(buffer, SIZE, "     Source: %d\n", mVideoSource);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Camera Id: %d\n", mCameraId);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Camera object address: %p\n", mCameraHw.get());
    result.append(buffer);
    snprintf(buffer, SIZE, "     Start time offset (ms): %d\n", mStartTimeOffsetMs);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Encoder: %d\n", mVideoEncoder);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Encoder profile: %d\n", mVideoEncoderProfile);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Encoder level: %d\n", mVideoEncoderLevel);
    result.append(buffer);
    snprintf(buffer, SIZE, "     I frames interval (s): %d\n", mIFramesIntervalSec);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Frame size (pixels): %dx%d\n", mVideoWidth, mVideoHeight);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Frame rate (fps): %d\n", mFrameRate);
    result.append(buffer);
    snprintf(buffer, SIZE, "     Bit rate (bps): %d\n", mVideoBitRate);
    result.append(buffer);
    ::write(fd, result.string(), result.size());
    return OK;
}

status_t GonkRecorder::setCamera(const sp<GonkCameraHardware>& aCameraHw) {
  mCameraHw = aCameraHw;
  return OK;
}

}  // namespace android
