/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "tab_capturer.h"

#include "desktop_device_info.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/TaskQueue.h"
#include "nsThreadUtils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

mozilla::LazyLogModule gTabShareLog("TabShare");
#define LOG_FUNC_IMPL(level) \
  MOZ_LOG(                   \
      gTabShareLog, level,   \
      ("TabCapturerWebrtc %p: %s id=%" PRIu64, this, __func__, mBrowserId))
#define LOG_FUNC() LOG_FUNC_IMPL(LogLevel::Debug)
#define LOG_FUNCV() LOG_FUNC_IMPL(LogLevel::Verbose)

using namespace mozilla::dom;

namespace mozilla {

class CaptureFrameRequest {
  using CapturePromise = TabCapturerWebrtc::CapturePromise;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CaptureFrameRequest)

  CaptureFrameRequest() : mCaptureTime(TimeStamp::Now()) {}

  operator MozPromiseRequestHolder<CapturePromise>&() { return mRequest; }

  void Complete() { mRequest.Complete(); }
  void Disconnect() { mRequest.Disconnect(); }
  bool Exists() { return mRequest.Exists(); }

 protected:
  virtual ~CaptureFrameRequest() { MOZ_RELEASE_ASSERT(!Exists()); }

 public:
  const TimeStamp mCaptureTime;

 private:
  MozPromiseRequestHolder<CapturePromise> mRequest;
};

TabCapturerWebrtc::TabCapturerWebrtc(
    SourceId aSourceId, nsCOMPtr<nsISerialEventTarget> aCaptureThread)
    : mBrowserId(aSourceId),
      mMainThreadWorker(
          TaskQueue::Create(do_AddRef(GetMainThreadSerialEventTarget()),
                            "TabCapturerWebrtc::mMainThreadWorker")),
      mCallbackWorker(TaskQueue::Create(aCaptureThread.forget(),
                                        "TabCapturerWebrtc::mCallbackWorker")) {
  RTC_DCHECK_RUN_ON(&mControlChecker);
  MOZ_ASSERT(aSourceId != 0);
  mCallbackChecker.Detach();

  LOG_FUNC();
}

// static
std::unique_ptr<webrtc::DesktopCapturer> TabCapturerWebrtc::Create(
    SourceId aSourceId, nsCOMPtr<nsISerialEventTarget> aCaptureThread) {
  return std::unique_ptr<webrtc::DesktopCapturer>(
      new TabCapturerWebrtc(aSourceId, std::move(aCaptureThread)));
}

TabCapturerWebrtc::~TabCapturerWebrtc() {
  RTC_DCHECK_RUN_ON(&mCallbackChecker);
  LOG_FUNC();

  // mMainThreadWorker handles frame capture requests async. Since we're in the
  // dtor, no more frame capture requests can be made through CaptureFrame(). It
  // can be shut down now.
  mMainThreadWorker->BeginShutdown();

  // There may still be async frame capture requests in flight, waiting to be
  // reported to mCallback on mCallbackWorker. Disconnect them (must be done on
  // mCallbackWorker) and shut down mCallbackWorker to ensure nothing more can
  // get queued to it.
  MOZ_ALWAYS_SUCCEEDS(
      mCallbackWorker->Dispatch(NS_NewRunnableFunction(__func__, [this] {
        RTC_DCHECK_RUN_ON(&mCallbackChecker);
        for (const auto& req : mRequests) {
          DisconnectRequest(req);
        }
        mCallbackWorker->BeginShutdown();
      })));

  // Block until the workers have run all pending tasks. We must do this for two
  // reasons:
  // - All runnables dispatched to mMainThreadWorker and mCallbackWorker capture
  //   the raw pointer `this` as they rely on `this` outliving the worker
  //   TaskQueues.
  // - mCallback is only guaranteed to outlive `this`. No calls can be made to
  //   it after the dtor is finished.

  // Spin the underlying thread of mCallbackWorker, which we are currently on,
  // until it is empty. We have no other way of waiting for mCallbackWorker to
  // become empty while blocking the current call.
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      "~TabCapturerWebrtc"_ns, [&] { return mCallbackWorker->IsEmpty(); });

  // No need to await shutdown since it was shut down synchronously above.
  mMainThreadWorker->AwaitIdle();
}

bool TabCapturerWebrtc::GetSourceList(
    webrtc::DesktopCapturer::SourceList* aSources) {
  MOZ_LOG(gTabShareLog, LogLevel::Debug,
          ("TabShare: GetSourceList, result %zu", aSources->size()));
  // XXX UI
  return true;
}

bool TabCapturerWebrtc::SelectSource(webrtc::DesktopCapturer::SourceId) {
  MOZ_ASSERT_UNREACHABLE("Source is passed through ctor for constness");
  return true;
}

bool TabCapturerWebrtc::FocusOnSelectedSource() { return true; }

void TabCapturerWebrtc::Start(webrtc::DesktopCapturer::Callback* aCallback) {
  RTC_DCHECK_RUN_ON(&mCallbackChecker);
  RTC_DCHECK(!mCallback);
  RTC_DCHECK(aCallback);

  LOG_FUNC();

  mCallback = aCallback;
}

void TabCapturerWebrtc::CaptureFrame() {
  RTC_DCHECK_RUN_ON(&mCallbackChecker);
  LOG_FUNCV();
  if (mRequests.GetSize() > 2) {
    // Allow two async capture requests in flight
    OnCaptureFrameFailure();
    return;
  }

  auto request = MakeRefPtr<CaptureFrameRequest>();
  InvokeAsync(mMainThreadWorker, __func__, [this] { return CaptureFrameNow(); })
      ->Then(mCallbackWorker, __func__,
             [this, request](CapturePromise::ResolveOrRejectValue&& aValue) {
               if (!CompleteRequest(request)) {
                 // Request was disconnected or overrun. Failure has already
                 // been reported to the callback elsewhere.
                 return;
               }

               if (aValue.IsReject()) {
                 OnCaptureFrameFailure();
                 return;
               }

               OnCaptureFrameSuccess(std::move(aValue.ResolveValue()));
             })
      ->Track(*request);
  mRequests.PushFront(request.forget());
}

