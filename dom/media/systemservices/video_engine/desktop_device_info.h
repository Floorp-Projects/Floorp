/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_DEVICE_INFO_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_DEVICE_INFO_H_

#include <map>
#include "modules/desktop_capture/desktop_capture_types.h"

namespace webrtc {

class DesktopDisplayDevice {
 public:
  DesktopDisplayDevice();
  ~DesktopDisplayDevice();

  void setScreenId(const ScreenId aScreenId);
  void setDeviceName(const char* aDeviceNameUTF8);
  void setUniqueIdName(const char* aDeviceUniqueIdUTF8);
  void setPid(pid_t aPid);

  ScreenId getScreenId();
  const char* getDeviceName();
  const char* getUniqueIdName();
  pid_t getPid();

  DesktopDisplayDevice& operator=(DesktopDisplayDevice& aOther);

 protected:
  ScreenId mScreenId;
  char* mDeviceNameUTF8;
  char* mDeviceUniqueIdUTF8;
  pid_t mPid;
};

using DesktopDisplayDeviceList = std::map<intptr_t, DesktopDisplayDevice*>;

class DesktopTab {
 public:
  DesktopTab();
  ~DesktopTab();

  void setTabBrowserId(uint64_t aTabBrowserId);
  void setUniqueIdName(const char* aTabUniqueIdUTF8);
  void setTabName(const char* aTabNameUTF8);
  void setTabCount(const uint32_t aCount);

  uint64_t getTabBrowserId();
  const char* getUniqueIdName();
  const char* getTabName();
  uint32_t getTabCount();

  DesktopTab& operator=(DesktopTab& aOther);

 protected:
  uint64_t mTabBrowserId;
  char* mTabNameUTF8;
  char* mTabUniqueIdUTF8;
  uint32_t mTabCount;
};

using DesktopTabList = std::map<intptr_t, DesktopTab*>;

class DesktopDeviceInfo {
 public:
  virtual ~DesktopDeviceInfo() = default;

  virtual int32_t Init() = 0;
  virtual int32_t Refresh() = 0;
  virtual int32_t getDisplayDeviceCount() = 0;
  virtual int32_t getDesktopDisplayDeviceInfo(
      uint32_t aIndex, DesktopDisplayDevice& aDesktopDisplayDevice) = 0;
  virtual int32_t getWindowCount() = 0;
  virtual int32_t getWindowInfo(uint32_t aIndex,
                                DesktopDisplayDevice& aWindowDevice) = 0;
  virtual uint32_t getTabCount() = 0;
  virtual int32_t getTabInfo(uint32_t aIndex, DesktopTab& aDesktopTab) = 0;

  static DesktopDeviceInfo* Create();
};
};  // namespace webrtc

#endif
