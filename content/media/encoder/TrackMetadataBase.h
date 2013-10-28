/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TrackMetadataBase_h_
#define TrackMetadataBase_h_

#include "nsTArray.h"
#include "nsCOMPtr.h"
namespace mozilla {

// A class represent meta data for various codec format. Only support one track information.
class TrackMetadataBase
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TrackMetadataBase)
  enum MetadataKind {
    METADATA_OPUS,    // Represent the Opus metadata
    METADATA_VP8,
    METADATA_UNKNOW   // Metadata Kind not set
  };
  virtual ~TrackMetadataBase() {}
  // Return the specific metadata kind
  virtual MetadataKind GetKind() const = 0;
};

}
#endif
