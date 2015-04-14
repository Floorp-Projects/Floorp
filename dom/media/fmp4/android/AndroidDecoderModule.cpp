/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidDecoderModule.h"
#include "AndroidBridge.h"
#include "GLBlitHelper.h"
#include "GLContext.h"
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "GLImages.h"
#include "GLLibraryEGL.h"

#include "MediaData.h"
#include "MediaInfo.h"

#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "nsPromiseFlatString.h"

#include <jni.h>
#include <string.h>

using namespace mozilla;
using namespace mozilla::gl;
using namespace mozilla::widget::sdk;

namespace mozilla {

static MediaCodec::LocalRef CreateDecoder(const nsACString& aMimeType)
{
  MediaCodec::LocalRef codec;
  NS_ENSURE_SUCCESS(MediaCodec::CreateDecoderByType(PromiseFlatCString(aMimeType).get(), &codec), nullptr);
  return codec;
}

class VideoDataDecoder : public MediaCodecDataDecoder {
public:
  VideoDataDecoder(const VideoInfo& aConfig,
                   MediaFormat::Param aFormat, MediaDataDecoderCallback* aCallback,
                   layers::ImageContainer* aImageContainer)
    : MediaCodecDataDecoder(MediaData::Type::VIDEO_DATA, aConfig.mMimeType, aFormat, aCallback)
    , mImageContainer(aImageContainer)
    , mConfig(aConfig)
  {

  }

  nsresult Init() override {
    mSurfaceTexture = AndroidSurfaceTexture::Create();
    if (!mSurfaceTexture) {
      NS_WARNING("Failed to create SurfaceTexture for video decode\n");
      return NS_ERROR_FAILURE;
    }

    return InitDecoder(mSurfaceTexture->JavaSurface());
  }

  void Cleanup() override {
    mGLContext = nullptr;
  }

  virtual nsresult Input(MediaRawData* aSample) override {
    return MediaCodecDataDecoder::Input(aSample);
  }

  bool WantCopy() {
    // Allocating a texture is incredibly slow on PowerVR
    return mGLContext->Vendor() != GLVendor::Imagination;
  }

  EGLImage CopySurface(layers::Image* img) {
    mGLContext->MakeCurrent();

    GLuint tex = CreateTextureForOffscreen(mGLContext, mGLContext->GetGLFormats(), img->GetSize());

    GLBlitHelper helper(mGLContext);
    if (!helper.BlitImageToTexture(img, img->GetSize(), tex, LOCAL_GL_TEXTURE_2D)) {
      mGLContext->fDeleteTextures(1, &tex);
      return nullptr;
    }

    EGLint attribs[] = {
      LOCAL_EGL_IMAGE_PRESERVED_KHR, LOCAL_EGL_TRUE,
      LOCAL_EGL_NONE, LOCAL_EGL_NONE
    };

    EGLContext eglContext = static_cast<GLContextEGL*>(mGLContext.get())->GetEGLContext();
    EGLImage eglImage = sEGLLibrary.fCreateImage(EGL_DISPLAY(), eglContext,
                                                 LOCAL_EGL_GL_TEXTURE_2D_KHR,
                                                 (EGLClientBuffer)tex, attribs);
    mGLContext->fDeleteTextures(1, &tex);

    return eglImage;
  }

