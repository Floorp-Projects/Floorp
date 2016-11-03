/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasCaptureMediaStream.h"
#include "DOMMediaStream.h"
#include "gfxPlatform.h"
#include "ImageContainer.h"
#include "MediaStreamGraph.h"
#include "mozilla/dom/CanvasCaptureMediaStreamBinding.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Atomics.h"
#include "nsContentUtils.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

class OutputStreamDriver::StreamListener : public MediaStreamListener
{
public:
  explicit StreamListener(OutputStreamDriver* aDriver,
                          TrackID aTrackId,
                          PrincipalHandle aPrincipalHandle,
                          SourceMediaStream* aSourceStream)
    : mEnded(false)
    , mSourceStream(aSourceStream)
    , mTrackId(aTrackId)
    , mPrincipalHandle(aPrincipalHandle)
    , mMutex("CanvasCaptureMediaStream OutputStreamDriver::StreamListener")
    , mImage(nullptr)
  {
    MOZ_ASSERT(mSourceStream);
  }

  void EndStream() {
    mEnded = true;
  }

  void SetImage(const RefPtr<layers::Image>& aImage)
  {
    MutexAutoLock lock(mMutex);
    mImage = aImage;
  }

  void NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime) override
  {
    // Called on the MediaStreamGraph thread.
    StreamTime delta = aDesiredTime - mSourceStream->GetEndOfAppendedData(mTrackId);
    if (delta > 0) {
      MutexAutoLock lock(mMutex);
      MOZ_ASSERT(mSourceStream);

      RefPtr<Image> image = mImage;
      IntSize size = image ? image->GetSize() : IntSize(0, 0);
      VideoSegment segment;
      segment.AppendFrame(image.forget(), delta, size, mPrincipalHandle);

      mSourceStream->AppendToTrack(mTrackId, &segment);
    }

    if (mEnded) {
      mSourceStream->EndAllTrackAndFinish();
    }
  }

protected:
  ~StreamListener() { }

private:
  Atomic<bool> mEnded;
  const RefPtr<SourceMediaStream> mSourceStream;
  const TrackID mTrackId;
  const PrincipalHandle mPrincipalHandle;

  Mutex mMutex;
  // The below members are protected by mMutex.
  RefPtr<layers::Image> mImage;
};

OutputStreamDriver::OutputStreamDriver(SourceMediaStream* aSourceStream,
                                       const TrackID& aTrackId,
                                       const PrincipalHandle& aPrincipalHandle)
  : FrameCaptureListener()
  , mSourceStream(aSourceStream)
  , mStreamListener(new StreamListener(this, aTrackId, aPrincipalHandle,
                                       aSourceStream))
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mSourceStream);
  mSourceStream->AddListener(mStreamListener);
  mSourceStream->AddTrack(aTrackId, 0, new VideoSegment());
  mSourceStream->AdvanceKnownTracksTime(STREAM_TIME_MAX);
  mSourceStream->SetPullEnabled(true);

  // All CanvasCaptureMediaStreams shall at least get one frame.
  mFrameCaptureRequested = true;
}

OutputStreamDriver::~OutputStreamDriver()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mStreamListener) {
    // MediaStreamGraph will keep the listener alive until it can finish the
    // stream on the next NotifyPull().
    mStreamListener->EndStream();
  }
}

void
OutputStreamDriver::SetImage(const RefPtr<layers::Image>& aImage)
{
  if (mStreamListener) {
    mStreamListener->SetImage(aImage);
  }
}

// ----------------------------------------------------------------------

class TimerDriver : public OutputStreamDriver
{
public:
  explicit TimerDriver(SourceMediaStream* aSourceStream,
                       const double& aFPS,
                       const TrackID& aTrackId,
                       const PrincipalHandle& aPrincipalHandle)
    : OutputStreamDriver(aSourceStream, aTrackId, aPrincipalHandle)
    , mFPS(aFPS)
    , mTimer(nullptr)
  {
    if (mFPS == 0.0) {
      return;
    }

    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (!mTimer) {
      return;
    }
    mTimer->InitWithFuncCallback(&TimerTick, this, int(1000 / mFPS), nsITimer::TYPE_REPEATING_SLACK);
  }

  static void TimerTick(nsITimer* aTimer, void* aClosure)
  {
    MOZ_ASSERT(aClosure);
    TimerDriver* driver = static_cast<TimerDriver*>(aClosure);

    driver->RequestFrameCapture();
  }

