/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(XiphExtradata_h)
#define XiphExtradata_h

#include "MediaData.h"

namespace mozilla {

/* This converts a list of headers to the canonical form of extradata for Xiph
   codecs in non-Ogg containers. We use it to pass those headers from demuxer
   to decoder even when demuxing from an Ogg cotainer. */
bool XiphHeadersToExtradata(MediaByteBuffer* aCodecSpecificConfig,
                            const nsTArray<const unsigned char*>& aHeaders,
                            const nsTArray<size_t>& aHeaderLens);

/* This converts a set of extradata back into a list of headers. */
bool XiphExtradataToHeaders(nsTArray<unsigned char*>& aHeaders,
                            nsTArray<size_t>& aHeaderLens,
                            unsigned char* aData,
                            size_t aAvailable);

} // namespace mozilla

#endif // XiphExtradata_h
