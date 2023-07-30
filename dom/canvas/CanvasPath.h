/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CanvasPath_h
#define CanvasPath_h

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "nsWrapperCache.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/dom/BindingDeclarations.h"

namespace mozilla {
class ErrorResult;

namespace dom {

enum class CanvasWindingRule : uint8_t;
struct DOMMatrix2DInit;

class
    UnrestrictedDoubleOrDOMPointInitOrUnrestrictedDoubleOrDOMPointInitSequence;

class CanvasPath final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(CanvasPath)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(CanvasPath)

  nsISupports* GetParentObject() { return mParent; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<CanvasPath> Constructor(const GlobalObject& aGlobal);
  static already_AddRefed<CanvasPath> Constructor(const GlobalObject& aGlobal,
                                                  CanvasPath& aCanvasPath);
  static already_AddRefed<CanvasPath> Constructor(const GlobalObject& aGlobal,
                                                  const nsAString& aPathString);

  void ClosePath();
  void MoveTo(double x, double y);
  void LineTo(double x, double y);
  void QuadraticCurveTo(double cpx, double cpy, double x, double y);
  void BezierCurveTo(double cp1x, double cp1y, double cp2x, double cp2y,
                     double x, double y);
  void ArcTo(double x1, double y1, double x2, double y2, double radius,
             ErrorResult& error);
  void Rect(double x, double y, double w, double h);
  void RoundRect(
      double aX, double aY, double aW, double aH,
      const UnrestrictedDoubleOrDOMPointInitOrUnrestrictedDoubleOrDOMPointInitSequence&
          aRadii,
      ErrorResult& aError);
  void Arc(double x, double y, double radius, double startAngle,
           double endAngle, bool anticlockwise, ErrorResult& error);
  void Ellipse(double x, double y, double radiusX, double radiusY,
               double rotation, double startAngle, double endAngle,
               bool anticlockwise, ErrorResult& error);

  void LineTo(const gfx::Point& aPoint);
  void BezierTo(const gfx::Point& aCP1, const gfx::Point& aCP2,
                const gfx::Point& aCP3);

  already_AddRefed<gfx::Path> GetPath(const CanvasWindingRule& aWinding,
                                      const gfx::DrawTarget* aTarget) const;

  explicit CanvasPath(nsISupports* aParent);
  // already_AddRefed arg because the return value from Path::CopyToBuilder()
  // is passed directly and we can't drop the only ref to have a raw pointer.
  CanvasPath(nsISupports* aParent,
             already_AddRefed<gfx::PathBuilder> aPathBuilder);

  void AddPath(CanvasPath& aCanvasPath, const DOMMatrix2DInit& aInit,
               ErrorResult& aError);

 private:
  virtual ~CanvasPath() = default;

  nsCOMPtr<nsISupports> mParent;
  static gfx::Float ToFloat(double aValue) { return gfx::Float(aValue); }

  mutable RefPtr<gfx::Path> mPath;
  mutable RefPtr<gfx::PathBuilder> mPathBuilder;

  // Whether an internal segment was zero-length.
  mutable bool mPruned = false;

  void EnsurePathBuilder() const;
  void EnsureCapped() const;
  void EnsureActive() const;
};

}  // namespace dom
}  // namespace mozilla

#endif /* CanvasPath_h */
