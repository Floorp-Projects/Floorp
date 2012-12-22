/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSPAINTREQUEST_H_
#define NSPAINTREQUEST_H_

#include "nsIDOMPaintRequest.h"
#include "nsIDOMPaintRequestList.h"
#include "nsPresContext.h"
#include "nsIDOMEvent.h"
#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"

class nsPaintRequest MOZ_FINAL : public nsIDOMPaintRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMPAINTREQUEST

  nsPaintRequest() { mRequest.mFlags = 0; }

  void SetRequest(const nsInvalidateRequestList::Request& aRequest)
  { mRequest = aRequest; }

private:
  ~nsPaintRequest() {}

  nsInvalidateRequestList::Request mRequest;
};

class nsPaintRequestList MOZ_FINAL : public nsIDOMPaintRequestList,
                                     public nsWrapperCache
{
public:
  nsPaintRequestList(nsIDOMEvent *aParent) : mParent(aParent)
  {
    SetIsDOMBinding();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsPaintRequestList)
  NS_DECL_NSIDOMPAINTREQUESTLIST
  
  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);
  nsISupports* GetParentObject()
  {
    return mParent;
  }

  void Append(nsPaintRequest* aElement)
  {
    mArray.AppendElement(aElement);
  }

  static nsPaintRequestList* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMPaintRequestList> list_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMClientRectList pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(list_qi == static_cast<nsIDOMPaintRequestList*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<nsPaintRequestList*>(aSupports);
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
