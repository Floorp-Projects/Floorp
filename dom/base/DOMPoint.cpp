/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMPoint.h"

#include "mozilla/dom/DOMPointBinding.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsAutoPtr.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMPoint, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMPoint, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMPoint, Release)

already_AddRefed<DOMPoint>
DOMPoint::Constructor(const GlobalObject& aGlobal, const DOMPointInit& aParams,
                      ErrorResult& aRV)
{
  nsRefPtr<DOMPoint> obj =
    new DOMPoint(aGlobal.GetAsSupports(), aParams.mX, aParams.mY,
                 aParams.mZ, aParams.mW);
  return obj.forget();
}

already_AddRefed<DOMPoint>
DOMPoint::Constructor(const GlobalObject& aGlobal, double aX, double aY,
                      double aZ, double aW, ErrorResult& aRV)
{
  nsRefPtr<DOMPoint> obj =
    new DOMPoint(aGlobal.GetAsSupports(), aX, aY, aZ, aW);
  return obj.forget();
}

JSObject*
DOMPoint::WrapObject(JSContext* aCx)
{
  return DOMPointBinding::Wrap(aCx, this);
}
