/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsCSSClipPathInstance.h"

#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGPathData.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/ShapeUtils.h"
#include "nsSVGUtils.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "nsCSSRendering.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

/* static*/
void nsCSSClipPathInstance::ApplyBasicShapeOrPathClip(
    gfxContext& aContext, nsIFrame* aFrame, const gfxMatrix& aTransform) {
  auto& clipPathStyle = aFrame->StyleSVGReset()->mClipPath;

#ifdef DEBUG
  StyleShapeSourceType type = clipPathStyle.GetType();
  MOZ_ASSERT(type == StyleShapeSourceType::Shape ||
                 type == StyleShapeSourceType::Box ||
                 type == StyleShapeSourceType::Path,
             "This is used with basic-shape, geometry-box, and path() only");
#endif

  nsCSSClipPathInstance instance(aFrame, clipPathStyle);

  aContext.NewPath();
  RefPtr<Path> path =
      instance.CreateClipPath(aContext.GetDrawTarget(), aTransform);
  aContext.SetPath(path);
  aContext.Clip();
}

/* static*/
bool nsCSSClipPathInstance::HitTestBasicShapeOrPathClip(
    nsIFrame* aFrame, const gfxPoint& aPoint) {
  auto& clipPathStyle = aFrame->StyleSVGReset()->mClipPath;
  StyleShapeSourceType type = clipPathStyle.GetType();
  MOZ_ASSERT(type != StyleShapeSourceType::None, "unexpected none value");
  // In the future nsCSSClipPathInstance may handle <clipPath> references as
  // well. For the time being return early.
  if (type == StyleShapeSourceType::URL) {
    return false;
  }

  nsCSSClipPathInstance instance(aFrame, clipPathStyle);

  RefPtr<DrawTarget> drawTarget =
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  RefPtr<Path> path = instance.CreateClipPath(
      drawTarget, nsSVGUtils::GetCSSPxToDevPxMatrix(aFrame));
  float pixelRatio = float(AppUnitsPerCSSPixel()) /
                     aFrame->PresContext()->AppUnitsPerDevPixel();
  return path->ContainsPoint(ToPoint(aPoint) * pixelRatio, Matrix());
}

/* static */
Rect nsCSSClipPathInstance::GetBoundingRectForBasicShapeOrPathClip(
    nsIFrame* aFrame, const StyleShapeSource& aClipPathStyle) {
  MOZ_ASSERT(aClipPathStyle.GetType() == StyleShapeSourceType::Shape ||
             aClipPathStyle.GetType() == StyleShapeSourceType::Box ||
             aClipPathStyle.GetType() == StyleShapeSourceType::Path);

  nsCSSClipPathInstance instance(aFrame, aClipPathStyle);

  RefPtr<DrawTarget> drawTarget =
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  RefPtr<Path> path = instance.CreateClipPath(
      drawTarget, nsSVGUtils::GetCSSPxToDevPxMatrix(aFrame));
  return path->GetBounds();
}

already_AddRefed<Path> nsCSSClipPathInstance::CreateClipPath(
    DrawTarget* aDrawTarget, const gfxMatrix& aTransform) {
  if (mClipPathStyle.GetType() == StyleShapeSourceType::Path) {
    return CreateClipPathPath(aDrawTarget);
  }

  nscoord appUnitsPerDevPixel =
      mTargetFrame->PresContext()->AppUnitsPerDevPixel();

  nsRect r = nsLayoutUtils::ComputeGeometryBox(
      mTargetFrame, mClipPathStyle.GetReferenceBox());

  gfxRect rr(r.x, r.y, r.width, r.height);
  rr.Scale(1.0 / AppUnitsPerCSSPixel());
  rr = aTransform.TransformRect(rr);
  rr.Scale(appUnitsPerDevPixel);
  rr.Round();

  r = nsRect(int(rr.x), int(rr.y), int(rr.width), int(rr.height));

  if (mClipPathStyle.GetType() == StyleShapeSourceType::Box) {
    RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
    AppendRectToPath(builder, NSRectToRect(r, appUnitsPerDevPixel), true);
    return builder->Finish();
  }

  MOZ_ASSERT(mClipPathStyle.GetType() == StyleShapeSourceType::Shape);

  r = ToAppUnits(r.ToNearestPixels(appUnitsPerDevPixel), appUnitsPerDevPixel);

  const auto& basicShape = mClipPathStyle.BasicShape();
  switch (basicShape.tag) {
    case StyleBasicShape::Tag::Circle:
      return CreateClipPathCircle(aDrawTarget, r);
    case StyleBasicShape::Tag::Ellipse:
      return CreateClipPathEllipse(aDrawTarget, r);
    case StyleBasicShape::Tag::Polygon:
      return CreateClipPathPolygon(aDrawTarget, r);
    case StyleBasicShape::Tag::Inset:
      return CreateClipPathInset(aDrawTarget, r);
      break;
    default:
      MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unexpected shape type");
  }
  // Return an empty Path:
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  return builder->Finish();
}

