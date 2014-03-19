/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMRECT_H_
#define MOZILLA_DOMRECT_H_

#include "nsIDOMClientRect.h"
#include "nsIDOMClientRectList.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

struct nsRect;

namespace mozilla {
namespace dom {

class DOMRect MOZ_FINAL : public nsIDOMClientRect
                        , public nsWrapperCache
{
public:
  DOMRect(nsISupports* aParent)
    : mParent(aParent), mX(0.0), mY(0.0), mWidth(0.0), mHeight(0.0)
  {
    SetIsDOMBinding();
  }
  virtual ~DOMRect() {}
  
  void SetRect(float aX, float aY, float aWidth, float aHeight) {
    mX = aX; mY = aY; mWidth = aWidth; mHeight = aHeight;
  }
  void SetLayoutRect(const nsRect& aLayoutRect);


  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMRect)
  NS_DECL_NSIDOMCLIENTRECT


  nsISupports* GetParentObject() const
  {
    MOZ_ASSERT(mParent);
    return mParent;
  }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;


  float Left() const
  {
    return mX;
  }

  float Top() const
  {
    return mY;
  }

  float Right() const
  {
    return mX + mWidth;
  }

  float Bottom() const
  {
    return mY + mHeight;
  }

  float Width() const
  {
    return mWidth;
  }

  float Height() const
  {
    return mHeight;
  }

protected:
  nsCOMPtr<nsISupports> mParent;
  float mX, mY, mWidth, mHeight;
};

class DOMRectList MOZ_FINAL : public nsIDOMClientRectList,
                              public nsWrapperCache
{
public:
  DOMRectList(nsISupports *aParent) : mParent(aParent)
  {
    SetIsDOMBinding();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMRectList)

  NS_DECL_NSIDOMCLIENTRECTLIST
  
  virtual JSObject* WrapObject(JSContext *cx,
                               JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

  nsISupports* GetParentObject()
  {
    return mParent;
  }

  void Append(DOMRect* aElement) { mArray.AppendElement(aElement); }

  static DOMRectList* FromSupports(nsISupports* aSupports)
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

    return static_cast<DOMRectList*>(aSupports);
  }

  uint32_t Length()
  {
    return mArray.Length();
  }
  DOMRect* Item(uint32_t aIndex)
  {
    return mArray.SafeElementAt(aIndex);
  }
  DOMRect* IndexedGetter(uint32_t aIndex, bool& aFound)
  {
    aFound = aIndex < mArray.Length();
    if (!aFound) {
      return nullptr;
    }
    return mArray[aIndex];
  }

protected:
  virtual ~DOMRectList() {}

  nsTArray< nsRefPtr<DOMRect> > mArray;
  nsCOMPtr<nsISupports> mParent;
};

}
}

#endif /*MOZILLA_DOMRECT_H_*/
