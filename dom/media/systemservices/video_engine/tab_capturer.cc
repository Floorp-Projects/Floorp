/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"

#include "tab_capturer.h"

#include <memory>
#include <string>
#include <utility>

#include "modules/desktop_capture/desktop_frame.h"
#include "mozilla/Logging.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "nsThreadUtils.h"
#include "nsIBrowserWindowTracker.h"
#include "nsIDocShellTreeOwner.h"
#include "nsImportModule.h"
#include "mozilla/dom/BrowserHost.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"
#include "desktop_device_info.h"

#include "MediaUtils.h"

mozilla::LazyLogModule gTabShareLog("TabShare");

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
  virtual ~CaptureFrameRequest() = default;

 public:
  const TimeStamp mCaptureTime;

 private:
  MozPromiseRequestHolder<CapturePromise> mRequest;
};

TabCapturerWebrtc::TabCapturerWebrtc(
    const webrtc::DesktopCaptureOptions& options)
    : mMainThreadWorker(
          TaskQueue::Create(do_AddRef(GetMainThreadSerialEventTarget()),
                            "TabCapturerWebrtc::mMainThreadWorker")) {}

TabCapturerWebrtc::~TabCapturerWebrtc() {
  MOZ_ALWAYS_SUCCEEDS(
      mMainThreadWorker->Dispatch(NS_NewRunnableFunction(__func__, [this] {
        for (const auto& req : mRequests) {
          req->Disconnect();
        }
        mMainThreadWorker->BeginShutdown();
      })));
  // Block until the worker has run all pending tasks, since mCallback must
  // outlive them, and libwebrtc only guarantees mCallback outlives us.
  mMainThreadWorker->AwaitShutdownAndIdle();
}

bool TabCapturerWebrtc::GetSourceList(
    webrtc::DesktopCapturer::SourceList* sources) {
  MOZ_LOG(gTabShareLog, LogLevel::Debug,
          ("TabShare: GetSourceList, result %zu", sources->size()));
  // XXX UI
  return true;
}

bool TabCapturerWebrtc::SelectSource(webrtc::DesktopCapturer::SourceId id) {
  MOZ_LOG(gTabShareLog, LogLevel::Debug, ("TabShare: source %d", (int)id));
  mBrowserId = id;
  return true;
}

bool TabCapturerWebrtc::FocusOnSelectedSource() { return true; }

void TabCapturerWebrtc::Start(webrtc::DesktopCapturer::Callback* callback) {
  RTC_DCHECK(!mCallback);
  RTC_DCHECK(callback);

  MOZ_LOG(gTabShareLog, LogLevel::Debug,
          ("TabShare: Start, id=%" PRIu64, mBrowserId));

  mCallback = callback;
  CaptureFrame();
}

void TabCapturerWebrtc::CaptureFrame() {
  auto request = MakeRefPtr<CaptureFrameRequest>();
  InvokeAsync(mMainThreadWorker, __func__,
              [this, request]() mutable {
                if (mRequests.GetSize() > 2) {
                  // Allow two async capture requests in flight
                  request->Disconnect();
                  return CapturePromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                         __func__);
                }
                mRequests.PushFront(request.forget());
                return CaptureFrameNow();
              })
      ->Then(
          mMainThreadWorker, __func__,
          [this, request](const RefPtr<dom::ImageBitmap>& aBitmap) {
            if (!CompleteRequest(request)) {
              return;
            }

            UniquePtr<dom::ImageBitmapCloneData> data = aBitmap->ToCloneData();
            webrtc::DesktopSize size(data->mPictureRect.Width(),
                                     data->mPictureRect.Height());
            webrtc::DesktopRect rect = webrtc::DesktopRect::MakeSize(size);
            std::unique_ptr<webrtc::DesktopFrame> frame(
                new webrtc::BasicDesktopFrame(size));

            gfx::DataSourceSurface::ScopedMap map(data->mSurface,
                                                  gfx::DataSourceSurface::READ);
            if (!map.IsMapped()) {
              mCallback->OnCaptureResult(
                  webrtc::DesktopCapturer::Result::ERROR_TEMPORARY, nullptr);
              return;
            }
            frame->CopyPixelsFrom(map.GetData(), map.GetStride(), rect);

            mCallback->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                                       std::move(frame));
          },
          [this, request](nsresult aRv) {
            if (!CompleteRequest(request)) {
              return;
            }

            mCallback->OnCaptureResult(
                webrtc::DesktopCapturer::Result::ERROR_TEMPORARY, nullptr);
          })
      ->Track(*request);
}

