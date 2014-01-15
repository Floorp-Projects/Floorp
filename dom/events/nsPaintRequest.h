/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSPAINTREQUEST_H_
#define NSPAINTREQUEST_H_

#include "nsIDOMPaintRequest.h"
#include "nsPresContext.h"
#include "nsIDOMEvent.h"
#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {
class DOMRect;
}
}

class nsPaintRequest MOZ_FINAL : public nsIDOMPaintRequest
                               , public nsWrapperCache
{
  typedef mozilla::dom::DOMRect DOMRect;

public:
  nsPaintRequest(nsIDOMEvent* aParent)
    : mParent(aParent)
  {
    mRequest.mFlags = 0;
    SetIsDOMBinding();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsPaintRequest)
  NS_DECL_NSIDOMPAINTREQUEST

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsIDOMEvent* GetParentObject() const
  {
    return mParent;
  }

  already_AddRefed<DOMRect> ClientRect();
  void GetReason(nsAString& aResult) const
  {
    aResult.AssignLiteral("repaint");
  }

  void SetRequest(const nsInvalidateRequestList::Request& aRequest)
  { mRequest = aRequest; }

private:
  ~nsPaintRequest() {}

  nsCOMPtr<nsIDOMEvent> mParent;
  nsInvalidateRequestList::Request mRequest;
};

class nsPaintRequestList MOZ_FINAL : public nsISupports,
                                     public nsWrapperCache
{
public:
  nsPaintRequestList(nsIDOMEvent *aParent) : mParent(aParent)
  {
    SetIsDOMBinding();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsPaintRequestList)
  
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
  nsISupports* GetParentObject()
  {
    return mParent;
  }

  void Append(nsPaintRequest* aElement)
  {
    mArray.AppendElement(aElement);
  }

  uint32_t Length()
  {
    return mArray.Length();
  }

  nsPaintRequest* Item(uint32_t aIndex)
  {
    return mArray.SafeElementAt(aIndex);
  }
  nsPaintRequest* IndexedGetter(uint32_t aIndex, bool& aFound)
  {
    aFound = aIndex < mArray.Length();
    return aFound ? mArray.ElementAt(aIndex) : nullptr;
  }

private:
  ~nsPaintRequestList() {}

  nsTArray< nsRefPtr<nsPaintRequest> > mArray;
  nsCOMPtr<nsIDOMEvent> mParent;
};

#endif /*NSPAINTREQUEST_H_*/
