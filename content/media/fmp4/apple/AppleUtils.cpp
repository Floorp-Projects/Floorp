/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Utility functions to help with Apple API calls.

#include <AudioToolbox/AudioToolbox.h>
#include "AppleUtils.h"
#include "prlog.h"

#ifdef PR_LOGGING
PRLogModuleInfo* GetDemuxerLog();
#define WARN(...) PR_LOG(GetDemuxerLog(), PR_LOG_WARNING, (__VA_ARGS__))
#else
#define WARN(...)
#endif

#define PROPERTY_ID_FORMAT "%c%c%c%c"
#define PROPERTY_ID_PRINT(x) ((x) >> 24), \
                             ((x) >> 16) & 0xff, \
                             ((x) >> 8) & 0xff, \
                              (x) & 0xff

namespace mozilla {

nsresult
AppleUtils::GetProperty(AudioFileStreamID aAudioFileStream,
                        AudioFileStreamPropertyID aPropertyID,
                        void *aData)
{
  UInt32 size;
  Boolean writeable;
  OSStatus rv = AudioFileStreamGetPropertyInfo(aAudioFileStream, aPropertyID,
                                                             &size, &writeable);

  if (rv) {
    WARN("Couldn't get property " PROPERTY_ID_FORMAT "\n",
         PROPERTY_ID_PRINT(aPropertyID));
    return NS_ERROR_FAILURE;
  }

  rv = AudioFileStreamGetProperty(aAudioFileStream, aPropertyID,
                                  &size, aData);

  return NS_OK;
}

void
AppleUtils::SetCFDict(CFMutableDictionaryRef dict,
                      const char* key,
                      const char* value)
{
  // We avoid using the CFSTR macros because there's no way to release those.
  AutoCFRelease<CFStringRef> keyRef =
    CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
  AutoCFRelease<CFStringRef> valueRef =
    CFStringCreateWithCString(NULL, value, kCFStringEncodingUTF8);
  CFDictionarySetValue(dict, keyRef, valueRef);
}

void
AppleUtils::SetCFDict(CFMutableDictionaryRef dict,
                      const char* key,
                      int32_t value)
{
  AutoCFRelease<CFNumberRef> valueRef =
    CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
  AutoCFRelease<CFStringRef> keyRef =
    CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
  CFDictionarySetValue(dict, keyRef, valueRef);
}

void
AppleUtils::SetCFDict(CFMutableDictionaryRef dict,
                      const char* key,
                      bool value)
{
  AutoCFRelease<CFStringRef> keyRef =
    CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
  CFDictionarySetValue(dict, keyRef, value ? kCFBooleanTrue : kCFBooleanFalse);
}


} // namespace mozilla
