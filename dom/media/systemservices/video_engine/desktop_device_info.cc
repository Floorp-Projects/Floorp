/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "desktop_device_info.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/UniquePtr.h"
#include "nsIBrowserWindowTracker.h"
#include "nsImportModule.h"

#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>

namespace webrtc {

static inline void SetStringMember(char** aMember, const char* aValue) {
  if (!aValue) {
    return;
  }

  if (*aMember) {
    delete[] *aMember;
    *aMember = nullptr;
  }

  size_t nBufLen = strlen(aValue) + 1;
  char* buffer = new char[nBufLen];
  memcpy(buffer, aValue, nBufLen - 1);
  buffer[nBufLen - 1] = '\0';
  *aMember = buffer;
}

DesktopDisplayDevice::DesktopDisplayDevice() {
  mScreenId = kInvalidScreenId;
  mDeviceUniqueIdUTF8 = nullptr;
  mDeviceNameUTF8 = nullptr;
  mPid = 0;
}

DesktopDisplayDevice::~DesktopDisplayDevice() {
  mScreenId = kInvalidScreenId;

  delete[] mDeviceUniqueIdUTF8;
  delete[] mDeviceNameUTF8;

  mDeviceUniqueIdUTF8 = nullptr;
  mDeviceNameUTF8 = nullptr;
}

void DesktopDisplayDevice::setScreenId(const ScreenId aScreenId) {
  mScreenId = aScreenId;
}

void DesktopDisplayDevice::setDeviceName(const char* aDeviceNameUTF8) {
  SetStringMember(&mDeviceNameUTF8, aDeviceNameUTF8);
}

void DesktopDisplayDevice::setUniqueIdName(const char* aDeviceUniqueIdUTF8) {
  SetStringMember(&mDeviceUniqueIdUTF8, aDeviceUniqueIdUTF8);
}

void DesktopDisplayDevice::setPid(const int aPid) { mPid = aPid; }

ScreenId DesktopDisplayDevice::getScreenId() { return mScreenId; }

const char* DesktopDisplayDevice::getDeviceName() { return mDeviceNameUTF8; }

const char* DesktopDisplayDevice::getUniqueIdName() {
  return mDeviceUniqueIdUTF8;
}

pid_t DesktopDisplayDevice::getPid() { return mPid; }

DesktopDisplayDevice& DesktopDisplayDevice::operator=(
    DesktopDisplayDevice& aOther) {
  if (&aOther == this) {
    return *this;
  }
  mScreenId = aOther.getScreenId();
  setUniqueIdName(aOther.getUniqueIdName());
  setDeviceName(aOther.getDeviceName());
  mPid = aOther.getPid();

  return *this;
}

DesktopTab::DesktopTab() {
  mTabBrowserId = 0;
  mTabNameUTF8 = nullptr;
  mTabUniqueIdUTF8 = nullptr;
  mTabCount = 0;
}

DesktopTab::~DesktopTab() {
  delete[] mTabNameUTF8;
  delete[] mTabUniqueIdUTF8;

  mTabNameUTF8 = nullptr;
  mTabUniqueIdUTF8 = nullptr;
}

void DesktopTab::setTabBrowserId(uint64_t aTabBrowserId) {
  mTabBrowserId = aTabBrowserId;
}

void DesktopTab::setUniqueIdName(const char* aTabUniqueIdUTF8) {
  SetStringMember(&mTabUniqueIdUTF8, aTabUniqueIdUTF8);
}

void DesktopTab::setTabName(const char* aTabNameUTF8) {
  SetStringMember(&mTabNameUTF8, aTabNameUTF8);
}

void DesktopTab::setTabCount(const uint32_t aCount) { mTabCount = aCount; }

uint64_t DesktopTab::getTabBrowserId() { return mTabBrowserId; }

const char* DesktopTab::getUniqueIdName() { return mTabUniqueIdUTF8; }

const char* DesktopTab::getTabName() { return mTabNameUTF8; }

uint32_t DesktopTab::getTabCount() { return mTabCount; }

DesktopTab& DesktopTab::operator=(DesktopTab& aOther) {
  mTabBrowserId = aOther.getTabBrowserId();
  setUniqueIdName(aOther.getUniqueIdName());
  setTabName(aOther.getTabName());

  return *this;
}

class DesktopDeviceInfoImpl : public DesktopDeviceInfo {
 public:
  DesktopDeviceInfoImpl();
  ~DesktopDeviceInfoImpl();

