/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_webkitcssmatrix_h__
#define mozilla_dom_webkitcssmatrix_h__

#include "mozilla/dom/DOMMatrix.h"

namespace mozilla {
namespace dom {

class WebKitCSSMatrix final : public DOMMatrix
{
public:
  explicit WebKitCSSMatrix(nsISupports* aParent)
    : DOMMatrix(aParent)
  {}

  WebKitCSSMatrix(nsISupports* aParent, const DOMMatrixReadOnly& other)
    : DOMMatrix(aParent, other)
  {}

  static bool FeatureEnabled(JSContext* aCx, JSObject* aObj);

  static already_AddRefed<WebKitCSSMatrix>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);
  static already_AddRefed<WebKitCSSMatrix>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aTransformList, ErrorResult& aRv);
  static already_AddRefed<WebKitCSSMatrix>
  Constructor(const GlobalObject& aGlobal,
              const DOMMatrixReadOnly& aOther, ErrorResult& aRv);

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  WebKitCSSMatrix* SetMatrixValue(const nsAString& aTransformList,
                                  ErrorResult& aRv);

  already_AddRefed<WebKitCSSMatrix> Multiply(const WebKitCSSMatrix& aOther) const;
  already_AddRefed<WebKitCSSMatrix> Inverse(ErrorResult& aRv) const;
  already_AddRefed<WebKitCSSMatrix> Translate(double aTx,
                                              double aTy,
                                              double aTz) const;
  already_AddRefed<WebKitCSSMatrix> Scale(double aScaleX,
                                          const Optional<double>& aScaleY,
                                          double aScaleZ) const;
  already_AddRefed<WebKitCSSMatrix> Rotate(double aRotX,
                                           const Optional<double>& aRotY,
                                           const Optional<double>& aRotZ) const;
  already_AddRefed<WebKitCSSMatrix> RotateAxisAngle(double aX,
                                                    double aY,
                                                    double aZ,
                                                    double aAngle) const;
  already_AddRefed<WebKitCSSMatrix> SkewX(double aSx) const;
  already_AddRefed<WebKitCSSMatrix> SkewY(double aSy) const;
protected:
  WebKitCSSMatrix* Rotate3dSelf(double aRotX,
                                double aRotY,
                                double aRotZ);

  WebKitCSSMatrix* InvertSelfThrow(ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_webkitcssmatrix_h__ */
