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
#include "desktop_device_info.h"

#include "MediaUtils.h"

mozilla::LazyLogModule gTabShareLog("TabShare");

using namespace mozilla::dom;

namespace mozilla {

TabCapturer::TabCapturer(const webrtc::DesktopCaptureOptions& options)
    : mMonitor("TabCapture") {}

TabCapturer::~TabCapturer() = default;

bool TabCapturer::GetSourceList(webrtc::DesktopCapturer::SourceList* sources) {
  MOZ_LOG(gTabShareLog, LogLevel::Debug,
          ("TabShare: GetSourceList, result %zu", sources->size()));
  // XXX UI
  return true;
}

bool TabCapturer::SelectSource(webrtc::DesktopCapturer::SourceId id) {
  MOZ_LOG(gTabShareLog, LogLevel::Debug, ("TabShare: source %d", (int)id));
  mBrowserId = id;
  return true;
}

bool TabCapturer::FocusOnSelectedSource() { return true; }

nsresult TabCapturer::StartRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_LOG(gTabShareLog, LogLevel::Debug,
          ("TabShare: Start, id=%" PRIu64, mVideoSource->mBrowserId));

  mVideoSource->CaptureFrameNow();
  return NS_OK;
}

void TabCapturer::Start(webrtc::DesktopCapturer::Callback* callback) {
  RTC_DCHECK(!mCallback);
  RTC_DCHECK(callback);

  mCallback = callback;
  NS_DispatchToMainThread(new StartRunnable(this));
}

// NOTE: this method is synchronous
void TabCapturer::CaptureFrame() {
  MonitorAutoLock monitor(mMonitor);
  NS_DispatchToMainThread(NS_NewRunnableFunction("TabCapturer: Capture a frame",
                                                 [self = RefPtr{this}] {
                                                   self->CaptureFrameNow();
                                                   // New frame will be returned
                                                   // via Promise callback on
                                                   // mThread
                                                 }));
  // mCallback may not be valid if we don't make this call synchronous;
  // especially since CaptureFrameNow isn't a synchronous event
  monitor.Wait();
}

bool TabCapturer::IsOccluded(const webrtc::DesktopVector& pos) { return false; }

class TabCapturedHandler final : public dom::PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  static void Create(dom::Promise* aPromise, TabCapturer* aEngine) {
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(NS_IsMainThread());

    RefPtr<TabCapturedHandler> handler = new TabCapturedHandler(aEngine);
    aPromise->AppendNativeHandler(handler);
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    MOZ_ASSERT(NS_IsMainThread());
    MonitorAutoLock monitor(mEngine->mMonitor);
    if (NS_WARN_IF(!aValue.isObject())) {
      monitor.Notify();
      return;
    }

    RefPtr<dom::ImageBitmap> bitmap;
    if (NS_WARN_IF(NS_FAILED(
            UNWRAP_OBJECT(ImageBitmap, &aValue.toObject(), bitmap)))) {
      monitor.Notify();
      return;
    }

    mEngine->OnFrame(bitmap);
    mEngine->mCapturing = false;
    // We no longer will touch mCallback from MainThread for this frame
    monitor.Notify();
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    MOZ_ASSERT(NS_IsMainThread());
    MonitorAutoLock monitor(mEngine->mMonitor);
    mEngine->mCapturing = false;
    monitor.Notify();
  }

 private:
  explicit TabCapturedHandler(TabCapturer* aEngine) : mEngine(aEngine) {
    MOZ_ASSERT(aEngine);
  }

  ~TabCapturedHandler() = default;

  RefPtr<TabCapturer> mEngine;
};

NS_IMPL_ISUPPORTS0(TabCapturedHandler)

void TabCapturer::CaptureFrameNow() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_LOG(gTabShareLog, LogLevel::Debug, ("TabShare: CaptureFrameNow"));

  // Ensure we clean up on failures
  auto autoFailureCallback = MakeScopeExit([&] {
    MonitorAutoLock monitor(mMonitor);
    mCallback->OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_TEMPORARY,
                               nullptr);
    monitor.Notify();
  });
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
    return;
  }
  if (!mCapturing) {
    // XXX This would be more efficient if it returned a MozPromise, and
    // even more if we used CrossProcessPaint directly and returned a surface.
    RefPtr<dom::Promise> promise =
        wgp->DrawSnapshot(nullptr, 1.0, "white"_ns, false, IgnoreErrors());
    if (!promise) {
      return;
    }
    mCapturing = true;
    TabCapturedHandler::Create(promise, this);
  }
  // else we haven't finished the previous capture

  autoFailureCallback.release();
}

void TabCapturer::OnFrame(dom::ImageBitmap* aBitmap) {
  MOZ_ASSERT(NS_IsMainThread());
  mMonitor.AssertCurrentThreadOwns();
  UniquePtr<dom::ImageBitmapCloneData> data = aBitmap->ToCloneData();
  webrtc::DesktopSize size(data->mPictureRect.Width(),
                           data->mPictureRect.Height());
  webrtc::DesktopRect rect = webrtc::DesktopRect::MakeSize(size);
  std::unique_ptr<webrtc::DesktopFrame> frame(
      new webrtc::BasicDesktopFrame(size));

  gfx::DataSourceSurface::ScopedMap map(data->mSurface,
                                        gfx::DataSourceSurface::READ);
  if (!map.IsMapped()) {
    mCallback->OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_TEMPORARY,
                               nullptr);
    return;
  }
  frame->CopyPixelsFrom(map.GetData(), map.GetStride(), rect);

  mCallback->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                             std::move(frame));
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
  NS_DispatchToMainThread(
      mozilla::media::NewRunnableFrom([this]() -> nsresult {
        nsresult rv;
        nsCOMPtr<nsIBrowserWindowTracker> bwt =
            do_ImportModule("resource:///modules/BrowserWindowTracker.jsm",
                            "BrowserWindowTracker", &rv);
        if (NS_SUCCEEDED(rv)) {
          nsTArray<RefPtr<nsIVisibleTab>> tabArray;
          rv = bwt->GetAllVisibleTabs(tabArray);
          if (NS_SUCCEEDED(rv)) {
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
                desktop_tab_list_[desktopTab->getTabBrowserId()] = desktopTab;
                free(contentTitleUTF8);
              }
            }
          }
        }

        return NS_OK;
      }),
      nsIEventTarget::DISPATCH_SYNC);
}

}  // namespace webrtc
