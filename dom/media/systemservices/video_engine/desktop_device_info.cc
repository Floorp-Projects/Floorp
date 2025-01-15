/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "desktop_device_info.h"
#include "VideoEngine.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"
#include "nsIBrowserWindowTracker.h"
#include "nsImportModule.h"

#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>

using mozilla::camera::CaptureDeviceType;

namespace webrtc {

void DesktopSource::setScreenId(ScreenId aId) { mScreenId = aId; }
void DesktopSource::setName(nsCString&& aName) { mName = std::move(aName); }
void DesktopSource::setUniqueId(nsCString&& aId) { mUniqueId = std::move(aId); }
void DesktopSource::setPid(const int aPid) { mPid = aPid; }

ScreenId DesktopSource::getScreenId() const { return mScreenId; }
const nsCString& DesktopSource::getName() const { return mName; }
const nsCString& DesktopSource::getUniqueId() const { return mUniqueId; }
pid_t DesktopSource::getPid() const { return mPid; }

void TabSource::setBrowserId(uint64_t aId) { mBrowserId = aId; }
void TabSource::setUniqueId(nsCString&& aId) { mUniqueId = std::move(aId); }
void TabSource::setName(nsCString&& aName) { mName = std::move(aName); }

uint64_t TabSource::getBrowserId() const { return mBrowserId; }
const nsCString& TabSource::getName() const { return mName; }
const nsCString& TabSource::getUniqueId() const { return mUniqueId; }

template <CaptureDeviceType Type, typename Device>
class DesktopDeviceInfoImpl : public CaptureInfo<Device> {
 public:
  explicit DesktopDeviceInfoImpl(const DesktopCaptureOptions& aOptions);

  void Refresh() override;
  size_t getSourceCount() const override;
  const Device* getSource(size_t aIndex) const override;

