/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMPOINT_H_
#define MOZILLA_DOMPOINT_H_

#include "js/StructuredClone.h"
#include "DOMMatrix.h"
#include "nsWrapperCache.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/BindingDeclarations.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class GlobalObject;
class DOMPoint;
struct DOMPointInit;
struct DOMMatrixInit;

class DOMPointReadOnly : public nsWrapperCache {
 public:
  explicit DOMPointReadOnly(nsISupports* aParent, double aX = 0.0,
                            double aY = 0.0, double aZ = 0.0, double aW = 1.0)
      : mParent(aParent), mX(aX), mY(aY), mZ(aZ), mW(aW) {}

  static already_AddRefed<DOMPointReadOnly> FromPoint(
      const GlobalObject& aGlobal, const DOMPointInit& aParams);
  static already_AddRefed<DOMPointReadOnly> Constructor(
      const GlobalObject& aGlobal, double aX, double aY, double aZ, double aW);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMPointReadOnly)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMPointReadOnly)

  double X() const { return mX; }
  double Y() const { return mY; }
  double Z() const { return mZ; }
  double W() const { return mW; }

  already_AddRefed<DOMPoint> MatrixTransform(const DOMMatrixInit& aInit,
                                             ErrorResult& aRv);

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  bool WriteStructuredClone(JSContext* aCx,
                            JSStructuredCloneWriter* aWriter) const;

  static already_AddRefed<DOMPointReadOnly> ReadStructuredClone(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      JSStructuredCloneReader* aReader);

 protected:
  virtual ~DOMPointReadOnly() = default;

  // Shared implementation of ReadStructuredClone for DOMPoint and
  // DOMPointReadOnly.
  bool ReadStructuredClone(JSStructuredCloneReader* aReader);

  nsCOMPtr<nsISupports> mParent;
  double mX, mY, mZ, mW;
};

class DOMPoint final : public DOMPointReadOnly {
 public:
  explicit DOMPoint(nsISupports* aParent, double aX = 0.0, double aY = 0.0,
                    double aZ = 0.0, double aW = 1.0)
      : DOMPointReadOnly(aParent, aX, aY, aZ, aW) {}

  static already_AddRefed<DOMPoint> FromPoint(const GlobalObject& aGlobal,
                                              const DOMPointInit& aParams);
  static already_AddRefed<DOMPoint> Constructor(const GlobalObject& aGlobal,
                                                double aX, double aY, double aZ,
                                                double aW);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<DOMPoint> ReadStructuredClone(
      JSContext* aCx, nsIGlobalObject* aGlobal,
      JSStructuredCloneReader* aReader);
  using DOMPointReadOnly::ReadStructuredClone;

  void SetX(double aX) { mX = aX; }
  void SetY(double aY) { mY = aY; }
  void SetZ(double aZ) { mZ = aZ; }
  void SetW(double aW) { mW = aW; }
};

}  // namespace dom
}  // namespace mozilla

#endif /*MOZILLA_DOMPOINT_H_*/
