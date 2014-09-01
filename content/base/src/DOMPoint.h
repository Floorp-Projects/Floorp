/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMPOINT_H_
#define MOZILLA_DOMPOINT_H_

#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/BindingDeclarations.h"

namespace mozilla {
namespace dom {

class GlobalObject;
struct DOMPointInit;

class DOMPointReadOnly : public nsWrapperCache
{
public:
  DOMPointReadOnly(nsISupports* aParent, double aX, double aY,
                   double aZ, double aW)
    : mParent(aParent)
    , mX(aX)
    , mY(aY)
    , mZ(aZ)
    , mW(aW)
  {
    SetIsDOMBinding();
  }

  double X() const { return mX; }
  double Y() const { return mY; }
  double Z() const { return mZ; }
  double W() const { return mW; }

protected:
  nsCOMPtr<nsISupports> mParent;
  double mX, mY, mZ, mW;
};

class DOMPoint MOZ_FINAL : public DOMPointReadOnly
{
  ~DOMPoint() {}

public:
  DOMPoint(nsISupports* aParent, double aX = 0.0, double aY = 0.0,
           double aZ = 0.0, double aW = 1.0)
    : DOMPointReadOnly(aParent, aX, aY, aZ, aW)
  {}

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMPoint)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMPoint)

  static already_AddRefed<DOMPoint>
  Constructor(const GlobalObject& aGlobal, const DOMPointInit& aParams,
              ErrorResult& aRV);
  static already_AddRefed<DOMPoint>
  Constructor(const GlobalObject& aGlobal, double aX, double aY,
              double aZ, double aW, ErrorResult& aRV);

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  void SetX(double aX) { mX = aX; }
  void SetY(double aY) { mY = aY; }
  void SetZ(double aZ) { mZ = aZ; }
  void SetW(double aW) { mW = aW; }
};

}
}

#endif /*MOZILLA_DOMPOINT_H_*/
