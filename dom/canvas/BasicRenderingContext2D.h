/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BasicRenderingContext2D_h
#define BasicRenderingContext2D_h

#include "mozilla/dom/CanvasRenderingContext2DBinding.h"

namespace mozilla {
namespace dom {

class HTMLImageElementOrSVGImageElementOrHTMLCanvasElementOrHTMLVideoElementOrImageBitmap;
typedef HTMLImageElementOrSVGImageElementOrHTMLCanvasElementOrHTMLVideoElementOrImageBitmap
  CanvasImageSource;

/*
 * BasicRenderingContext2D
 */
class BasicRenderingContext2D
{
public:
  //
  // CanvasState
  //
  virtual void Save() = 0;
  virtual void Restore() = 0;

  //
  // CanvasTransform
  //
  virtual void Scale(double aX, double aY, mozilla::ErrorResult& aError) = 0;
  virtual void Rotate(double aAngle, mozilla::ErrorResult& aError) = 0;
  virtual void Translate(double aX,
                         double aY,
                         mozilla::ErrorResult& aError) = 0;
  virtual void Transform(double aM11,
                         double aM12,
                         double aM21,
                         double aM22,
                         double aDx,
                         double aDy,
                         mozilla::ErrorResult& aError) = 0;
  virtual void SetTransform(double aM11,
                            double aM12,
                            double aM21,
                            double aM22,
                            double aDx,
                            double aDy,
                            mozilla::ErrorResult& aError) = 0;
  virtual void ResetTransform(mozilla::ErrorResult& aError) = 0;

  //
  // CanvasCompositing
  //
  virtual double GlobalAlpha() = 0;
  virtual void SetGlobalAlpha(double aGlobalAlpha) = 0;
  virtual void GetGlobalCompositeOperation(nsAString& aOp,
                                           mozilla::ErrorResult& aError) = 0;
  virtual void SetGlobalCompositeOperation(const nsAString& aOp,
                                           mozilla::ErrorResult& aError) = 0;

  //
  // CanvasImageSmoothing
  //
  virtual bool ImageSmoothingEnabled() = 0;
  virtual void SetImageSmoothingEnabled(bool aImageSmoothingEnabled) = 0;

  //
  // CanvasFillStrokeStyles
  //
  virtual void GetStrokeStyle(
    OwningStringOrCanvasGradientOrCanvasPattern& aValue) = 0;
  virtual void SetStrokeStyle(
    const StringOrCanvasGradientOrCanvasPattern& aValue) = 0;
  virtual void GetFillStyle(
    OwningStringOrCanvasGradientOrCanvasPattern& aValue) = 0;
  virtual void SetFillStyle(
    const StringOrCanvasGradientOrCanvasPattern& aValue) = 0;
  virtual already_AddRefed<CanvasGradient> CreateLinearGradient(double aX0,
                                                                double aY0,
                                                                double aX1,
                                                                double aY1) = 0;
  virtual already_AddRefed<CanvasGradient> CreateRadialGradient(
    double aX0,
    double aY0,
    double aR0,
    double aX1,
    double aY1,
    double aR1,
    ErrorResult& aError) = 0;
  virtual already_AddRefed<CanvasPattern> CreatePattern(
    const CanvasImageSource& aElement,
    const nsAString& aRepeat,
    ErrorResult& aError) = 0;
  //
  // CanvasShadowStyles
  //
  virtual double ShadowOffsetX() = 0;
  virtual void SetShadowOffsetX(double aShadowOffsetX) = 0;
  virtual double ShadowOffsetY() = 0;
  virtual void SetShadowOffsetY(double aShadowOffsetY) = 0;
  virtual double ShadowBlur() = 0;
  virtual void SetShadowBlur(double aShadowBlur) = 0;
  virtual void GetShadowColor(nsAString& aShadowColor) = 0;
  virtual void SetShadowColor(const nsAString& aShadowColor) = 0;

  //
  // CanvasRect
  //
  virtual void ClearRect(double aX, double aY, double aW, double aH) = 0;
  virtual void FillRect(double aX, double aY, double aW, double aH) = 0;
  virtual void StrokeRect(double aX, double aY, double aW, double aH) = 0;

  //
  // CanvasDrawImage
  //
  virtual void DrawImage(const CanvasImageSource& aImage,
                         double aDx,
                         double aDy,
                         mozilla::ErrorResult& aError) = 0;
  virtual void DrawImage(const CanvasImageSource& aImage,
                         double aDx,
                         double aDy,
                         double aDw,
                         double aDh,
                         mozilla::ErrorResult& aError) = 0;
  virtual void DrawImage(const CanvasImageSource& aImage,
                         double aSx,
                         double aSy,
                         double aSw,
                         double aSh,
                         double aDx,
                         double aDy,
                         double aDw,
                         double aDh,
                         mozilla::ErrorResult& aError) = 0;

  //
  // CanvasPathDrawingStyles
  //
  virtual double LineWidth() = 0;
  virtual void SetLineWidth(double aWidth) = 0;
  virtual void GetLineCap(nsAString& aLinecapStyle) = 0;
  virtual void SetLineCap(const nsAString& aLinecapStyle) = 0;
  virtual void GetLineJoin(nsAString& aLinejoinStyle,
                           mozilla::ErrorResult& aError) = 0;
  virtual void SetLineJoin(const nsAString& aLinejoinStyle) = 0;
  virtual double MiterLimit() = 0;
  virtual void SetMiterLimit(double aMiter) = 0;
  virtual void SetLineDash(const Sequence<double>& aSegments,
                           mozilla::ErrorResult& aRv) = 0;
  virtual void GetLineDash(nsTArray<double>& aSegments) const = 0;
  virtual void SetLineDashOffset(double aOffset) = 0;
  virtual double LineDashOffset() const = 0;

  //
  // CanvasPath
  //
  virtual void ClosePath() = 0;
  virtual void MoveTo(double aX, double aY) = 0;
  virtual void LineTo(double aX, double aY) = 0;
  virtual void QuadraticCurveTo(double aCpx,
                                double aCpy,
                                double aX,
                                double aY) = 0;
  virtual void BezierCurveTo(double aCp1x,
                             double aCp1y,
                             double aCp2x,
                             double aCp2y,
                             double aX,
                             double aY) = 0;
  virtual void ArcTo(double aX1,
                     double aY1,
                     double aX2,
                     double aY2,
                     double aRadius,
                     mozilla::ErrorResult& aError) = 0;
  virtual void Rect(double aX, double aY, double aW, double aH) = 0;
  virtual void Arc(double aX,
                   double aY,
                   double aRadius,
                   double aStartAngle,
                   double aEndAngle,
                   bool aAnticlockwise,
                   mozilla::ErrorResult& aError) = 0;
  virtual void Ellipse(double aX,
                       double aY,
                       double aRadiusX,
                       double aRadiusY,
                       double aRotation,
                       double aStartAngle,
                       double aEndAngle,
                       bool aAnticlockwise,
                       ErrorResult& aError) = 0;
};

} // namespace dom
} // namespace mozilla

#endif /* BasicRenderingContext2D_h */