  void NewFrame(already_AddRefed<Image> aImage) override
  {
    RefPtr<Image> image = aImage;

    if (!mFrameCaptureRequested) {
      return;
    }

    mFrameCaptureRequested = false;
    SetImage(image.forget());
  }

  void Forget() override
  {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
  }

protected:
  virtual ~TimerDriver() {}

private:
  const double mFPS;
  nsCOMPtr<nsITimer> mTimer;
};

// ----------------------------------------------------------------------

class AutoDriver : public OutputStreamDriver
{
public:
  explicit AutoDriver(SourceMediaStream* aSourceStream,
                      const TrackID& aTrackId,
                      const PrincipalHandle& aPrincipalHandle)
    : OutputStreamDriver(aSourceStream, aTrackId, aPrincipalHandle) {}

  void NewFrame(already_AddRefed<Image> aImage) override
  {
    // Don't reset `mFrameCaptureRequested` since AutoDriver shall always have
    // `mFrameCaptureRequested` set to true.
    // This also means we should accept every frame as NewFrame is called only
    // after something changed.

    RefPtr<Image> image = aImage;
    SetImage(image.forget());
  }

protected:
  virtual ~AutoDriver() {}
};

// ----------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_INHERITED(CanvasCaptureMediaStream, DOMMediaStream,
                                   mCanvas)

NS_IMPL_ADDREF_INHERITED(CanvasCaptureMediaStream, DOMMediaStream)
NS_IMPL_RELEASE_INHERITED(CanvasCaptureMediaStream, DOMMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(CanvasCaptureMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMMediaStream)

CanvasCaptureMediaStream::CanvasCaptureMediaStream(nsPIDOMWindowInner* aWindow,
                                                   HTMLCanvasElement* aCanvas)
  : DOMMediaStream(aWindow, nullptr)
  , mCanvas(aCanvas)
  , mOutputStreamDriver(nullptr)
{
}

CanvasCaptureMediaStream::~CanvasCaptureMediaStream()
{
  if (mOutputStreamDriver) {
    mOutputStreamDriver->Forget();
  }
}

JSObject*
CanvasCaptureMediaStream::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::CanvasCaptureMediaStreamBinding::Wrap(aCx, this, aGivenProto);
}

void
CanvasCaptureMediaStream::RequestFrame()
{
  if (mOutputStreamDriver) {
    mOutputStreamDriver->RequestFrameCapture();
  }
}

nsresult
CanvasCaptureMediaStream::Init(const dom::Optional<double>& aFPS,
                               const TrackID& aTrackId,
                               nsIPrincipal* aPrincipal)
{
  PrincipalHandle principalHandle = MakePrincipalHandle(aPrincipal);

  if (!aFPS.WasPassed()) {
    mOutputStreamDriver =
      new AutoDriver(GetInputStream()->AsSourceStream(), aTrackId, principalHandle);
  } else if (aFPS.Value() < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  } else {
    // Cap frame rate to 60 FPS for sanity
    double fps = std::min(60.0, aFPS.Value());
    mOutputStreamDriver =
      new TimerDriver(GetInputStream()->AsSourceStream(), fps, aTrackId, principalHandle);
  }
  return NS_OK;
}

already_AddRefed<CanvasCaptureMediaStream>
CanvasCaptureMediaStream::CreateSourceStream(nsPIDOMWindowInner* aWindow,
                                             HTMLCanvasElement* aCanvas)
{
  RefPtr<CanvasCaptureMediaStream> stream = new CanvasCaptureMediaStream(aWindow, aCanvas);
  MediaStreamGraph* graph =
    MediaStreamGraph::GetInstance(MediaStreamGraph::SYSTEM_THREAD_DRIVER,
                                  AudioChannel::Normal);
  stream->InitSourceStream(graph);
  return stream.forget();
}

FrameCaptureListener*
CanvasCaptureMediaStream::FrameCaptureListener()
{
  return mOutputStreamDriver;
}

void
CanvasCaptureMediaStream::StopCapture()
{
  if (!mOutputStreamDriver) {
    return;
  }

  mOutputStreamDriver->Forget();
  mOutputStreamDriver = nullptr;
}

} // namespace dom
} // namespace mozilla

