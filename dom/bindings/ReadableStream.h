/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadableStream_h
#define mozilla_dom_ReadableStream_h

#ifdef MOZ_DOM_STREAMS
#  error "This shouldn't have been included"
#else
#  include "js/experimental/TypedData.h"
#  include "mozilla/dom/SpiderMonkeyInterface.h"
namespace mozilla {
namespace dom {

class ReadableStream : public SpiderMonkeyInterfaceObjectStorage {
 public:
  inline bool Init(JSObject* obj) {
    MOZ_ASSERT(!inited());
    mImplObj = mWrappedObj = js::UnwrapReadableStream(obj);
    return inited();
  }
};

}  // namespace dom
}  // namespace mozilla
#endif
#endif /* mozilla_dom_ReadableStream_h */
