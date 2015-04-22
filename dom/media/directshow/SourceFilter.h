/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(nsDirectShowSource_h___)
#define nsDirectShowSource_h___

#include "BaseFilter.h"
#include "BasePin.h"
#include "MediaType.h"

#include "nsDeque.h"
#include "nsAutoPtr.h"
#include "DirectShowUtils.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

class MediaResource;
class OutputPin;


// SourceFilter is an asynchronous DirectShow source filter which
// reads from an MediaResource, and supplies data via a pull model downstream
// using OutputPin. It us used to supply a generic byte stream into
// DirectShow.
//
// Implements:
//  * IBaseFilter
//  * IMediaFilter
//  * IPersist
//  * IUnknown
//
class DECLSPEC_UUID("5c2a7ad0-ba82-4659-9178-c4719a2765d6")
SourceFilter : public media::BaseFilter
{
public:

  // Constructs source filter to deliver given media type.
  SourceFilter(const GUID& aMajorType, const GUID& aSubType);
  ~SourceFilter();

  nsresult Init(MediaResource *aResource, int64_t aMP3Offset);

  // BaseFilter overrides.
  // Only one output - the byte stream.
  int GetPinCount() override { return 1; }

  media::BasePin* GetPin(int n) override;

  // Get's the media type we're supplying.
  const media::MediaType* GetMediaType() const;

  uint32_t GetAndResetBytesConsumedCount();

protected:

  // Our async pull output pin.
  nsAutoPtr<OutputPin> mOutputPin;

  // Type of byte stream we output.
  media::MediaType mMediaType;

};

} // namespace mozilla

// For mingw __uuidof support
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(mozilla::SourceFilter, 0x5c2a7ad0,0xba82,0x4659,0x91,0x78,0xc4,0x71,0x9a,0x27,0x65,0xd6);
#endif

#endif
