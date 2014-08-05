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
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include <algorithm>

struct nsRect;

namespace mozilla {
namespace dom {

class DOMRectReadOnly : public nsISupports
                      , public nsWrapperCache
{
protected:
  virtual ~DOMRectReadOnly() {}

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMRectReadOnly)

  explicit DOMRectReadOnly(nsISupports* aParent)
    : mParent(aParent)
  {
    SetIsDOMBinding();
  }

  nsISupports* GetParentObject() const
  {
    MOZ_ASSERT(mParent);
    return mParent;
  }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual double X() const = 0;
  virtual double Y() const = 0;
  virtual double Width() const = 0;
  virtual double Height() const = 0;

  double Left() const
  {
    double x = X(), w = Width();
    return std::min(x, x + w);
  }
  double Top() const
  {
    double y = Y(), h = Height();
    return std::min(y, y + h);
  }
  double Right() const
  {
    double x = X(), w = Width();
    return std::max(x, x + w);
  }
  double Bottom() const
  {
    double y = Y(), h = Height();
    return std::max(y, y + h);
  }

protected:
  nsCOMPtr<nsISupports> mParent;
};

class DOMRect MOZ_FINAL : public DOMRectReadOnly
                        , public nsIDOMClientRect
{
public:
  explicit DOMRect(nsISupports* aParent, double aX = 0, double aY = 0,
                   double aWidth = 0, double aHeight = 0)
    : DOMRectReadOnly(aParent)
    , mX(aX)
    , mY(aY)
    , mWidth(aWidth)
    , mHeight(aHeight)
  {
  }
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMCLIENTRECT

  static already_AddRefed<DOMRect>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRV);
  static already_AddRefed<DOMRect>
  Constructor(const GlobalObject& aGlobal, double aX, double aY,
              double aWidth, double aHeight, ErrorResult& aRV);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  void SetRect(float aX, float aY, float aWidth, float aHeight) {
    mX = aX; mY = aY; mWidth = aWidth; mHeight = aHeight;
  }
  void SetLayoutRect(const nsRect& aLayoutRect);

  virtual double X() const MOZ_OVERRIDE
  {
    return mX;
  }
  virtual double Y() const MOZ_OVERRIDE
  {
    return mY;
  }
  virtual double Width() const MOZ_OVERRIDE
  {
    return mWidth;
  }
  virtual double Height() const MOZ_OVERRIDE
  {
    return mHeight;
  }

  void SetX(double aX)
  {
    mX = aX;
  }
  void SetY(double aY)
  {
    mY = aY;
  }
  void SetWidth(double aWidth)
  {
    mWidth = aWidth;
  }
  void SetHeight(double aHeight)
  {
    mHeight = aHeight;
  }

protected:
  double mX, mY, mWidth, mHeight;
};

class DOMRectList MOZ_FINAL : public nsIDOMClientRectList,
                              public nsWrapperCache
{
  ~DOMRectList() {}

public:
  explicit DOMRectList(nsISupports *aParent) : mParent(aParent)
  {
    SetIsDOMBinding();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMRectList)

  NS_DECL_NSIDOMCLIENTRECTLIST
  
  virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;

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
  nsTArray<nsRefPtr<DOMRect> > mArray;
  nsCOMPtr<nsISupports> mParent;
};

}

template<>
struct HasDangerousPublicDestructor<dom::DOMRect>
{
  static const bool value = true;
};

}

#endif /*MOZILLA_DOMRECT_H_*/