bool TabCapturerWebrtc::IsOccluded(const webrtc::DesktopVector& pos) {
  return false;
}

class TabCapturedHandler final : public dom::PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  using CapturePromise = TabCapturerWebrtc::CapturePromise;

  static void Create(dom::Promise* aPromise,
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

    RefPtr<dom::ImageBitmap> bitmap;
    if (NS_WARN_IF(NS_FAILED(
            UNWRAP_OBJECT(ImageBitmap, &aValue.toObject(), bitmap)))) {
      mHolder.Reject(NS_ERROR_UNEXPECTED, __func__);
      return;
    }

    mHolder.Resolve(std::move(bitmap), __func__);
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
  MOZ_ASSERT(mMainThreadWorker->IsOnCurrentThread());
  if (!aRequest->Exists()) {
    // Request was disconnected or overrun
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
      mCallback->OnCaptureResult(
          webrtc::DesktopCapturer::Result::ERROR_TEMPORARY, nullptr);
    }
  }
  MOZ_DIAGNOSTIC_ASSERT(!aRequest->Exists());
  return true;
}

auto TabCapturerWebrtc::CaptureFrameNow() -> RefPtr<CapturePromise> {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_LOG(gTabShareLog, LogLevel::Debug, ("TabShare: CaptureFrameNow"));

  WindowGlobalParent* wgp = nullptr;
  if (mBrowserId != 0) {
    RefPtr<BrowsingContext> context =
        BrowsingContext::GetCurrentTopByBrowserId(mBrowserId);
    if (context) {
      wgp = context->Canonical()->GetCurrentWindowGlobal();
    }
    // If we can't access the window, we just won't capture anything
  }
  if (!wgp) {
    return CapturePromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  // XXX This would be more efficient if we used CrossProcessPaint directly and
  // returned a surface.
  RefPtr<dom::Promise> promise =
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

namespace webrtc {
// static
std::unique_ptr<webrtc::DesktopCapturer>
webrtc::DesktopCapturer::CreateRawTabCapturer(
    const webrtc::DesktopCaptureOptions& options) {
  return std::unique_ptr<webrtc::DesktopCapturer>(
      new mozilla::TabCapturerWebrtc(options));
}

void webrtc::DesktopDeviceInfoImpl::InitializeTabList() {
  if (!mozilla::StaticPrefs::media_getusermedia_browser_enabled()) {
    return;
  }

  // This is a sync dispatch to main thread, which is unfortunate. To
  // call JavaScript we have to be on main thread, but the remaining
  // DesktopCapturer very much wants to be off main thread. This might
  // be solvable by calling this method earlier on while we're still on
  // main thread and plumbing the information down to here.
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(__func__, [&] {
    nsresult rv;
    nsCOMPtr<nsIBrowserWindowTracker> bwt =
        do_ImportModule("resource:///modules/BrowserWindowTracker.jsm",
                        "BrowserWindowTracker", &rv);
    if (NS_FAILED(rv)) {
      return;
    }

    nsTArray<RefPtr<nsIVisibleTab>> tabArray;
    rv = bwt->GetAllVisibleTabs(tabArray);
    if (NS_FAILED(rv)) {
      return;
    }

    for (const auto& browserTab : tabArray) {
      nsString contentTitle;
      browserTab->GetContentTitle(contentTitle);
      int64_t browserId;
      browserTab->GetBrowserId(&browserId);

      DesktopTab* desktopTab = new DesktopTab;
      if (desktopTab) {
        char* contentTitleUTF8 = ToNewUTF8String(contentTitle);
        desktopTab->setTabBrowserId(browserId);
        desktopTab->setTabName(contentTitleUTF8);
        std::ostringstream uniqueId;
        uniqueId << browserId;
        desktopTab->setUniqueIdName(uniqueId.str().c_str());
        desktop_tab_list_[static_cast<intptr_t>(
            desktopTab->getTabBrowserId())] = desktopTab;
        free(contentTitleUTF8);
      }
    }
  });
  mozilla::SyncRunnable::DispatchToThread(
      mozilla::GetMainThreadSerialEventTarget(), runnable);
}

}  // namespace webrtc