already_AddRefed<Path> nsCSSClipPathInstance::CreateClipPathCircle(
    DrawTarget* aDrawTarget, const nsRect& aRefBox) {
  const auto& basicShape = mClipPathStyle.BasicShape();

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();

  nsPoint center =
      ShapeUtils::ComputeCircleOrEllipseCenter(basicShape, aRefBox);
  nscoord r = ShapeUtils::ComputeCircleRadius(basicShape, center, aRefBox);
  nscoord appUnitsPerDevPixel =
      mTargetFrame->PresContext()->AppUnitsPerDevPixel();
  builder->Arc(Point(center.x, center.y) / appUnitsPerDevPixel,
               r / appUnitsPerDevPixel, 0, Float(2 * M_PI));
  builder->Close();
  return builder->Finish();
}

already_AddRefed<Path> nsCSSClipPathInstance::CreateClipPathEllipse(
    DrawTarget* aDrawTarget, const nsRect& aRefBox) {
  const auto& basicShape = mClipPathStyle.BasicShape();

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();

  nsPoint center =
      ShapeUtils::ComputeCircleOrEllipseCenter(basicShape, aRefBox);
  nsSize radii = ShapeUtils::ComputeEllipseRadii(basicShape, center, aRefBox);
  nscoord appUnitsPerDevPixel =
      mTargetFrame->PresContext()->AppUnitsPerDevPixel();
  EllipseToBezier(builder.get(),
                  Point(center.x, center.y) / appUnitsPerDevPixel,
                  Size(radii.width, radii.height) / appUnitsPerDevPixel);
  builder->Close();
  return builder->Finish();
}

already_AddRefed<Path> nsCSSClipPathInstance::CreateClipPathPolygon(
    DrawTarget* aDrawTarget, const nsRect& aRefBox) {
  const auto& basicShape = mClipPathStyle.BasicShape();
  auto fillRule = basicShape.AsPolygon().fill == StyleFillRule::Nonzero
                      ? FillRule::FILL_WINDING
                      : FillRule::FILL_EVEN_ODD;
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder(fillRule);

  nsTArray<nsPoint> vertices =
      ShapeUtils::ComputePolygonVertices(basicShape, aRefBox);
  if (vertices.IsEmpty()) {
    MOZ_ASSERT_UNREACHABLE(
        "ComputePolygonVertices() should've given us some vertices!");
  } else {
    nscoord appUnitsPerDevPixel =
        mTargetFrame->PresContext()->AppUnitsPerDevPixel();
    builder->MoveTo(NSPointToPoint(vertices[0], appUnitsPerDevPixel));
    for (size_t i = 1; i < vertices.Length(); ++i) {
      builder->LineTo(NSPointToPoint(vertices[i], appUnitsPerDevPixel));
    }
  }
  builder->Close();
  return builder->Finish();
}

already_AddRefed<Path> nsCSSClipPathInstance::CreateClipPathInset(
    DrawTarget* aDrawTarget, const nsRect& aRefBox) {
  const auto& basicShape = mClipPathStyle.BasicShape();

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();

  nscoord appUnitsPerDevPixel =
      mTargetFrame->PresContext()->AppUnitsPerDevPixel();

  nsRect insetRect = ShapeUtils::ComputeInsetRect(basicShape, aRefBox);
  const Rect insetRectPixels = NSRectToRect(insetRect, appUnitsPerDevPixel);
  nscoord appUnitsRadii[8];

  if (ShapeUtils::ComputeInsetRadii(basicShape, insetRect, aRefBox,
                                    appUnitsRadii)) {
    RectCornerRadii corners;
    nsCSSRendering::ComputePixelRadii(appUnitsRadii, appUnitsPerDevPixel,
                                      &corners);

    AppendRoundedRectToPath(builder, insetRectPixels, corners, true);
  } else {
    AppendRectToPath(builder, insetRectPixels, true);
  }
  return builder->Finish();
}

already_AddRefed<Path> nsCSSClipPathInstance::CreateClipPathPath(
    DrawTarget* aDrawTarget) {
  const StyleSVGPath& path = mClipPathStyle.Path();

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder(
      path.FillRule() == StyleFillRule::Nonzero ? FillRule::FILL_WINDING
                                                : FillRule::FILL_EVEN_ODD);
  float scale = float(AppUnitsPerCSSPixel()) /
                mTargetFrame->PresContext()->AppUnitsPerDevPixel();
  return SVGPathData::BuildPath(path.Path(), builder,
                                NS_STYLE_STROKE_LINECAP_BUTT, 0.0, scale);
}
