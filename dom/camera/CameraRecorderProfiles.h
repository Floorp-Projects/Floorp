/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_CAMERA_RECORDER_PROFILES_H
#define DOM_CAMERA_CAMERA_RECORDER_PROFILES_H

#include "nsISupportsImpl.h"
#include "nsMimeTypes.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "jsapi.h"
#include "CameraCommon.h"

namespace mozilla {

class CameraControlImpl;

class RecorderVideoProfile
{
public:
  RecorderVideoProfile(uint32_t aCameraId, uint32_t aQualityIndex);
  virtual ~RecorderVideoProfile();

  int GetBitrate() const    { return mBitrate; }
  int GetFramerate() const  { return mFramerate; }
  int GetWidth() const      { return mWidth; }
  int GetHeight() const     { return mHeight; }

  enum Codec {
    H263,
    H264,
    MPEG4SP,
    UNKNOWN
  };
  Codec GetCodec() const    { return mCodec; }
  const char* GetCodecName() const
  {
    switch (mCodec) {
      case H263:    return "h263";
      case H264:    return "h264";
      case MPEG4SP: return "mpeg4sp";
      default:      return nullptr;
    }
  }

  // Get a representation of this video profile that can be returned
  // to JS, possibly as a child member of another object.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_INVALID_ARG if 'aObject' is null;
  //  - NS_ERROR_OUT_OF_MEMORY if a new object could not be allocated;
  //  - NS_ERROR_FAILURE if construction of the JS object fails.
  nsresult GetJsObject(JSContext* aCx, JSObject** aObject);

protected:
  uint32_t mCameraId;
  uint32_t mQualityIndex;
  Codec mCodec;
  int mBitrate;
  int mFramerate;
  int mWidth;
  int mHeight;
};

class RecorderAudioProfile
{
public:
  RecorderAudioProfile(uint32_t aCameraId, uint32_t aQualityIndex);
  virtual ~RecorderAudioProfile();

  int GetBitrate() const    { return mBitrate; }
  int GetSamplerate() const { return mSamplerate; }
  int GetChannels() const   { return mChannels; }

  enum Codec {
    AMRNB,
    AMRWB,
    AAC,
    UNKNOWN
  };

  Codec GetCodec() const    { return mCodec; }
  const char* GetCodecName() const
  {
    switch (mCodec) {
      case AMRNB: return "amrnb";
      case AMRWB: return "amrwb";
      case AAC:   return "aac";
      default:    return nullptr;
    }
  }

  // Get a representation of this audio profile that can be returned
  // to JS, possibly as a child member of another object.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_INVALID_ARG if 'aObject' is null;
  //  - NS_ERROR_OUT_OF_MEMORY if a new object could not be allocated;
  //  - NS_ERROR_FAILURE if construction of the JS object fails.
  nsresult GetJsObject(JSContext* aCx, JSObject** aObject);

protected:
  uint32_t mCameraId;
  uint32_t mQualityIndex;
  Codec mCodec;
  int mBitrate;
  int mSamplerate;
  int mChannels;
};

class RecorderProfile
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RecorderProfile)

  RecorderProfile(uint32_t aCameraId, uint32_t aQualityIndex);

  virtual const RecorderVideoProfile* GetVideoProfile() const = 0;
  virtual const RecorderAudioProfile* GetAudioProfile() const = 0;
  const char* GetName() const { return mName; }

  enum FileFormat {
    THREE_GPP,
    MPEG4,
    UNKNOWN
  };
  FileFormat GetFileFormat() const { return mFileFormat; }
  const char* GetFileFormatName() const
  {
    switch (mFileFormat) {
      case THREE_GPP: return "3gp";
      case MPEG4:     return "mp4";
      default:        return nullptr;
    }
  }
  const char* GetFileMimeType() const
  {
    switch (mFileFormat) {
      case THREE_GPP: return VIDEO_3GPP;
      case MPEG4:     return VIDEO_MP4;
      default:        return nullptr;
    }
  }

  // Get a representation of this recorder profile that can be returned
  // to JS, possibly as a child member of another object.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_INVALID_ARG if 'aObject' is null;
  //  - NS_ERROR_OUT_OF_MEMORY if a new object could not be allocated;
  //  - NS_ERROR_FAILURE if construction of the JS object fails.
  virtual nsresult GetJsObject(JSContext* aCx, JSObject** aObject) = 0;