 protected:
  const DesktopCaptureOptions mOptions;
  std::map<intptr_t, Device> mDeviceList;
};

template <CaptureDeviceType Type, typename Device>
DesktopDeviceInfoImpl<Type, Device>::DesktopDeviceInfoImpl(
    const DesktopCaptureOptions& aOptions)
    : mOptions(aOptions) {}

template <CaptureDeviceType Type, typename Device>
size_t DesktopDeviceInfoImpl<Type, Device>::getSourceCount() const {
  return mDeviceList.size();
}

template <CaptureDeviceType Type, typename Device>
const Device* DesktopDeviceInfoImpl<Type, Device>::getSource(
    size_t aIndex) const {
  if (aIndex >= mDeviceList.size()) {
    return nullptr;
  }
  auto it = mDeviceList.begin();
  std::advance(it, aIndex);
  return &std::get<Device>(*it);
}

static std::map<intptr_t, TabSource> InitializeTabList() {
  std::map<intptr_t, TabSource> tabList;
  if (!mozilla::StaticPrefs::media_getusermedia_browser_enabled()) {
    return tabList;
  }

  // This is a sync dispatch to main thread, which is unfortunate. To
  // call JavaScript we have to be on main thread, but the remaining
  // DesktopCapturer very much wants to be off main thread. This might
  // be solvable by calling this method earlier on while we're still on
  // main thread and plumbing the information down to here.
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(__func__, [&] {
    nsresult rv;
    nsCOMPtr<nsIBrowserWindowTracker> bwt =
        do_ImportESModule("resource:///modules/BrowserWindowTracker.sys.mjs",
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

      auto result = tabList.try_emplace(static_cast<intptr_t>(browserId));
      auto& [iter, inserted] = result;
      if (!inserted) {
        MOZ_ASSERT_UNREACHABLE("Duplicate browser ids");
        continue;
      }
      auto& [key, desktopTab] = *iter;
      desktopTab.setBrowserId(browserId);
      desktopTab.setName(NS_ConvertUTF16toUTF8(contentTitle));
      desktopTab.setUniqueId(nsPrintfCString("%" PRId64, browserId));
    }
  });
  mozilla::SyncRunnable::DispatchToThread(
      mozilla::GetMainThreadSerialEventTarget(), runnable);
  return tabList;
}

template <CaptureDeviceType Type, typename Device>
void DesktopDeviceInfoImpl<Type, Device>::Refresh() {
  if constexpr (Type == CaptureDeviceType::Browser) {
    mDeviceList = InitializeTabList();
    return;
  }

  mDeviceList.clear();

#if defined(WEBRTC_USE_PIPEWIRE)
  if (mOptions.allow_pipewire() &&
      webrtc::DesktopCapturer::IsRunningUnderWayland()) {
    // Wayland is special and we will not get any information about screens or
    // windows without going through xdg-desktop-portal. We add a single screen
    // placeholder here.
    if constexpr (Type == CaptureDeviceType::Screen) {
      // With PipeWire we can't select which system resource is shared so
      // we don't create a window/screen list. Instead we place these constants
      // as window name/id so frontend code can identify PipeWire backend
      // and does not try to create screen/window preview.
      constexpr ScreenId PIPEWIRE_ID = 0xaffffff;
      constexpr const char* PIPEWIRE_NAME = "####_PIPEWIRE_PORTAL_####";

      auto result = mDeviceList.try_emplace(PIPEWIRE_ID);
      auto& [iter, inserted] = result;
      if (!inserted) {
        MOZ_CRASH("Device list was supposed to be empty");
      }
      auto& [key, device] = *iter;

      device.setScreenId(PIPEWIRE_ID);
      device.setUniqueId(nsPrintfCString("%" PRIdPTR, PIPEWIRE_ID));
      device.setName(nsCString(PIPEWIRE_NAME));
      return;
    } else if constexpr (Type == CaptureDeviceType::Window) {
      // Wayland is special and we will not get any information about windows
      // without going through xdg-desktop-portal. We will already have
      // a screen placeholder so there is no reason to build windows list.
      return;
    }
  }
#endif

  std::unique_ptr<DesktopCapturer> cap;
  if constexpr (Type == CaptureDeviceType::Screen ||
                Type == CaptureDeviceType::Window) {
    if constexpr (Type == CaptureDeviceType::Screen) {
      cap = DesktopCapturer::CreateScreenCapturer(mOptions);
    } else if constexpr (Type == CaptureDeviceType::Window) {
      cap = DesktopCapturer::CreateWindowCapturer(mOptions);
    }

    if (!cap) {
      return;
    }

    DesktopCapturer::SourceList list;
    if (!cap->GetSourceList(&list)) {
      return;
    }

    for (const auto& elem : list) {
      auto result = mDeviceList.try_emplace(elem.id);
      auto& [iter, inserted] = result;
      if (!inserted) {
        MOZ_ASSERT_UNREACHABLE("Duplicate screen id");
        continue;
      }
      auto& [key, device] = *iter;
      device.setScreenId(elem.id);
      device.setUniqueId(nsPrintfCString("%" PRIdPTR, elem.id));
      if (Type == CaptureDeviceType::Screen && list.size() == 1) {
        device.setName(nsCString("Primary Monitor"));
      } else {
        device.setName(nsCString(elem.title.c_str()));
      }
      device.setPid(elem.pid);
    }
  }
}

std::unique_ptr<DesktopCaptureInfo> CreateScreenCaptureInfo(
    const DesktopCaptureOptions& aOptions) {
  std::unique_ptr<DesktopCaptureInfo> info(
      new DesktopDeviceInfoImpl<CaptureDeviceType::Screen, DesktopSource>(
          aOptions));
  info->Refresh();
  return info;
}

std::unique_ptr<DesktopCaptureInfo> CreateWindowCaptureInfo(
    const DesktopCaptureOptions& aOptions) {
  std::unique_ptr<DesktopCaptureInfo> info(
      new DesktopDeviceInfoImpl<CaptureDeviceType::Window, DesktopSource>(
          aOptions));
  info->Refresh();
  return info;
}

std::unique_ptr<TabCaptureInfo> CreateTabCaptureInfo() {
  std::unique_ptr<TabCaptureInfo> info(
      new DesktopDeviceInfoImpl<CaptureDeviceType::Browser, TabSource>(
          DesktopCaptureOptions()));
  info->Refresh();
  return info;
}

// simulate deviceInfo interface for video engine, bridge screen/application and
// real screen/application device info
template <typename Source>
class DesktopCaptureDeviceInfo final : public VideoCaptureModule::DeviceInfo {
 public:
  DesktopCaptureDeviceInfo(int32_t aId,
                           std::unique_ptr<CaptureInfo<Source>>&& aSourceInfo);

