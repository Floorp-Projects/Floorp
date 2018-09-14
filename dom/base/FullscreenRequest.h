/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/*
 * Struct for holding fullscreen request.
 */

#ifndef mozilla_FullscreenRequest_h
#define mozilla_FullscreenRequest_h

#include "mozilla/LinkedList.h"
#include "mozilla/dom/Element.h"
#include "nsIDocument.h"

namespace mozilla {

struct FullscreenRequest : public LinkedListElement<FullscreenRequest>
{
  FullscreenRequest(dom::Element* aElement, dom::CallerType aCallerType)
    : mElement(aElement)
    , mDocument(aElement->OwnerDoc())
    , mCallerType(aCallerType)
  {
    MOZ_COUNT_CTOR(FullscreenRequest);
  }

  FullscreenRequest(const FullscreenRequest&) = delete;

  ~FullscreenRequest()
  {
    MOZ_COUNT_DTOR(FullscreenRequest);
  }

  dom::Element* GetElement() const { return mElement; }
  nsIDocument* GetDocument() const { return mDocument; }

private:
  RefPtr<dom::Element> mElement;
  RefPtr<nsIDocument> mDocument;

public:
  // This value should be true if the fullscreen request is
  // originated from system code.
  const dom::CallerType mCallerType;
  // This value denotes whether we should trigger a NewOrigin event if
  // requesting fullscreen in its document causes the origin which is
  // fullscreen to change. We may want *not* to trigger that event if
  // we're calling RequestFullscreen() as part of a continuation of a
  // request in a subdocument in different process, whereupon the caller
  // need to send some notification itself with the real origin.
  bool mShouldNotifyNewOrigin = true;
};

} // namespace mozilla

#endif // mozilla_FullscreenRequest_h
