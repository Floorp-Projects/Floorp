/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_MobileMessageCursorCallback_h
#define mozilla_dom_mobilemessage_MobileMessageCursorCallback_h

#include "nsIMobileMessageCursorCallback.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"

class nsICursorContinueCallback;

namespace mozilla {
namespace dom {

class DOMCursor;
class MobileMessageManager;

namespace mobilemessage {

class MobileMessageCursorCallback : public nsIMobileMessageCursorCallback
{
  friend class mozilla::dom::MobileMessageManager;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIMOBILEMESSAGECURSORCALLBACK

  NS_DECL_CYCLE_COLLECTION_CLASS(MobileMessageCursorCallback)

  MobileMessageCursorCallback()
  {
    MOZ_COUNT_CTOR(MobileMessageCursorCallback);
  }

private:
  virtual ~MobileMessageCursorCallback()
  {
    MOZ_COUNT_DTOR(MobileMessageCursorCallback);
  }

  nsRefPtr<DOMCursor> mDOMCursor;
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_MobileMessageCursorCallback_h

