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
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Mutex.h"
#include "nsContentUtils.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

class OutputStreamDriver::StreamListener : public MediaStreamListener
{
public:
  explicit StreamListener(OutputStreamDriver* aDriver,
                          SourceMediaStream* aSourceStream)
    : mSourceStream(aSourceStream)
    , mMutex("CanvasCaptureMediaStream::OSD::StreamListener")
    , mDriver(aDriver)
  {
    MOZ_ASSERT(mDriver);
    MOZ_ASSERT(mSourceStream);
  }

  void Forget() {
    MOZ_ASSERT(NS_IsMainThread());

    MutexAutoLock lock(mMutex);
    mDriver = nullptr;
  }

  virtual void NotifyPull(MediaStreamGraph* aGraph, StreamTime aDesiredTime) override
  {
    // Called on the MediaStreamGraph thread.

    MutexAutoLock lock(mMutex);
    if (mDriver) {
      mDriver->NotifyPull(aDesiredTime);
    } else {
      // The DOM stream is dead, let's end it
      mSourceStream->EndAllTrackAndFinish();
    }
  }

protected:
  ~StreamListener() { }

private:
  nsRefPtr<SourceMediaStream> mSourceStream;

  // The below members are protected by mMutex.
  Mutex mMutex;
  // This is a raw pointer to avoid a reference cycle with OutputStreamDriver.
  // Accessed on main and MediaStreamGraph threads, set on main thread.
  OutputStreamDriver* mDriver;
};

OutputStreamDriver::OutputStreamDriver(CanvasCaptureMediaStream* aDOMStream,
                                       const TrackID& aTrackId)
  : mDOMStream(aDOMStream)
  , mSourceStream(nullptr)
  , mStarted(false)
  , mStreamListener(nullptr)
  , mTrackId(aTrackId)
  , mMutex("CanvasCaptureMediaStream::OutputStreamDriver")
  , mImage(nullptr)
{
  MOZ_ASSERT(mDOMStream);
}

OutputStreamDriver::~OutputStreamDriver()
{
  if (mStreamListener) {
    // MediaStreamGraph will keep the listener alive until it can finish the
    // stream on the next NotifyPull().
    mStreamListener->Forget();
  }
}

nsresult
OutputStreamDriver::Start()
{
  if (mStarted) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  MOZ_ASSERT(mDOMStream);

  mDOMStream->CreateDOMTrack(mTrackId, MediaSegment::VIDEO);

  mSourceStream = mDOMStream->GetStream()->AsSourceStream();
  MOZ_ASSERT(mSourceStream);

  mStreamListener = new StreamListener(this, mSourceStream);
  mSourceStream->AddListener(mStreamListener);
  mSourceStream->AddTrack(mTrackId, 0, new VideoSegment());
  mSourceStream->AdvanceKnownTracksTime(STREAM_TIME_MAX);
  mSourceStream->SetPullEnabled(true);

  // Run StartInternal() in stable state to allow it to directly capture a frame
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &OutputStreamDriver::StartInternal);
  nsContentUtils::RunInStableState(runnable.forget());

  mStarted = true;
  return NS_OK;
}

void
OutputStreamDriver::ForgetDOMStream()
{
  if (mStreamListener) {
    mStreamListener->Forget();
  }
  mDOMStream = nullptr;
}

void
OutputStreamDriver::AppendToTrack(StreamTime aDuration)
{
  MOZ_ASSERT(mSourceStream);

  MutexAutoLock lock(mMutex);

  nsRefPtr<Image> image = mImage;
  IntSize size = image ? image->GetSize() : IntSize(0, 0);
  VideoSegment segment;
  segment.AppendFrame(image.forget(), aDuration, size);

  mSourceStream->AppendToTrack(mTrackId, &segment);
}

void
OutputStreamDriver::NotifyPull(StreamTime aDesiredTime)
{
  StreamTime delta = aDesiredTime - mSourceStream->GetEndOfAppendedData(mTrackId);
  if (delta > 0) {
    // nullptr images are allowed
    AppendToTrack(delta);
  }
}

void
OutputStreamDriver::SetImage(Image* aImage)
{
  MutexAutoLock lock(mMutex);
  mImage = aImage;
}

// ----------------------------------------------------------------------

