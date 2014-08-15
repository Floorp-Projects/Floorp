/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Utility functions to help with Apple API calls.

#include "AppleUtils.h"
#include "prlog.h"
#include "nsAutoPtr.h"

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
                        void* aData)
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

nsresult
AppleUtils::GetRichestDecodableFormat(AudioFileStreamID aAudioFileStream,
                                      AudioStreamBasicDescription& aFormat)
{
  // Fill in the default format description from the stream.
  nsresult rv = GetProperty(aAudioFileStream,
                            kAudioFileStreamProperty_DataFormat, &aFormat);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  UInt32 propertySize;
  OSStatus status = AudioFileStreamGetPropertyInfo(
    aAudioFileStream, kAudioFileStreamProperty_FormatList, &propertySize, NULL);
  if (NS_WARN_IF(status)) {
    // Return the default format description.
    return NS_OK;
  }

  MOZ_ASSERT(propertySize % sizeof(AudioFormatListItem) == 0);
  uint32_t sizeList = propertySize / sizeof(AudioFormatListItem);
  nsAutoArrayPtr<AudioFormatListItem> formatListPtr(
    new AudioFormatListItem[sizeList]);

  rv = GetProperty(aAudioFileStream, kAudioFileStreamProperty_FormatList,
                   formatListPtr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Return the default format description.
    return NS_OK;
  }

  // Get the index number of the first playable format.
  // This index number will be for the highest quality layer the platform
  // is capable of playing.
  UInt32 itemIndex;
  UInt32 indexSize = sizeof(itemIndex);
  status =
    AudioFormatGetProperty(kAudioFormatProperty_FirstPlayableFormatFromList,
                           propertySize, formatListPtr, &indexSize, &itemIndex);
  if (NS_WARN_IF(status)) {
    // Return the default format description.
    return NS_OK;
  }
  aFormat = formatListPtr[itemIndex].mASBD;

  return NS_OK;
}

} // namespace mozilla