  int32_t Init() override;
  int32_t Refresh() override;
  int32_t getDisplayDeviceCount() override;
  int32_t getDesktopDisplayDeviceInfo(
      uint32_t aIndex, DesktopDisplayDevice& aDesktopDisplayDevice) override;
  int32_t getWindowCount() override;
  int32_t getWindowInfo(uint32_t aIndex,
                        DesktopDisplayDevice& aWindowDevice) override;
  uint32_t getTabCount() override;
  int32_t getTabInfo(uint32_t aIndex, DesktopTab& aDesktopTab) override;

 protected:
  DesktopDisplayDeviceList mDesktopDisplayList;
  DesktopDisplayDeviceList mDesktopWindowList;
  DesktopTabList mDesktopTabList;

  void CleanUp();
  void CleanUpWindowList();
  void CleanUpTabList();
  void CleanUpScreenList();

  void InitializeWindowList();
  virtual void InitializeTabList();
  void InitializeScreenList();

  void RefreshWindowList();
  void RefreshTabList();
  void RefreshScreenList();

  void DummyTabList(DesktopTabList& aList);
};

DesktopDeviceInfoImpl::DesktopDeviceInfoImpl() = default;

DesktopDeviceInfoImpl::~DesktopDeviceInfoImpl() { CleanUp(); }

int32_t DesktopDeviceInfoImpl::getDisplayDeviceCount() {
  return static_cast<int32_t>(mDesktopDisplayList.size());
}

int32_t DesktopDeviceInfoImpl::getDesktopDisplayDeviceInfo(
    uint32_t aIndex, DesktopDisplayDevice& aDesktopDisplayDevice) {
  if (aIndex >= mDesktopDisplayList.size()) {
    return -1;
  }

  std::map<intptr_t, DesktopDisplayDevice*>::iterator iter =
      mDesktopDisplayList.begin();
  std::advance(iter, aIndex);
  DesktopDisplayDevice* desktopDisplayDevice = iter->second;
  if (desktopDisplayDevice) {
    aDesktopDisplayDevice = (*desktopDisplayDevice);
  }

  return 0;
}

int32_t DesktopDeviceInfoImpl::getWindowCount() {
  return static_cast<int32_t>(mDesktopWindowList.size());
}

int32_t DesktopDeviceInfoImpl::getWindowInfo(
    uint32_t aIndex, DesktopDisplayDevice& aWindowDevice) {
  if (aIndex >= mDesktopWindowList.size()) {
    return -1;
  }

  std::map<intptr_t, DesktopDisplayDevice*>::iterator itr =
      mDesktopWindowList.begin();
  std::advance(itr, aIndex);
  DesktopDisplayDevice* window = itr->second;
  if (!window) {
    return -1;
  }

  aWindowDevice = (*window);
  return 0;
}

uint32_t DesktopDeviceInfoImpl::getTabCount() { return mDesktopTabList.size(); }

int32_t DesktopDeviceInfoImpl::getTabInfo(uint32_t aIndex,
                                          DesktopTab& aDesktopTab) {
  if (aIndex >= mDesktopTabList.size()) {
    return -1;
  }

  std::map<intptr_t, DesktopTab*>::iterator iter = mDesktopTabList.begin();
  std::advance(iter, aIndex);
  DesktopTab* desktopTab = iter->second;
  if (desktopTab) {
    aDesktopTab = (*desktopTab);
  }

  return 0;
}

void DesktopDeviceInfoImpl::CleanUp() {
  CleanUpScreenList();
  CleanUpWindowList();
  CleanUpTabList();
}
int32_t DesktopDeviceInfoImpl::Init() {
  InitializeScreenList();
  InitializeWindowList();
  InitializeTabList();

  return 0;
}
int32_t DesktopDeviceInfoImpl::Refresh() {
  RefreshScreenList();
  RefreshWindowList();
  RefreshTabList();

  return 0;
}

void DesktopDeviceInfoImpl::CleanUpWindowList() {
  std::map<intptr_t, DesktopDisplayDevice*>::iterator iterWindow;
  for (iterWindow = mDesktopWindowList.begin();
       iterWindow != mDesktopWindowList.end(); iterWindow++) {
    DesktopDisplayDevice* aWindow = iterWindow->second;
    delete aWindow;
    iterWindow->second = nullptr;
  }
  mDesktopWindowList.clear();
}

void DesktopDeviceInfoImpl::InitializeWindowList() {
  DesktopCaptureOptions options;

// Wayland is special and we will not get any information about windows
// without going through xdg-desktop-portal. We will already have
// a screen placeholder so there is no reason to build windows list.
#if defined(WEBRTC_USE_PIPEWIRE)
  if (mozilla::StaticPrefs::media_webrtc_capture_allow_pipewire() &&
      webrtc::DesktopCapturer::IsRunningUnderWayland()) {
    return;
  }
#endif

// Help avoid an X11 deadlock, see bug 1456101.
#ifdef MOZ_X11
  MOZ_ALWAYS_SUCCEEDS(mozilla::SyncRunnable::DispatchToThread(
      mozilla::GetMainThreadSerialEventTarget(),
      NS_NewRunnableFunction(__func__, [&] {
        options = DesktopCaptureOptions::CreateDefault();
      })));
#else
  options = DesktopCaptureOptions::CreateDefault();
#endif
  std::unique_ptr<DesktopCapturer> winCap =
      DesktopCapturer::CreateWindowCapturer(options);
  DesktopCapturer::SourceList list;
  if (winCap && winCap->GetSourceList(&list)) {
    DesktopCapturer::SourceList::iterator itr;
    for (itr = list.begin(); itr != list.end(); itr++) {
      DesktopDisplayDevice* winDevice = new DesktopDisplayDevice;
      if (!winDevice) {
        continue;
      }

      winDevice->setScreenId(itr->id);
      winDevice->setDeviceName(itr->title.c_str());
      winDevice->setPid(itr->pid);

      char idStr[BUFSIZ];
#if WEBRTC_WIN
      _snprintf_s(idStr, sizeof(idStr), sizeof(idStr) - 1, "%ld",
                  static_cast<long>(winDevice->getScreenId()));
#else
      SprintfLiteral(idStr, "%ld", static_cast<long>(winDevice->getScreenId()));
#endif
      winDevice->setUniqueIdName(idStr);
      mDesktopWindowList[winDevice->getScreenId()] = winDevice;
    }
  }
}

void DesktopDeviceInfoImpl::RefreshWindowList() {
  CleanUpWindowList();
  InitializeWindowList();
}

void DesktopDeviceInfoImpl::CleanUpTabList() {
  for (auto& iterTab : mDesktopTabList) {
    DesktopTab* desktopTab = iterTab.second;
    delete desktopTab;
    iterTab.second = nullptr;
  }
  mDesktopTabList.clear();
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

      DesktopTab* desktopTab = new DesktopTab;
      if (desktopTab) {
        char* contentTitleUTF8 = ToNewUTF8String(contentTitle);
        desktopTab->setTabBrowserId(browserId);
        desktopTab->setTabName(contentTitleUTF8);
        std::ostringstream uniqueId;
        uniqueId << browserId;
        desktopTab->setUniqueIdName(uniqueId.str().c_str());
        mDesktopTabList[static_cast<intptr_t>(desktopTab->getTabBrowserId())] =
            desktopTab;
        free(contentTitleUTF8);
      }
    }
  });
  mozilla::SyncRunnable::DispatchToThread(
      mozilla::GetMainThreadSerialEventTarget(), runnable);
}

