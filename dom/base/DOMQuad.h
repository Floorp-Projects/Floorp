/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMQUAD_H_
#define MOZILLA_DOMQUAD_H_

#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "Units.h"

namespace mozilla {
namespace dom {

class DOMRectReadOnly;
class DOMPoint;
struct DOMQuadJSON;
struct DOMPointInit;

class DOMQuad final : public nsWrapperCache
{
  ~DOMQuad();

public:
  DOMQuad(nsISupports* aParent, CSSPoint aPoints[4]);
  explicit DOMQuad(nsISupports* aParent);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMQuad)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMQuad)

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<DOMQuad>
  Constructor(const GlobalObject& aGlobal,
              const DOMPointInit& aP1,
              const DOMPointInit& aP2,
              const DOMPointInit& aP3,
              const DOMPointInit& aP4,
              ErrorResult& aRV);
  static already_AddRefed<DOMQuad>
  Constructor(const GlobalObject& aGlobal, const DOMRectReadOnly& aRect,
              ErrorResult& aRV);

  DOMRectReadOnly* Bounds();
  already_AddRefed<DOMRectReadOnly> GetBounds() const;
  DOMPoint* P1() const { return mPoints[0]; }
  DOMPoint* P2() const { return mPoints[1]; }
  DOMPoint* P3() const { return mPoints[2]; }
  DOMPoint* P4() const { return mPoints[3]; }

  DOMPoint* Point(uint32_t aIndex) const { return mPoints[aIndex]; }

  void ToJSON(DOMQuadJSON& aInit);

protected:
  void GetHorizontalMinMax(double* aX1, double* aX2) const;
  void GetVerticalMinMax(double* aY1, double* aY2) const;

  nsCOMPtr<nsISupports> mParent;
  RefPtr<DOMPoint> mPoints[4];
  RefPtr<DOMRectReadOnly> mBounds;
};

} // namespace dom
} // namespace mozilla

#endif /*MOZILLA_DOMRECT_H_*/
