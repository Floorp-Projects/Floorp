/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Utility functions to help with Apple API calls.

#ifndef mozilla_AppleUtils_h
#define mozilla_AppleUtils_h

#include <AudioToolbox/AudioToolbox.h>
#include "nsError.h"

namespace mozilla {

struct AppleUtils {
  // Helper to retrieve properties from AudioFileStream objects.
  static nsresult GetProperty(AudioFileStreamID aAudioFileStream,
                              AudioFileStreamPropertyID aPropertyID,
                              void *aData);

  // Helper to set a string, string pair on a CFMutableDictionaryRef.
  static void SetCFDict(CFMutableDictionaryRef dict,
                        const char* key,
                        const char* value);
  // Helper to set a string, int32_t pair on a CFMutableDictionaryRef.
  static void SetCFDict(CFMutableDictionaryRef dict,
                        const char* key,
                        int32_t value);
  // Helper to set a string, bool pair on a CFMutableDictionaryRef.
  static void SetCFDict(CFMutableDictionaryRef dict,
                        const char* key,
                        bool value);
};

// Wrapper class to call CFRelease on reference types
// when they go out of scope.
template <class T>
class AutoCFRelease {
public:
  AutoCFRelease(T aRef)
    : mRef(aRef)
  {
  }
  ~AutoCFRelease()
  {
    if (mRef) {
      CFRelease(mRef);
    }
  }
  // Return the wrapped ref so it can be used as an in parameter.
  operator T()
  {
    return mRef;
  }
  // Return a pointer to the wrapped ref for use as an out parameter.
  T* receive()
  {
    return &mRef;
  }
private:
  T mRef;
};

} // namespace mozilla

#endif // mozilla_AppleUtils_h
