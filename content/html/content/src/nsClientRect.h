/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSCLIENTRECT_H_
#define NSCLIENTRECT_H_

#include "nsIDOMClientRect.h"
#include "nsIDOMClientRectList.h"
#include "nsCOMArray.h"
#include "nsRect.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"

class nsClientRect : public nsIDOMClientRect
{
public:
  NS_DECL_ISUPPORTS

  nsClientRect();
  void SetRect(float aX, float aY, float aWidth, float aHeight) {
    mX = aX; mY = aY; mWidth = aWidth; mHeight = aHeight;
  }
  virtual ~nsClientRect() {}
  
  NS_DECL_NSIDOMCLIENTRECT

  void SetLayoutRect(const nsRect& aLayoutRect);

protected:
  float mX, mY, mWidth, mHeight;
};

class nsClientRectList MOZ_FINAL : public nsIDOMClientRectList,
                                   public nsWrapperCache
{
public:
  nsClientRectList(nsISupports *aParent) : mParent(aParent)
  {
    SetIsDOMBinding();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsClientRectList)

  NS_DECL_NSIDOMCLIENTRECTLIST
  
  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);

  nsISupports* GetParentObject()
  {
    return mParent;
  }

  void Append(nsIDOMClientRect* aElement) { mArray.AppendObject(aElement); }

  static nsClientRectList* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMClientRectList> list_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMClientRectList pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(list_qi == static_cast<nsIDOMClientRectList*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<nsClientRectList*>(aSupports);
  }

protected:
  virtual ~nsClientRectList() {}

  nsCOMArray<nsIDOMClientRect> mArray;
  nsCOMPtr<nsISupports> mParent;
};

#endif /*NSCLIENTRECT_H_*/
