/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMRECT_H_
#define MOZILLA_DOMRECT_H_

#include <algorithm>
#include <cstdint>
#include <new>
#include <utility>
#include "js/TypeDecls.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class JSObject;
class nsIGlobalObject;
struct JSContext;
struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;
struct nsRect;

namespace mozilla::dom {

class GlobalObject;
struct DOMRectInit;

class DOMRectReadOnly : public nsISupports, public nsWrapperCache {
 protected:
  virtual ~DOMRectReadOnly() = default;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(DOMRectReadOnly)

  explicit DOMRectReadOnly(nsISupports* aParent, double aX = 0, double aY = 0,
                           double aWidth = 0, double aHeight = 0)
      : mParent(aParent), mX(aX), mY(aY), mWidth(aWidth), mHeight(aHeight) {}

  nsISupports* GetParentObject() const {
    MOZ_ASSERT(mParent);
    return mParent;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<DOMRectReadOnly> FromRect(const GlobalObject& aGlobal,
                                                    const DOMRectInit& aInit);

  static already_AddRefed<DOMRectReadOnly> Constructor(
      const GlobalObject& aGlobal, double aX, double aY, double aWidth,
      double aHeight);

  double X() const { return mX; }
  double Y() const { return mY; }
  double Width() const { return mWidth; }
  double Height() const { return mHeight; }

  double Left() const {
    double x = X(), w = Width();
    return NaNSafeMin(x, x + w);
  }
  double Top() const {
    double y = Y(), h = Height();
    return NaNSafeMin(y, y + h);
  }
  double Right() const {
    double x = X(), w = Width();
    return NaNSafeMax(x, x + w);
  }
  double Bottom() const {
    double y = Y(), h = Height();
    return NaNSafeMax(y, y + h);
  }

  bool WriteStructuredClone(JSContext* aCx,
                            JSStructuredCloneWriter* aWriter) const;

  static already_AddRefed<DOMRectReadOnly> ReadStructuredClone(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      JSStructuredCloneReader* aReader);

 protected:
  // Shared implementation of ReadStructuredClone for DOMRect and
  // DOMRectReadOnly.
  bool ReadStructuredClone(JSStructuredCloneReader* aReader);

  nsCOMPtr<nsISupports> mParent;
  double mX, mY, mWidth, mHeight;
};

class DOMRect final : public DOMRectReadOnly {
 public:
  explicit DOMRect(nsISupports* aParent, double aX = 0, double aY = 0,
                   double aWidth = 0, double aHeight = 0)
      : DOMRectReadOnly(aParent, aX, aY, aWidth, aHeight) {}

  NS_INLINE_DECL_REFCOUNTING_INHERITED(DOMRect, DOMRectReadOnly)

  static already_AddRefed<DOMRect> FromRect(const GlobalObject& aGlobal,
                                            const DOMRectInit& aInit);

  static already_AddRefed<DOMRect> Constructor(const GlobalObject& aGlobal,
                                               double aX, double aY,
                                               double aWidth, double aHeight);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<DOMRect> ReadStructuredClone(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      JSStructuredCloneReader* aReader);
  using DOMRectReadOnly::ReadStructuredClone;

  void SetRect(float aX, float aY, float aWidth, float aHeight) {
    mX = aX;
    mY = aY;
    mWidth = aWidth;
    mHeight = aHeight;
  }
  void SetLayoutRect(const nsRect& aLayoutRect);

  void SetX(double aX) { mX = aX; }
  void SetY(double aY) { mY = aY; }
  void SetWidth(double aWidth) { mWidth = aWidth; }
  void SetHeight(double aHeight) { mHeight = aHeight; }

  static DOMRect* FromSupports(nsISupports* aSupports) {
    return static_cast<DOMRect*>(aSupports);
  }

 private:
  ~DOMRect() = default;
};

class DOMRectList final : public nsISupports, public nsWrapperCache {
  ~DOMRectList() = default;

 public:
  explicit DOMRectList(nsISupports* aParent) : mParent(aParent) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(DOMRectList)

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() { return mParent; }

  void Append(DOMRect* aElement) { mArray.AppendElement(aElement); }

  uint32_t Length() { return mArray.Length(); }
  DOMRect* Item(uint32_t aIndex) { return mArray.SafeElementAt(aIndex); }
  DOMRect* IndexedGetter(uint32_t aIndex, bool& aFound) {
    aFound = aIndex < mArray.Length();
    if (!aFound) {
      return nullptr;
    }
    return mArray[aIndex];
  }

 protected:
  nsTArray<RefPtr<DOMRect> > mArray;
  nsCOMPtr<nsISupports> mParent;
};

}  // namespace mozilla::dom

#endif /*MOZILLA_DOMRECT_H_*/