void DesktopDeviceInfoImpl::RefreshTabList() {
  CleanUpTabList();
  InitializeTabList();
}

void DesktopDeviceInfoImpl::CleanUpScreenList() {
  std::map<intptr_t, DesktopDisplayDevice*>::iterator iterDevice;
  for (iterDevice = mDesktopDisplayList.begin();
       iterDevice != mDesktopDisplayList.end(); iterDevice++) {
    DesktopDisplayDevice* desktopDisplayDevice = iterDevice->second;
    delete desktopDisplayDevice;
    iterDevice->second = nullptr;
  }
  mDesktopDisplayList.clear();
}

// With PipeWire we can't select which system resource is shared so
// we don't create a window/screen list. Instead we place these constants
// as window name/id so frontend code can identify PipeWire backend
// and does not try to create screen/window preview.

#define PIPEWIRE_ID 0xaffffff
#define PIPEWIRE_NAME "####_PIPEWIRE_PORTAL_####"

void DesktopDeviceInfoImpl::InitializeScreenList() {
  DesktopCaptureOptions options;

// Wayland is special and we will not get any information about screens
// without going through xdg-desktop-portal so we just need a screen
// placeholder.
#if defined(WEBRTC_USE_PIPEWIRE)
  if (mozilla::StaticPrefs::media_webrtc_capture_allow_pipewire() &&
      webrtc::DesktopCapturer::IsRunningUnderWayland()) {
    DesktopDisplayDevice* screenDevice = new DesktopDisplayDevice;
    if (!screenDevice) {
      return;
    }

    screenDevice->setScreenId(PIPEWIRE_ID);
    screenDevice->setDeviceName(PIPEWIRE_NAME);

    char idStr[BUFSIZ];
    SprintfLiteral(idStr, "%ld",
                   static_cast<long>(screenDevice->getScreenId()));
    screenDevice->setUniqueIdName(idStr);
    mDesktopDisplayList[screenDevice->getScreenId()] = screenDevice;
    return;
  }
#endif

// Help avoid an X11 deadlock, see bug 1456101.
#ifdef MOZ_X11
  MOZ_ALWAYS_SUCCEEDS(mozilla::SyncRunnable::DispatchToThread(
      mozilla::GetMainThreadSerialEventTarget(),
      NS_NewRunnableFunction(__func__, [&] {
        options = DesktopCaptureOptions::CreateDefault();
      })));
#else
  options = DesktopCaptureOptions::CreateDefault();
#endif
  std::unique_ptr<DesktopCapturer> screenCapturer =
      DesktopCapturer::CreateScreenCapturer(options);
  DesktopCapturer::SourceList list;
  if (screenCapturer && screenCapturer->GetSourceList(&list)) {
    DesktopCapturer::SourceList::iterator itr;
    for (itr = list.begin(); itr != list.end(); itr++) {
      DesktopDisplayDevice* screenDevice = new DesktopDisplayDevice;
      screenDevice->setScreenId(itr->id);
      if (list.size() == 1) {
        screenDevice->setDeviceName("Primary Monitor");
      } else {
        screenDevice->setDeviceName(itr->title.c_str());
      }
      screenDevice->setPid(itr->pid);

      char idStr[BUFSIZ];
#if WEBRTC_WIN
      _snprintf_s(idStr, sizeof(idStr), sizeof(idStr) - 1, "%ld",
                  static_cast<long>(screenDevice->getScreenId()));
#else
      SprintfLiteral(idStr, "%ld",
                     static_cast<long>(screenDevice->getScreenId()));
#endif
      screenDevice->setUniqueIdName(idStr);
      mDesktopDisplayList[screenDevice->getScreenId()] = screenDevice;
    }
  }
}

void DesktopDeviceInfoImpl::RefreshScreenList() {
  CleanUpScreenList();
  InitializeScreenList();
}

/* static */
DesktopDeviceInfo* DesktopDeviceInfo::Create() {
  auto info = mozilla::MakeUnique<DesktopDeviceInfoImpl>();
  if (info->Init() != 0) {
    return nullptr;
  }
  return info.release();
}
}  // namespace webrtc
