/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMF.h"
#include "nsString.h"

// Various utilities shared by WMF backend files.

namespace mozilla {

nsCString GetGUIDName(const GUID& guid);

// Returns true if the reader has a stream with the specified index.
// Index can be a specific index, or one of:
//   MF_SOURCE_READER_FIRST_VIDEO_STREAM
//   MF_SOURCE_READER_FIRST_AUDIO_STREAM
bool
SourceReaderHasStream(IMFSourceReader* aReader, const DWORD aIndex);

// Auto manages the lifecycle of a PROPVARIANT.
class AutoPropVar {
public:
  AutoPropVar() {
    PropVariantInit(&mVar);
  }
  ~AutoPropVar() {
    PropVariantClear(&mVar);
  }
  operator PROPVARIANT&() {
    return mVar;
  }
  PROPVARIANT* operator->() {
    return &mVar;
  }
  PROPVARIANT* operator&() {
    return &mVar;
  }
private:
  PROPVARIANT mVar;
};

// Converts from microseconds to hundreds of nanoseconds.
// We use microseconds for our timestamps, whereas WMF uses
// hundreds of nanoseconds.
inline int64_t UsecsToHNs(int64_t aUsecs) {
  return aUsecs * 10;
}

// Converts from hundreds of nanoseconds to microseconds.
// We use microseconds for our timestamps, whereas WMF uses
// hundreds of nanoseconds.
inline int64_t HNsToUsecs(int64_t hNanoSecs) {
  return hNanoSecs / 10;
}

} // namespace mozilla