  int32_t Refresh() override;

  uint32_t NumberOfDevices() override;
  int32_t GetDeviceName(uint32_t aDeviceNumber, char* aDeviceNameUTF8,
                        uint32_t aDeviceNameUTF8Size, char* aDeviceUniqueIdUTF8,
                        uint32_t aDeviceUniqueIdUTF8Size,
                        char* aProductUniqueIdUTF8,
                        uint32_t aProductUniqueIdUTF8Size, pid_t* aPid,
                        bool* aDeviceIsPlaceholder = nullptr) override;

  int32_t DisplayCaptureSettingsDialogBox(const char* aDeviceUniqueIdUTF8,
                                          const char* aDialogTitleUTF8,
                                          void* aParentWindow,
                                          uint32_t aPositionX,
                                          uint32_t aPositionY) override;
  int32_t NumberOfCapabilities(const char* aDeviceUniqueIdUTF8) override;
  int32_t GetCapability(const char* aDeviceUniqueIdUTF8,
                        uint32_t aDeviceCapabilityNumber,
                        VideoCaptureCapability& aCapability) override;

  int32_t GetBestMatchedCapability(const char* aDeviceUniqueIdUTF8,
                                   const VideoCaptureCapability& aRequested,
                                   VideoCaptureCapability& aResulting) override;
  int32_t GetOrientation(const char* aDeviceUniqueIdUTF8,
                         VideoRotation& aOrientation) override;