  virtual nsresult PostOutput(BufferInfo::Param aInfo, MediaFormat::Param aFormat, Microseconds aDuration) override {
    if (!EnsureGLContext()) {
      return NS_ERROR_FAILURE;
    }

    nsRefPtr<layers::Image> img = mImageContainer->CreateImage(ImageFormat::SURFACE_TEXTURE);
    layers::SurfaceTextureImage::Data data;
    data.mSurfTex = mSurfaceTexture.get();
    data.mSize = mConfig.mDisplay;
    data.mOriginPos = gl::OriginPos::BottomLeft;

    layers::SurfaceTextureImage* stImg = static_cast<layers::SurfaceTextureImage*>(img.get());
    stImg->SetData(data);

    if (WantCopy()) {
      EGLImage eglImage = CopySurface(img);
      if (!eglImage) {
        return NS_ERROR_FAILURE;
      }

      EGLSync eglSync = nullptr;
      if (sEGLLibrary.IsExtensionSupported(GLLibraryEGL::KHR_fence_sync) &&
          mGLContext->IsExtensionSupported(GLContext::OES_EGL_sync))
      {
        MOZ_ASSERT(mGLContext->IsCurrent());
        eglSync = sEGLLibrary.fCreateSync(EGL_DISPLAY(),
                                          LOCAL_EGL_SYNC_FENCE,
                                          nullptr);
        MOZ_ASSERT(eglSync);
        mGLContext->fFlush();
      } else {
        NS_WARNING("No EGL fence support detected, rendering artifacts may occur!");
      }

      img = mImageContainer->CreateImage(ImageFormat::EGLIMAGE);
      layers::EGLImageImage::Data data;
      data.mImage = eglImage;
      data.mSync = eglSync;
      data.mOwns = true;
      data.mSize = mConfig.mDisplay;
      data.mOriginPos = gl::OriginPos::BottomLeft;

      layers::EGLImageImage* typedImg = static_cast<layers::EGLImageImage*>(img.get());
      typedImg->SetData(data);
    }

    nsresult rv;
    int32_t flags;
    NS_ENSURE_SUCCESS(rv = aInfo->Flags(&flags), rv);

    bool isSync = !!(flags & MediaCodec::BUFFER_FLAG_SYNC_FRAME);

    int32_t offset;
    NS_ENSURE_SUCCESS(rv = aInfo->Offset(&offset), rv);

    int64_t presentationTimeUs;
    NS_ENSURE_SUCCESS(rv = aInfo->PresentationTimeUs(&presentationTimeUs), rv);

    nsRefPtr<VideoData> v =
      VideoData::CreateFromImage(mConfig,
                                 mImageContainer,
                                 offset,
                                 presentationTimeUs,
                                 aDuration,
                                 img,
                                 isSync,
                                 presentationTimeUs,
                                 gfx::IntRect(0, 0,
                                              mConfig.mDisplay.width,
                                              mConfig.mDisplay.height));
    mCallback->Output(v);
    return NS_OK;
  }

protected:
  bool EnsureGLContext() {
    if (mGLContext) {
      return true;
    }

    mGLContext = GLContextProvider::CreateHeadless(false);
    return mGLContext;
  }

  layers::ImageContainer* mImageContainer;
  const VideoInfo& mConfig;
  RefPtr<AndroidSurfaceTexture> mSurfaceTexture;
  nsRefPtr<GLContext> mGLContext;
};

class AudioDataDecoder : public MediaCodecDataDecoder {
private:
  uint8_t csd0[2];

public:
  AudioDataDecoder(const AudioInfo& aConfig, MediaFormat::Param aFormat, MediaDataDecoderCallback* aCallback)
    : MediaCodecDataDecoder(MediaData::Type::AUDIO_DATA, aConfig.mMimeType, aFormat, aCallback)
  {
    JNIEnv* env = GetJNIForThread();

    jni::Object::LocalRef buffer(env);
    NS_ENSURE_SUCCESS_VOID(aFormat->GetByteBuffer(NS_LITERAL_STRING("csd-0"), &buffer));

    if (!buffer && aConfig.mCodecSpecificConfig->Length() >= 2) {
      csd0[0] = (*aConfig.mCodecSpecificConfig)[0];
      csd0[1] = (*aConfig.mCodecSpecificConfig)[1];

      buffer = jni::Object::LocalRef::Adopt(env, env->NewDirectByteBuffer(csd0, 2));
      NS_ENSURE_SUCCESS_VOID(aFormat->SetByteBuffer(NS_LITERAL_STRING("csd-0"), buffer));
    }
  }

