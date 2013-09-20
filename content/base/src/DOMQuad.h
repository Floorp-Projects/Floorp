/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsAutoPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "Units.h"

namespace mozilla {
namespace dom {

class DOMRectReadOnly;
class DOMPoint;
struct DOMPointInit;

class DOMQuad MOZ_FINAL : public nsWrapperCache
{
public:
  DOMQuad(nsISupports* aParent, CSSPoint aPoints[4]);
  DOMQuad(nsISupports* aParent);
  ~DOMQuad();

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMQuad)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMQuad)

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

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

  DOMRectReadOnly* Bounds() const;
  DOMPoint* P1() const { return mPoints[0]; }
  DOMPoint* P2() const { return mPoints[1]; }
  DOMPoint* P3() const { return mPoints[2]; }
  DOMPoint* P4() const { return mPoints[3]; }

  DOMPoint* Point(uint32_t aIndex) { return mPoints[aIndex]; }

protected:
  class QuadBounds;

  nsCOMPtr<nsISupports> mParent;
  nsRefPtr<DOMPoint> mPoints[4];
  mutable nsRefPtr<QuadBounds> mBounds; // allocated lazily
};

}
}

#endif /*MOZILLA_DOMRECT_H_*/
