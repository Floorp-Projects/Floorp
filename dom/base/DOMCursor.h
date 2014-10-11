/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_domcursor_h__
#define mozilla_dom_domcursor_h__

#include "nsIDOMDOMCursor.h"
#include "DOMRequest.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {

class DOMCursor : public DOMRequest
                , public nsIDOMDOMCursor
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMDOMCURSOR
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DOMCursor,
                                           DOMRequest)

  DOMCursor(nsPIDOMWindow* aWindow, nsICursorContinueCallback *aCallback);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  bool Done() const
  {
    return mFinished;
  }
  virtual void Continue(ErrorResult& aRv);

  void Reset();
  void FireDone();

protected:
  ~DOMCursor() {}

private:
  DOMCursor() MOZ_DELETE;
  // Calling Then() on DOMCursor is a mistake, since the DOMCursor object
  // should not have a .then() method from JS' point of view.
  already_AddRefed<mozilla::dom::Promise>
  Then(JSContext* aCx, AnyCallback* aResolveCallback,
       AnyCallback* aRejectCallback, ErrorResult& aRv) MOZ_DELETE;

  nsCOMPtr<nsICursorContinueCallback> mCallback;
  bool mFinished;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_domcursor_h__ */