void TabCapturerWebrtc::OnCaptureFrameSuccess(
    UniquePtr<dom::ImageBitmapCloneData> aData) {
  RTC_DCHECK_RUN_ON(&mCallbackChecker);
  MOZ_DIAGNOSTIC_ASSERT(aData);
  LOG_FUNCV();
  webrtc::DesktopSize size(aData->mPictureRect.Width(),
                           aData->mPictureRect.Height());
  webrtc::DesktopRect rect = webrtc::DesktopRect::MakeSize(size);
  std::unique_ptr<webrtc::DesktopFrame> frame(
      new webrtc::BasicDesktopFrame(size));

  gfx::DataSourceSurface::ScopedMap map(aData->mSurface,
                                        gfx::DataSourceSurface::READ);
  if (!map.IsMapped()) {
    OnCaptureFrameFailure();
    return;
  }
  frame->CopyPixelsFrom(map.GetData(), map.GetStride(), rect);

  mCallback->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                             std::move(frame));
}

void TabCapturerWebrtc::OnCaptureFrameFailure() {
  RTC_DCHECK_RUN_ON(&mCallbackChecker);
  LOG_FUNC();
  mCallback->OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_TEMPORARY,
                             nullptr);
}

bool TabCapturerWebrtc::IsOccluded(const webrtc::DesktopVector& aPos) {
  return false;
}

class TabCapturedHandler final : public PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  using CapturePromise = TabCapturerWebrtc::CapturePromise;

  static void Create(Promise* aPromise,
                     MozPromiseHolder<CapturePromise> aHolder) {
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(NS_IsMainThread());

    RefPtr<TabCapturedHandler> handler =
        new TabCapturedHandler(std::move(aHolder));
    aPromise->AppendNativeHandler(handler);
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    MOZ_ASSERT(NS_IsMainThread());
    if (NS_WARN_IF(!aValue.isObject())) {
      mHolder.Reject(NS_ERROR_UNEXPECTED, __func__);
      return;
    }

    RefPtr<ImageBitmap> bitmap;
    if (NS_WARN_IF(NS_FAILED(
            UNWRAP_OBJECT(ImageBitmap, &aValue.toObject(), bitmap)))) {
      mHolder.Reject(NS_ERROR_UNEXPECTED, __func__);
      return;
    }

    UniquePtr<ImageBitmapCloneData> data = bitmap->ToCloneData();
    if (!data) {
      mHolder.Reject(NS_ERROR_UNEXPECTED, __func__);
      return;
    }

    mHolder.Resolve(std::move(data), __func__);
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
    MOZ_ASSERT(NS_IsMainThread());
    mHolder.Reject(aRv.StealNSResult(), __func__);
  }

 private:
  explicit TabCapturedHandler(MozPromiseHolder<CapturePromise> aHolder)
      : mHolder(std::move(aHolder)) {}

  ~TabCapturedHandler() = default;

  MozPromiseHolder<CapturePromise> mHolder;
};

NS_IMPL_ISUPPORTS0(TabCapturedHandler)

bool TabCapturerWebrtc::CompleteRequest(CaptureFrameRequest* aRequest) {
  RTC_DCHECK_RUN_ON(&mCallbackChecker);
  if (!aRequest->Exists()) {
    // Request was disconnected or overrun. mCallback has already been notified.
    return false;
  }
  while (CaptureFrameRequest* req = mRequests.Peek()) {
    if (req->mCaptureTime > aRequest->mCaptureTime) {
      break;
    }
    // Pop the request before calling the callback, in case it could mutate
    // mRequests, now or in the future.
    RefPtr<CaptureFrameRequest> dropMe = mRequests.Pop();
    req->Complete();
    if (req->mCaptureTime < aRequest->mCaptureTime) {
      OnCaptureFrameFailure();
    }
  }
  MOZ_DIAGNOSTIC_ASSERT(!aRequest->Exists());
  return true;
}

void TabCapturerWebrtc::DisconnectRequest(CaptureFrameRequest* aRequest) {
  RTC_DCHECK_RUN_ON(&mCallbackChecker);
  LOG_FUNCV();
  aRequest->Disconnect();
  OnCaptureFrameFailure();
}

auto TabCapturerWebrtc::CaptureFrameNow() -> RefPtr<CapturePromise> {
  MOZ_ASSERT(mMainThreadWorker->IsOnCurrentThread());
  LOG_FUNCV();

  WindowGlobalParent* wgp = nullptr;
  RefPtr<BrowsingContext> context =
      BrowsingContext::GetCurrentTopByBrowserId(mBrowserId);
  if (context) {
    wgp = context->Canonical()->GetCurrentWindowGlobal();
  }
  if (!wgp) {
    // If we can't access the window, we just won't capture anything
    return CapturePromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  // XXX This would be more efficient if we used CrossProcessPaint directly and
  // returned a surface.
  RefPtr<Promise> promise =
      wgp->DrawSnapshot(nullptr, 1.0, "white"_ns, false, IgnoreErrors());
  if (!promise) {
    return CapturePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  MozPromiseHolder<CapturePromise> holder;
  RefPtr<CapturePromise> p = holder.Ensure(__func__);
  TabCapturedHandler::Create(promise, std::move(holder));
  return p;
}

}  // namespace mozilla