 protected:
  int32_t mId;
  std::unique_ptr<CaptureInfo<Source>> mDeviceInfo;
};

using DesktopDeviceInfo = DesktopCaptureDeviceInfo<DesktopSource>;
using TabDeviceInfo = DesktopCaptureDeviceInfo<TabSource>;

template <typename Source>
DesktopCaptureDeviceInfo<Source>::DesktopCaptureDeviceInfo(
    int32_t aId, std::unique_ptr<CaptureInfo<Source>>&& aSourceInfo)
    : mId(aId), mDeviceInfo(std::move(aSourceInfo)) {}

template <typename Source>
int32_t DesktopCaptureDeviceInfo<Source>::Refresh() {
  mDeviceInfo->Refresh();
  return 0;
}

template <typename Source>
uint32_t DesktopCaptureDeviceInfo<Source>::NumberOfDevices() {
  return mDeviceInfo->getSourceCount();
}

template <>
int32_t DesktopCaptureDeviceInfo<DesktopSource>::GetDeviceName(
    uint32_t aDeviceNumber, char* aDeviceNameUTF8, uint32_t aDeviceNameUTF8Size,
    char* aDeviceUniqueIdUTF8, uint32_t aDeviceUniqueIdUTF8Size,
    char* aProductUniqueIdUTF8, uint32_t aProductUniqueIdUTF8Size, pid_t* aPid,
    bool* aDeviceIsPlaceholder) {
  // always initialize output
  if (aDeviceNameUTF8 && aDeviceNameUTF8Size > 0) {
    memset(aDeviceNameUTF8, 0, aDeviceNameUTF8Size);
  }
  if (aDeviceUniqueIdUTF8 && aDeviceUniqueIdUTF8Size > 0) {
    memset(aDeviceUniqueIdUTF8, 0, aDeviceUniqueIdUTF8Size);
  }
  if (aProductUniqueIdUTF8 && aProductUniqueIdUTF8Size > 0) {
    memset(aProductUniqueIdUTF8, 0, aProductUniqueIdUTF8Size);
  }

  const DesktopSource* source = mDeviceInfo->getSource(aDeviceNumber);
  if (!source) {
    return 0;
  }

  const nsCString& deviceName = source->getName();
  size_t len = deviceName.Length();
  if (len && aDeviceNameUTF8 && len < aDeviceNameUTF8Size) {
    memcpy(aDeviceNameUTF8, deviceName.Data(), len);
  }

  const nsCString& deviceUniqueId = source->getUniqueId();
  len = deviceUniqueId.Length();
  if (len && aDeviceUniqueIdUTF8 && len < aDeviceUniqueIdUTF8Size) {
    memcpy(aDeviceUniqueIdUTF8, deviceUniqueId.Data(), len);
  }

  if (aPid) {
    *aPid = source->getPid();
  }

  return 0;
}

template <>
int32_t DesktopCaptureDeviceInfo<TabSource>::GetDeviceName(
    uint32_t aDeviceNumber, char* aDeviceNameUTF8, uint32_t aDeviceNameUTF8Size,
    char* aDeviceUniqueIdUTF8, uint32_t aDeviceUniqueIdUTF8Size,
    char* aProductUniqueIdUTF8, uint32_t aProductUniqueIdUTF8Size, pid_t* aPid,
    bool* aDeviceIsPlaceholder) {
  // always initialize output
  if (aDeviceNameUTF8 && aDeviceNameUTF8Size > 0) {
    memset(aDeviceNameUTF8, 0, aDeviceNameUTF8Size);
  }
  if (aDeviceUniqueIdUTF8 && aDeviceUniqueIdUTF8Size > 0) {
    memset(aDeviceUniqueIdUTF8, 0, aDeviceUniqueIdUTF8Size);
  }
  if (aProductUniqueIdUTF8 && aProductUniqueIdUTF8Size > 0) {
    memset(aProductUniqueIdUTF8, 0, aProductUniqueIdUTF8Size);
  }

  const TabSource* source = mDeviceInfo->getSource(aDeviceNumber);
  if (!source) {
    return 0;
  }

  const nsCString& deviceName = source->getName();
  size_t len = deviceName.Length();
  if (len && aDeviceNameUTF8 && len < aDeviceNameUTF8Size) {
    memcpy(aDeviceNameUTF8, deviceName.Data(), len);
  }

  const nsCString& deviceUniqueId = source->getUniqueId();
  len = deviceUniqueId.Length();
  if (len && aDeviceUniqueIdUTF8 && len < aDeviceUniqueIdUTF8Size) {
    memcpy(aDeviceUniqueIdUTF8, deviceUniqueId.Data(), len);
  }

  return 0;
}

template <typename Source>
int32_t DesktopCaptureDeviceInfo<Source>::DisplayCaptureSettingsDialogBox(
    const char* aDeviceUniqueIdUTF8, const char* aDialogTitleUTF8,
    void* aParentWindow, uint32_t aPositionX, uint32_t aPositionY) {
  // no device properties to change
  return 0;
}

template <typename Source>
int32_t DesktopCaptureDeviceInfo<Source>::NumberOfCapabilities(
    const char* aDeviceUniqueIdUTF8) {
  return 0;
}

template <typename Source>
int32_t DesktopCaptureDeviceInfo<Source>::GetCapability(
    const char* aDeviceUniqueIdUTF8, uint32_t aDeviceCapabilityNumber,
    VideoCaptureCapability& aCapability) {
  return 0;
}

template <typename Source>
int32_t DesktopCaptureDeviceInfo<Source>::GetBestMatchedCapability(
    const char* aDeviceUniqueIdUTF8, const VideoCaptureCapability& aRequested,
    VideoCaptureCapability& aResulting) {
  return 0;
}

template <typename Source>
int32_t DesktopCaptureDeviceInfo<Source>::GetOrientation(
    const char* aDeviceUniqueIdUTF8, VideoRotation& aOrientation) {
  return 0;
}

std::shared_ptr<VideoCaptureModule::DeviceInfo> CreateDesktopDeviceInfo(
    int32_t aId, std::unique_ptr<DesktopCaptureInfo>&& aInfo) {
  return std::make_shared<DesktopDeviceInfo>(aId, std::move(aInfo));
}

std::shared_ptr<VideoCaptureModule::DeviceInfo> CreateTabDeviceInfo(
    int32_t aId, std::unique_ptr<TabCaptureInfo>&& aInfo) {
  return std::make_shared<TabDeviceInfo>(aId, std::move(aInfo));
}
}  // namespace webrtc
