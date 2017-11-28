/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WMFUtils_h
#define WMFUtils_h

#include "ImageTypes.h"
#include "TimeUnits.h"
#include "VideoUtils.h"
#include "WMF.h"
#include "mozilla/gfx/Rect.h"
#include "nsString.h"

// Various utilities shared by WMF backend files.

namespace mozilla {

// Converts from microseconds to hundreds of nanoseconds.
// We use microseconds for our timestamps, whereas WMF uses
// hundreds of nanoseconds.
inline int64_t
UsecsToHNs(int64_t aUsecs)
{
  return aUsecs * 10;
}

// Converts from hundreds of nanoseconds to microseconds.
// We use microseconds for our timestamps, whereas WMF uses
// hundreds of nanoseconds.
inline int64_t
HNsToUsecs(int64_t hNanoSecs)
{
  return hNanoSecs / 10;
}

HRESULT HNsToFrames(int64_t aHNs, uint32_t aRate, int64_t* aOutFrames);

HRESULT
GetDefaultStride(IMFMediaType* aType, uint32_t aWidth, uint32_t* aOutStride);

YUVColorSpace GetYUVColorSpace(IMFMediaType* aType);

int32_t MFOffsetToInt32(const MFOffset& aOffset);

// Gets the sub-region of the video frame that should be displayed.
// See: http://msdn.microsoft.com/en-us/library/windows/desktop/bb530115(v=vs.85).aspx
HRESULT
GetPictureRegion(IMFMediaType* aMediaType, gfx::IntRect& aOutPictureRegion);

// Returns the duration of a IMFSample in TimeUnit.
// Returns media::TimeUnit::Invalid() on failure.
media::TimeUnit GetSampleDuration(IMFSample* aSample);

// Returns the presentation time of a IMFSample in TimeUnit.
// Returns media::TimeUnit::Invalid() on failure.
media::TimeUnit GetSampleTime(IMFSample* aSample);

inline bool
IsFlagSet(DWORD flags, DWORD pattern)
{
  return (flags & pattern) == pattern;
}

// Will return %ProgramW6432% value as per:
// https://msdn.microsoft.com/library/windows/desktop/aa384274.aspx
nsString GetProgramW6432Path();
} // namespace mozilla

#endif
