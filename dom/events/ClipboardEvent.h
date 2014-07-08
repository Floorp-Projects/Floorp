/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ClipboardEvent_h_
#define mozilla_dom_ClipboardEvent_h_

#include "mozilla/EventForwards.h"
#include "mozilla/dom/ClipboardEventBinding.h"
#include "mozilla/dom/Event.h"
#include "nsIDOMClipboardEvent.h"

namespace mozilla {
namespace dom {
class DataTransfer;

class ClipboardEvent : public Event,
                       public nsIDOMClipboardEvent
{
public:
  ClipboardEvent(EventTarget* aOwner,
                 nsPresContext* aPresContext,
                 InternalClipboardEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMCLIPBOARDEVENT

  // Forward to base class
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return ClipboardEventBinding::Wrap(aCx, this);
  }

  static already_AddRefed<ClipboardEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const ClipboardEventInit& aParam,
              ErrorResult& aRv);

  DataTransfer* GetClipboardData();

  void InitClipboardEvent(const nsAString& aType, bool aCanBubble,
                          bool aCancelable,
                          DataTransfer* aClipboardData,
                          ErrorResult& aError);

protected:
  ~ClipboardEvent() {}
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ClipboardEvent_h_