class TimerDriver : public OutputStreamDriver
                  , public nsITimerCallback
{
public:
  explicit TimerDriver(CanvasCaptureMediaStream* aDOMStream,
                       const double& aFPS,
                       const TrackID& aTrackId)
    : OutputStreamDriver(aDOMStream, aTrackId)
    , mFPS(aFPS)
    , mTimer(nullptr)
  {
  }

  void ForgetDOMStream() override
  {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
    OutputStreamDriver::ForgetDOMStream();
  }

  nsresult
  TakeSnapshot()
  {
    // mDOMStream can't be killed while we're on main thread
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(DOMStream());

    if (!DOMStream()->Canvas()) {
      // DOMStream's canvas pointer was garbage collected. We can abort now.
      return NS_ERROR_NOT_AVAILABLE;
    }
    MOZ_ASSERT(DOMStream()->Canvas());

    if (DOMStream()->Canvas()->IsWriteOnly()) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    // Pass `nullptr` to force alpha-premult.
    RefPtr<SourceSurface> snapshot = DOMStream()->Canvas()->GetSurfaceSnapshot(nullptr);
    if (!snapshot) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<DataSourceSurface> data = snapshot->GetDataSurface();
    if (!data) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<DataSourceSurface> copy;

    {
      DataSourceSurface::ScopedMap read(data, DataSourceSurface::READ);
      if (!read.IsMapped()) {
        return NS_ERROR_FAILURE;
      }

      copy = Factory::CreateDataSourceSurfaceWithStride(data->GetSize(),
                                                        data->GetFormat(),
                                                        read.GetStride());
      if (!copy) {
        return NS_ERROR_FAILURE;
      }

      DataSourceSurface::ScopedMap write(copy, DataSourceSurface::WRITE);
      if (!write.IsMapped()) {
        return NS_ERROR_FAILURE;
      }

      MOZ_ASSERT(read.GetStride() == write.GetStride());
      MOZ_ASSERT(data->GetSize() == copy->GetSize());
      MOZ_ASSERT(data->GetFormat() == copy->GetFormat());

      memcpy(write.GetData(), read.GetData(),
             write.GetStride() * copy->GetSize().height);
    }

    CairoImage::Data imageData;
    imageData.mSize = copy->GetSize();
    imageData.mSourceSurface = copy;

    RefPtr<CairoImage> image = new layers::CairoImage();
    image->SetData(imageData);

    SetImage(image);
    return NS_OK;
  }

  NS_IMETHODIMP
  Notify(nsITimer* aTimer) override
  {
    nsresult rv = TakeSnapshot();
    if (NS_FAILED(rv)) {
      aTimer->Cancel();
    }
    return rv;
  }

  virtual void RequestFrame() override
  {
    TakeSnapshot();
  }

  NS_DECL_ISUPPORTS_INHERITED

protected:
  virtual ~TimerDriver() {}

  virtual void StartInternal() override
  {
    // Always capture at least one frame.
    DebugOnly<nsresult> rv = TakeSnapshot();
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (mFPS == 0.0) {
      return;
    }

    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (!mTimer) {
      return;
    }
    mTimer->InitWithCallback(this, int(1000 / mFPS), nsITimer::TYPE_REPEATING_SLACK);
  }

private:
  const double mFPS;
  nsCOMPtr<nsITimer> mTimer;
};

NS_IMPL_ADDREF_INHERITED(TimerDriver, OutputStreamDriver)
NS_IMPL_RELEASE_INHERITED(TimerDriver, OutputStreamDriver)
NS_IMPL_QUERY_INTERFACE(TimerDriver, nsITimerCallback)

// ----------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_INHERITED(CanvasCaptureMediaStream, DOMMediaStream,
                                   mCanvas)

NS_IMPL_ADDREF_INHERITED(CanvasCaptureMediaStream, DOMMediaStream)
NS_IMPL_RELEASE_INHERITED(CanvasCaptureMediaStream, DOMMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(CanvasCaptureMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMMediaStream)

CanvasCaptureMediaStream::CanvasCaptureMediaStream(HTMLCanvasElement* aCanvas)
  : mCanvas(aCanvas)
  , mOutputStreamDriver(nullptr)
{
}

CanvasCaptureMediaStream::~CanvasCaptureMediaStream()
{
  if (mOutputStreamDriver) {
    mOutputStreamDriver->ForgetDOMStream();
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
    mOutputStreamDriver->RequestFrame();
  }
}

nsresult
CanvasCaptureMediaStream::Init(const dom::Optional<double>& aFPS,
                               const TrackID& aTrackId)
{
  if (!aFPS.WasPassed()) {
    // TODO (Bug 1152298): Implement a real AutoDriver.
    // We use a 30FPS TimerDriver for now.
    mOutputStreamDriver = new TimerDriver(this, 30.0, aTrackId);
  } else if (aFPS.Value() < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  } else {
    // Cap frame rate to 60 FPS for sanity
    double fps = std::min(60.0, aFPS.Value());
    mOutputStreamDriver = new TimerDriver(this, fps, aTrackId);
  }
  return mOutputStreamDriver->Start();
}

already_AddRefed<CanvasCaptureMediaStream>
CanvasCaptureMediaStream::CreateSourceStream(nsIDOMWindow* aWindow,
                                             HTMLCanvasElement* aCanvas)
{
  nsRefPtr<CanvasCaptureMediaStream> stream = new CanvasCaptureMediaStream(aCanvas);
  stream->InitSourceStream(aWindow);
  return stream.forget();
}

} // namespace dom
} // namespace mozilla

