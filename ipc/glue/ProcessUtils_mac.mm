/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessUtils.h"

#include "nsObjCExceptions.h"
#include "nsCocoaUtils.h"
#include "nsString.h"
#include "mozilla/Sprintf.h"

#define UNDOCUMENTED_SESSION_CONSTANT ((int)-2)

namespace mozilla {
namespace ipc {

static void* sApplicationASN = NULL;
static void* sApplicationInfoItem = NULL;

void SetThisProcessName(const char* aProcessName) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
  nsAutoreleasePool localPool;

  if (!aProcessName || strcmp(aProcessName, "") == 0) {
    return;
  }

  NSString* currentName = [[[NSBundle mainBundle] localizedInfoDictionary]
      objectForKey:(NSString*)kCFBundleNameKey];

  char formattedName[1024];
  SprintfLiteral(formattedName, "%s %s", [currentName UTF8String],
                 aProcessName);

  aProcessName = formattedName;

  // This function is based on Chrome/Webkit's and relies on potentially
  // dangerous SPI.
  typedef CFTypeRef (*LSGetASNType)();
  typedef OSStatus (*LSSetInformationItemType)(int, CFTypeRef, CFStringRef,
                                               CFStringRef, CFDictionaryRef*);

  CFBundleRef launchServices =
      ::CFBundleGetBundleWithIdentifier(CFSTR("com.apple.LaunchServices"));
  if (!launchServices) {
    NS_WARNING(
        "Failed to set process name: Could not open LaunchServices bundle");
    return;
  }

  if (!sApplicationASN) {
    sApplicationASN = ::CFBundleGetFunctionPointerForName(
        launchServices, CFSTR("_LSGetCurrentApplicationASN"));
    if (!sApplicationASN) {
      NS_WARNING("Failed to set process name: Could not get function pointer "
                 "for LaunchServices");
      return;
    }
  }

  LSGetASNType getASNFunc = reinterpret_cast<LSGetASNType>(sApplicationASN);

  if (!sApplicationInfoItem) {
    sApplicationInfoItem = ::CFBundleGetFunctionPointerForName(
        launchServices, CFSTR("_LSSetApplicationInformationItem"));
  }

  LSSetInformationItemType setInformationItemFunc =
      reinterpret_cast<LSSetInformationItemType>(sApplicationInfoItem);

  void* displayNameKeyAddr = ::CFBundleGetDataPointerForName(
      launchServices, CFSTR("_kLSDisplayNameKey"));

  CFStringRef displayNameKey = nil;
  if (displayNameKeyAddr) {
    displayNameKey =
        reinterpret_cast<CFStringRef>(*(CFStringRef*)displayNameKeyAddr);
  }

  // We need this to ensure we have a connection to the Process Manager, not
  // doing so will silently fail and process name wont be updated.
  ProcessSerialNumber psn;
  if (::GetCurrentProcess(&psn) != noErr) {
    return;
  }

  CFTypeRef currentAsn = getASNFunc ? getASNFunc() : nullptr;

  if (!getASNFunc || !setInformationItemFunc || !displayNameKey ||
      !currentAsn) {
    NS_WARNING("Failed to set process name: Accessing launchServices failed");
    return;
  }

  CFStringRef processName =
      ::CFStringCreateWithCString(nil, aProcessName, kCFStringEncodingASCII);
  if (!processName) {
    NS_WARNING("Failed to set process name: Could not create CFStringRef");
    return;
  }

  OSErr err = setInformationItemFunc(UNDOCUMENTED_SESSION_CONSTANT, currentAsn,
                                     displayNameKey, processName,
                                     nil);  // Optional out param
  ::CFRelease(processName);
  if (err != noErr) {
    NS_WARNING("Failed to set process name: LSSetInformationItemType err");
    return;
  }

  return;
  NS_OBJC_END_TRY_ABORT_BLOCK;
}

}  // namespace ipc
}  // namespace mozilla