  nsresult Output(BufferInfo::Param aInfo, void* aBuffer, MediaFormat::Param aFormat, Microseconds aDuration) {
    // The output on Android is always 16-bit signed

    nsresult rv;
    int32_t numChannels;
    NS_ENSURE_SUCCESS(rv =
        aFormat->GetInteger(NS_LITERAL_STRING("channel-count"), &numChannels), rv);

    int32_t sampleRate;
    NS_ENSURE_SUCCESS(rv =
        aFormat->GetInteger(NS_LITERAL_STRING("sample-rate"), &sampleRate), rv);

    int32_t size;
    NS_ENSURE_SUCCESS(rv = aInfo->Size(&size), rv);

    const int32_t numFrames = (size / numChannels) / 2;
    AudioDataValue* audio = new AudioDataValue[size];
    PodCopy(audio, static_cast<AudioDataValue*>(aBuffer), size);

    int32_t offset;
    NS_ENSURE_SUCCESS(rv = aInfo->Offset(&offset), rv);

    int64_t presentationTimeUs;
    NS_ENSURE_SUCCESS(rv = aInfo->PresentationTimeUs(&presentationTimeUs), rv);

    nsRefPtr<AudioData> data = new AudioData(offset, presentationTimeUs,
                                             aDuration,
                                             numFrames,
                                             audio,
                                             numChannels,
                                             sampleRate);
    mCallback->Output(data);
    return NS_OK;
  }
};


bool AndroidDecoderModule::SupportsMimeType(const nsACString& aMimeType)
{
  if (aMimeType.EqualsLiteral("video/mp4") ||
      aMimeType.EqualsLiteral("video/avc")) {
    return true;
  }
  return static_cast<bool>(mozilla::CreateDecoder(aMimeType));
}

already_AddRefed<MediaDataDecoder>
AndroidDecoderModule::CreateVideoDecoder(
                                const VideoInfo& aConfig,
                                layers::LayersBackend aLayersBackend,
                                layers::ImageContainer* aImageContainer,
                                FlushableMediaTaskQueue* aVideoTaskQueue,
                                MediaDataDecoderCallback* aCallback)
{
  MediaFormat::LocalRef format;

  NS_ENSURE_SUCCESS(MediaFormat::CreateVideoFormat(
      aConfig.mMimeType,
      aConfig.mDisplay.width,
      aConfig.mDisplay.height,
      &format), nullptr);

  nsRefPtr<MediaDataDecoder> decoder =
    new VideoDataDecoder(aConfig, format, aCallback, aImageContainer);

  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
AndroidDecoderModule::CreateAudioDecoder(const AudioInfo& aConfig,
                                         FlushableMediaTaskQueue* aAudioTaskQueue,
                                         MediaDataDecoderCallback* aCallback)
{
  MOZ_ASSERT(aConfig.mBitDepth == 16, "We only handle 16-bit audio!");

  MediaFormat::LocalRef format;

  NS_ENSURE_SUCCESS(MediaFormat::CreateAudioFormat(
      aConfig.mMimeType,
      aConfig.mBitDepth,
      aConfig.mChannels,
      &format), nullptr);

  nsRefPtr<MediaDataDecoder> decoder =
    new AudioDataDecoder(aConfig, format, aCallback);

  return decoder.forget();

}

PlatformDecoderModule::ConversionRequired
AndroidDecoderModule::DecoderNeedsConversion(const TrackInfo& aConfig) const
{
  if (aConfig.IsVideo()) {
    return kNeedAnnexB;
  } else {
    return kNeedNone;
  }
}

MediaCodecDataDecoder::MediaCodecDataDecoder(MediaData::Type aType,
                                             const nsACString& aMimeType,
                                             MediaFormat::Param aFormat,
                                             MediaDataDecoderCallback* aCallback)
  : mType(aType)
  , mMimeType(aMimeType)
  , mFormat(aFormat)
  , mCallback(aCallback)
  , mInputBuffers(nullptr)
  , mOutputBuffers(nullptr)
  , mMonitor("MediaCodecDataDecoder::mMonitor")
  , mFlushing(false)
  , mDraining(false)
  , mStopping(false)
{

}

MediaCodecDataDecoder::~MediaCodecDataDecoder()
{
  Shutdown();
}

nsresult MediaCodecDataDecoder::Init()
{
  return InitDecoder(nullptr);
}

nsresult MediaCodecDataDecoder::InitDecoder(Surface::Param aSurface)
{
  mDecoder = CreateDecoder(mMimeType);
  if (!mDecoder) {
    mCallback->Error();
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  NS_ENSURE_SUCCESS(rv = mDecoder->Configure(mFormat, aSurface, nullptr, 0), rv);
  NS_ENSURE_SUCCESS(rv = mDecoder->Start(), rv);

  NS_ENSURE_SUCCESS(rv = ResetInputBuffers(), rv);
  NS_ENSURE_SUCCESS(rv = ResetOutputBuffers(), rv);

  NS_NewNamedThread("MC Decoder", getter_AddRefs(mThread),
                    NS_NewRunnableMethod(this, &MediaCodecDataDecoder::DecoderLoop));

  return NS_OK;
}

// This is in usec, so that's 10ms
#define DECODER_TIMEOUT 10000

#define HANDLE_DECODER_ERROR() \
  if (NS_FAILED(res)) { \
    NS_WARNING("exiting decoder loop due to exception"); \
    mCallback->Error(); \
    break; \
  }

nsresult MediaCodecDataDecoder::GetInputBuffer(JNIEnv* env, int index, jni::Object::LocalRef* buffer)
{
  bool retried = false;
  while (!*buffer) {
    *buffer = jni::Object::LocalRef::Adopt(env->GetObjectArrayElement(mInputBuffers.Get(), index));
    if (!*buffer) {
      if (!retried) {
        // Reset the input buffers and then try again
        nsresult res = ResetInputBuffers();
        if (NS_FAILED(res)) {
          return res;
        }
        retried = true;
      } else {
        // We already tried resetting the input buffers, return an error
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

void MediaCodecDataDecoder::DecoderLoop()
{
  bool outputDone = false;

  bool draining = false;
  bool waitingEOF = false;

  AutoLocalJNIFrame frame(GetJNIForThread(), 1);
  nsRefPtr<MediaRawData> sample;

  MediaFormat::LocalRef outputFormat(frame.GetEnv());
  nsresult res;

  for (;;) {
    {
      MonitorAutoLock lock(mMonitor);
      while (!mStopping && !mDraining && !mFlushing && mQueue.empty()) {
        if (mQueue.empty()) {
          // We could be waiting here forever if we don't signal that we need more input
          mCallback->InputExhausted();
        }
        lock.Wait();
      }

      if (mStopping) {
        // Get out of the loop. This is the only exit point.
        break;
      }

      if (mFlushing) {
        mDecoder->Flush();
        ClearQueue();
        mFlushing =  false;
        lock.Notify();
        continue;
      }

      if (mDraining && !sample && !waitingEOF) {
        draining = true;
      }

      // We're not stopping or draining, so try to get a sample
      if (!mQueue.empty()) {
        sample = mQueue.front();
      }
    }

    if (draining && !waitingEOF) {
      MOZ_ASSERT(!sample, "Shouldn't have a sample when pushing EOF frame");

      int32_t inputIndex;
      res = mDecoder->DequeueInputBuffer(DECODER_TIMEOUT, &inputIndex);
      HANDLE_DECODER_ERROR();

      if (inputIndex >= 0) {
        res = mDecoder->QueueInputBuffer(inputIndex, 0, 0, 0, MediaCodec::BUFFER_FLAG_END_OF_STREAM);
        HANDLE_DECODER_ERROR();

        waitingEOF = true;
      }
    }

    if (sample) {
      // We have a sample, try to feed it to the decoder
      int inputIndex;
      res = mDecoder->DequeueInputBuffer(DECODER_TIMEOUT, &inputIndex);
      HANDLE_DECODER_ERROR();

      if (inputIndex >= 0) {
        jni::Object::LocalRef buffer(frame.GetEnv());
        res = GetInputBuffer(frame.GetEnv(), inputIndex, &buffer);
        HANDLE_DECODER_ERROR();

        void* directBuffer = frame.GetEnv()->GetDirectBufferAddress(buffer.Get());

        MOZ_ASSERT(frame.GetEnv()->GetDirectBufferCapacity(buffer.Get()) >= sample->mSize,
          "Decoder buffer is not large enough for sample");

        {
          // We're feeding this to the decoder, so remove it from the queue
          MonitorAutoLock lock(mMonitor);
          mQueue.pop();
        }

        PodCopy((uint8_t*)directBuffer, sample->mData, sample->mSize);

        res = mDecoder->QueueInputBuffer(inputIndex, 0, sample->mSize,
                                         sample->mTime, 0);
        HANDLE_DECODER_ERROR();

        mDurations.push(sample->mDuration);
        sample = nullptr;
        outputDone = false;
      }
    }

    if (!outputDone) {
      BufferInfo::LocalRef bufferInfo;
      res = BufferInfo::New(&bufferInfo);
      HANDLE_DECODER_ERROR();

      int32_t outputStatus;
      res = mDecoder->DequeueOutputBuffer(bufferInfo, DECODER_TIMEOUT, &outputStatus);
      HANDLE_DECODER_ERROR();

      if (outputStatus == MediaCodec::INFO_TRY_AGAIN_LATER) {
        // We might want to call mCallback->InputExhausted() here, but there seems to be
        // some possible bad interactions here with the threading
      } else if (outputStatus == MediaCodec::INFO_OUTPUT_BUFFERS_CHANGED) {
        res = ResetOutputBuffers();
        HANDLE_DECODER_ERROR();
      } else if (outputStatus == MediaCodec::INFO_OUTPUT_FORMAT_CHANGED) {
        res = mDecoder->GetOutputFormat(ReturnTo(&outputFormat));
        HANDLE_DECODER_ERROR();
      } else if (outputStatus < 0) {
        NS_WARNING("unknown error from decoder!");
        mCallback->Error();

        // Don't break here just in case it's recoverable. If it's not, others stuff will fail later and
        // we'll bail out.
      } else {
        int32_t flags;
        res = bufferInfo->Flags(&flags);
        HANDLE_DECODER_ERROR();

        // We have a valid buffer index >= 0 here
        if (flags & MediaCodec::BUFFER_FLAG_END_OF_STREAM) {
          if (draining) {
            draining = false;
            waitingEOF = false;

            mMonitor.Lock();
            mDraining = false;
            mMonitor.Notify();
            mMonitor.Unlock();

            mCallback->DrainComplete();
          }

          mDecoder->ReleaseOutputBuffer(outputStatus, false);
          outputDone = true;

          // We only queue empty EOF frames, so we're done for now
          continue;
        }

        MOZ_ASSERT(!mDurations.empty(), "Should have had a duration queued");

        Microseconds duration = 0;
        if (!mDurations.empty()) {
          duration = mDurations.front();
          mDurations.pop();
        }

        auto buffer = jni::Object::LocalRef::Adopt(
            frame.GetEnv()->GetObjectArrayElement(mOutputBuffers.Get(), outputStatus));
        if (buffer) {
          // The buffer will be null on Android L if we are decoding to a Surface
          void* directBuffer = frame.GetEnv()->GetDirectBufferAddress(buffer.Get());
          Output(bufferInfo, directBuffer, outputFormat, duration);
        }

        // The Surface will be updated at this point (for video)
        mDecoder->ReleaseOutputBuffer(outputStatus, true);

        PostOutput(bufferInfo, outputFormat, duration);
      }
    }
  }

  Cleanup();

  // We're done
  MonitorAutoLock lock(mMonitor);
  mStopping = false;
  mMonitor.Notify();
}

void MediaCodecDataDecoder::ClearQueue()
{
  mMonitor.AssertCurrentThreadOwns();
  while (!mQueue.empty()) {
    mQueue.pop();
  }
  while (!mDurations.empty()) {
    mDurations.pop();
  }
}

nsresult MediaCodecDataDecoder::Input(MediaRawData* aSample) {
  MonitorAutoLock lock(mMonitor);
  mQueue.push(aSample);
  lock.NotifyAll();

  return NS_OK;
}

nsresult MediaCodecDataDecoder::ResetInputBuffers()
{
  return mDecoder->GetInputBuffers(ReturnTo(&mInputBuffers));
}

nsresult MediaCodecDataDecoder::ResetOutputBuffers()
{
  return mDecoder->GetOutputBuffers(ReturnTo(&mOutputBuffers));
}

nsresult MediaCodecDataDecoder::Flush() {
  MonitorAutoLock lock(mMonitor);
  mFlushing = true;
  lock.Notify();

  while (mFlushing) {
    lock.Wait();
  }

  return NS_OK;
}

nsresult MediaCodecDataDecoder::Drain() {
  MonitorAutoLock lock(mMonitor);
  if (mDraining) {
    return NS_OK;
  }

  mDraining = true;
  lock.Notify();

  return NS_OK;
}


nsresult MediaCodecDataDecoder::Shutdown() {
  MonitorAutoLock lock(mMonitor);

  if (!mThread || mStopping) {
    // Already shutdown or in the process of doing so
    return NS_OK;
  }

  mStopping = true;
  lock.Notify();

  while (mStopping) {
    lock.Wait();
  }

  mThread->Shutdown();
  mThread = nullptr;

  mDecoder->Stop();
  mDecoder->Release();

  return NS_OK;
}

} // mozilla
