/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_DEVICE_INFO_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_DEVICE_INFO_H_

#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/video_capture/video_capture.h"
#include "nsString.h"

namespace webrtc {

class DesktopCaptureOptions;

class DesktopSource {
 public:
  void setScreenId(ScreenId aId);
  void setName(nsCString&& aName);
  void setUniqueId(nsCString&& aId);
  void setPid(pid_t aPid);

  ScreenId getScreenId() const;
  const nsCString& getName() const;
  const nsCString& getUniqueId() const;
  pid_t getPid() const;

 protected:
  ScreenId mScreenId = kInvalidScreenId;
  nsCString mName;
  nsCString mUniqueId;
  pid_t mPid = 0;
};

class TabSource {
 public:
  void setBrowserId(uint64_t aId);
  void setName(nsCString&& aName);
  void setUniqueId(nsCString&& aId);

  uint64_t getBrowserId() const;
  const nsCString& getName() const;
  const nsCString& getUniqueId() const;

 protected:
  uint64_t mBrowserId = 0;
  nsCString mName;
  nsCString mUniqueId;
};

template <typename Source>
class CaptureInfo {
 public:
  virtual ~CaptureInfo() = default;

  virtual void Refresh() = 0;
  virtual size_t getSourceCount() const = 0;
  virtual const Source* getSource(size_t aIndex) const = 0;
};

using DesktopCaptureInfo = CaptureInfo<DesktopSource>;
std::unique_ptr<DesktopCaptureInfo> CreateScreenCaptureInfo(
    const DesktopCaptureOptions& aOptions);
std::unique_ptr<DesktopCaptureInfo> CreateWindowCaptureInfo(
    const DesktopCaptureOptions& aOptions);
using TabCaptureInfo = CaptureInfo<TabSource>;
std::unique_ptr<TabCaptureInfo> CreateTabCaptureInfo();

std::shared_ptr<VideoCaptureModule::DeviceInfo> CreateDesktopDeviceInfo(
    int32_t aId, std::unique_ptr<DesktopCaptureInfo>&& aInfo);
std::shared_ptr<VideoCaptureModule::DeviceInfo> CreateTabDeviceInfo(
    int32_t aId, std::unique_ptr<TabCaptureInfo>&& aInfo);

};  // namespace webrtc

#endif
