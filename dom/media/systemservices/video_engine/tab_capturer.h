/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_TAB_CAPTURER_H_
#define MODULES_DESKTOP_CAPTURE_TAB_CAPTURER_H_

#include <memory>
#include <string>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/Monitor.h"

namespace mozilla {

class TabCapturedHandler;

class TabCapturer {
 private:
  ~TabCapturer();

  TabCapturer(const TabCapturer&) = delete;
  TabCapturer& operator=(const TabCapturer&) = delete;

 public:
  friend class TabCapturedHandler;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TabCapturer)

  explicit TabCapturer(const webrtc::DesktopCaptureOptions& options);

  static std::unique_ptr<webrtc::DesktopCapturer> CreateRawWindowCapturer(
      const webrtc::DesktopCaptureOptions& options);

  // support for DesktopCapturer interface.
  void Start(webrtc::DesktopCapturer::Callback* callback);
  void CaptureFrame();
  bool GetSourceList(webrtc::DesktopCapturer::SourceList* sources);
  bool SelectSource(webrtc::DesktopCapturer::SourceId id);
  bool FocusOnSelectedSource();
  bool IsOccluded(const webrtc::DesktopVector& pos);

  // Capture code
  void CaptureFrameNow();
  void OnFrame(dom::ImageBitmap* aBitmap);

  class StartRunnable : public Runnable {
   public:
    explicit StartRunnable(TabCapturer* videoSource)
        : Runnable("TabCapturer::StartRunnable"), mVideoSource(videoSource) {}
    NS_IMETHOD Run() override;
    const RefPtr<TabCapturer> mVideoSource;
  };

 private:
  // Used to protect mCallback, since TabCapturer's lifetime might be
  // longer than mCallback's on stop/shutdown, and we may be waiting on a
  // tab to finish capturing on MainThread.
  Monitor mMonitor MOZ_UNANNOTATED;
  webrtc::DesktopCapturer::Callback* mCallback = nullptr;

  uint64_t mBrowserId = 0;
  bool mCapturing = false;
};

// Warning: webrtc capture wants this in a uniqueptr, but it greatly eases
// things for us if it's a refcounted object (so we can pass it around to
// Dispatch/etc). Solve this by having a DesktopCapturer that just holds the
// refcounted TabCapturer.

// XXX This is a little ugly; we could decompose into front-end (webrtc)
// and backend (refcounted), but that doesn't actually make things a lot better.
class TabCapturerWebrtc : public webrtc::DesktopCapturer {
 public:
  explicit TabCapturerWebrtc(const webrtc::DesktopCaptureOptions& options) {
    mInternal = new TabCapturer(options);
  }

  ~TabCapturerWebrtc() override = default;

  // DesktopCapturer interface.
  void Start(Callback* callback) override { mInternal->Start(callback); }
  void CaptureFrame() override { mInternal->CaptureFrame(); }
  bool GetSourceList(SourceList* sources) override {
    return mInternal->GetSourceList(sources);
  }
  bool SelectSource(SourceId id) override {
    return mInternal->SelectSource(id);
  }
  bool FocusOnSelectedSource() override {
    return mInternal->FocusOnSelectedSource();
  }
  bool IsOccluded(const webrtc::DesktopVector& pos) override {
    return mInternal->IsOccluded(pos);
  }

 private:
  RefPtr<TabCapturer> mInternal;
};

}  // namespace mozilla

#endif  // MODULES_DESKTOP_CAPTURE_TAB_CAPTURER_H_
