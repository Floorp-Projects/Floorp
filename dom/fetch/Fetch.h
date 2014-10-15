/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Fetch_h
#define mozilla_dom_Fetch_h

#include "mozilla/dom/UnionTypes.h"

class nsIInputStream;

namespace mozilla {
namespace dom {

/*
 * Creates an nsIInputStream based on the fetch specifications 'extract a byte
 * stream algorithm' - http://fetch.spec.whatwg.org/#concept-bodyinit-extract.
 * Stores content type in out param aContentType.
 */
nsresult
ExtractByteStreamFromBody(const OwningArrayBufferOrArrayBufferViewOrScalarValueStringOrURLSearchParams& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentType);

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Fetch_h