protected:
  virtual ~RecorderProfile();

  uint32_t mCameraId;
  uint32_t mQualityIndex;
  const char* mName;
  FileFormat mFileFormat;
};

template <class Audio, class Video>
class RecorderProfileBase : public RecorderProfile
{
public:
  RecorderProfileBase(uint32_t aCameraId, uint32_t aQualityIndex)
    : RecorderProfile(aCameraId, aQualityIndex)
    , mVideo(aCameraId, aQualityIndex)
    , mAudio(aCameraId, aQualityIndex)
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  virtual ~RecorderProfileBase()
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  const RecorderVideoProfile* GetVideoProfile() const { return &mVideo; }
  const RecorderAudioProfile* GetAudioProfile() const { return &mAudio; }

  // Get a representation of this recorder profile that can be returned
  // to JS, possibly as a child member of another object.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_INVALID_ARG if 'aObject' is null;
  //  - NS_ERROR_OUT_OF_MEMORY if a new object could not be allocated;
  //  - NS_ERROR_NOT_AVAILABLE if the profile has no file format name;
  //  - NS_ERROR_FAILURE if construction of the JS object fails.
  nsresult
  GetJsObject(JSContext* aCx, JSObject** aObject)
  {
    NS_ENSURE_TRUE(aObject, NS_ERROR_INVALID_ARG);

    const char* format = GetFileFormatName();
    if (!format) {
      // the profile must have a file format
      return NS_ERROR_NOT_AVAILABLE;
    }

    JS::Rooted<JSObject*> o(aCx, JS_NewObject(aCx, nullptr, JS::NullPtr(), JS::NullPtr()));
    if (!o) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    JS::Rooted<JSString*> s(aCx, JS_NewStringCopyZ(aCx, format));
    JS::Rooted<JS::Value> v(aCx, STRING_TO_JSVAL(s));
    if (!JS_SetProperty(aCx, o, "format", v)) {
      return NS_ERROR_FAILURE;
    }

    JS::Rooted<JSObject*> video(aCx);
    nsresult rv = mVideo.GetJsObject(aCx, video.address());
    NS_ENSURE_SUCCESS(rv, rv);
    v = OBJECT_TO_JSVAL(video);
    if (!JS_SetProperty(aCx, o, "video", v)) {
      return NS_ERROR_FAILURE;
    }

    JS::Rooted<JSObject*> audio(aCx);
    rv = mAudio.GetJsObject(aCx, audio.address());
    NS_ENSURE_SUCCESS(rv, rv);
    v = OBJECT_TO_JSVAL(audio);
    if (!JS_SetProperty(aCx, o, "audio", v)) {
      return NS_ERROR_FAILURE;
    }

    *aObject = o;
    return NS_OK;
  }

protected:
  Video mVideo;
  Audio mAudio;
};

class RecorderProfileManager
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RecorderProfileManager)

  virtual bool IsSupported(uint32_t aQualityIndex) const { return true; }
  virtual already_AddRefed<RecorderProfile> Get(uint32_t aQualityIndex) const = 0;

  uint32_t GetMaxQualityIndex() const { return mMaxQualityIndex; }

  // Get a representation of all supported recorder profiles that can be
  // returned to JS.
  //
  // Return values:
  //  - NS_OK on success;
  //  - NS_ERROR_INVALID_ARG if 'aObject' is null;
  //  - NS_ERROR_OUT_OF_MEMORY if a new object could not be allocated;
  //  - NS_ERROR_NOT_AVAILABLE if the profile has no file format name;
  //  - NS_ERROR_FAILURE if construction of the JS object fails.
  nsresult GetJsObject(JSContext* aCx, JSObject** aObject) const;

protected:
  explicit RecorderProfileManager(uint32_t aCameraId);
  virtual ~RecorderProfileManager();

  uint32_t mCameraId;
  uint32_t mMaxQualityIndex;
};

} // namespace mozilla

#endif // DOM_CAMERA_CAMERA_RECORDER_PROFILES_H
