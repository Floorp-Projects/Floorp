/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "desktop_device_info_x11.h"
#include "mozilla/Sprintf.h"

#include <inttypes.h>

#if !defined(WEBRTC_USE_X11) || !defined(MOZ_X11)
#  error Bad build setup, some X11 defines are missing
#endif

namespace webrtc {

DesktopDeviceInfo* DesktopDeviceInfoImpl::Create() {
  DesktopDeviceInfoX11* pDesktopDeviceInfo = new DesktopDeviceInfoX11();
  if (pDesktopDeviceInfo && pDesktopDeviceInfo->Init() != 0) {
    delete pDesktopDeviceInfo;
    pDesktopDeviceInfo = NULL;
  }
  return pDesktopDeviceInfo;
}

DesktopDeviceInfoX11::DesktopDeviceInfoX11() {}

DesktopDeviceInfoX11::~DesktopDeviceInfoX11() {}

void DesktopDeviceInfoX11::MultiMonitorScreenshare() {
  DesktopDisplayDevice* desktop_device_info = new DesktopDisplayDevice;
  if (desktop_device_info) {
    desktop_device_info->setScreenId(webrtc::kFullDesktopScreenId);
    desktop_device_info->setDeviceName("Primary Monitor");

    char idStr[64];
    SprintfLiteral(idStr, "%" PRIdPTR, desktop_device_info->getScreenId());
    desktop_device_info->setUniqueIdName(idStr);
    desktop_display_list_[desktop_device_info->getScreenId()] =
        desktop_device_info;
  }
}

void DesktopDeviceInfoX11::InitializeScreenList() { MultiMonitorScreenshare(); }

}  // namespace webrtc
