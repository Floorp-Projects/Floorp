/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "CSSClipPathInstance.h"

#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGPathData.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/ShapeUtils.h"
#include "mozilla/SVGUtils.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "nsCSSRendering.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;

namespace mozilla {

/* static*/
void CSSClipPathInstance::ApplyBasicShapeOrPathClip(
    gfxContext& aContext, nsIFrame* aFrame, const gfxMatrix& aTransform) {
  RefPtr<Path> path =
      CreateClipPathForFrame(aContext.GetDrawTarget(), aFrame, aTransform);
  if (!path) {
    // This behavior matches |SVGClipPathFrame::ApplyClipPath()|.
    // https://www.w3.org/TR/css-masking-1/#ClipPathElement:
    // "An empty clipping path will completely clip away the element that had
    // the clip-path property applied."
    aContext.Clip(Rect());
    return;
  }
  aContext.Clip(path);
}

/* static*/
RefPtr<Path> CSSClipPathInstance::CreateClipPathForFrame(
    gfx::DrawTarget* aDt, nsIFrame* aFrame, const gfxMatrix& aTransform) {
  const auto& clipPathStyle = aFrame->StyleSVGReset()->mClipPath;
  MOZ_ASSERT(clipPathStyle.IsShape() || clipPathStyle.IsBox() ||
                 clipPathStyle.IsPath(),
             "This is used with basic-shape, geometry-box, and path() only");

  CSSClipPathInstance instance(aFrame, clipPathStyle);

  return instance.CreateClipPath(aDt, aTransform);
}

/* static*/
bool CSSClipPathInstance::HitTestBasicShapeOrPathClip(nsIFrame* aFrame,
                                                      const gfxPoint& aPoint) {
  const auto& clipPathStyle = aFrame->StyleSVGReset()->mClipPath;
  MOZ_ASSERT(!clipPathStyle.IsNone(), "unexpected none value");
  MOZ_ASSERT(!clipPathStyle.IsUrl(), "unexpected url value");

  CSSClipPathInstance instance(aFrame, clipPathStyle);

  RefPtr<DrawTarget> drawTarget =
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  RefPtr<Path> path = instance.CreateClipPath(
      drawTarget, SVGUtils::GetCSSPxToDevPxMatrix(aFrame));
  float pixelRatio = float(AppUnitsPerCSSPixel()) /
                     aFrame->PresContext()->AppUnitsPerDevPixel();
  return path && path->ContainsPoint(ToPoint(aPoint) * pixelRatio, Matrix());
}

/* static */
Maybe<Rect> CSSClipPathInstance::GetBoundingRectForBasicShapeOrPathClip(
    nsIFrame* aFrame, const StyleClipPath& aClipPathStyle) {
  MOZ_ASSERT(aClipPathStyle.IsShape() || aClipPathStyle.IsBox() ||
             aClipPathStyle.IsPath());

  CSSClipPathInstance instance(aFrame, aClipPathStyle);

  RefPtr<DrawTarget> drawTarget =
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  RefPtr<Path> path = instance.CreateClipPath(
      drawTarget, SVGUtils::GetCSSPxToDevPxMatrix(aFrame));
  return path ? Some(path->GetBounds()) : Nothing();
}

already_AddRefed<Path> CSSClipPathInstance::CreateClipPath(
    DrawTarget* aDrawTarget, const gfxMatrix& aTransform) {
  if (mClipPathStyle.IsPath()) {
    return CreateClipPathPath(aDrawTarget);
  }

  nscoord appUnitsPerDevPixel =
      mTargetFrame->PresContext()->AppUnitsPerDevPixel();

  nsRect r;
  if (mClipPathStyle.IsBox()) {
    r = nsLayoutUtils::ComputeGeometryBox(mTargetFrame, mClipPathStyle.AsBox());
  } else {
    r = nsLayoutUtils::ComputeGeometryBox(mTargetFrame,
                                          mClipPathStyle.AsShape()._1);
  }

  gfxRect rr(r.x, r.y, r.width, r.height);
  rr.Scale(1.0 / AppUnitsPerCSSPixel());
  rr = aTransform.TransformRect(rr);
  rr.Scale(appUnitsPerDevPixel);
  rr.Round();

  r = nsRect(int(rr.x), int(rr.y), int(rr.width), int(rr.height));

  if (mClipPathStyle.IsBox()) {
    RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
    AppendRectToPath(builder, NSRectToRect(r, appUnitsPerDevPixel), true);
    return builder->Finish();
  }

  MOZ_ASSERT(mClipPathStyle.IsShape());

  r = ToAppUnits(r.ToNearestPixels(appUnitsPerDevPixel), appUnitsPerDevPixel);

  const auto& basicShape = *mClipPathStyle.AsShape()._0;
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

already_AddRefed<Path> CSSClipPathInstance::CreateClipPathCircle(
    DrawTarget* aDrawTarget, const nsRect& aRefBox) {
  const auto& basicShape = *mClipPathStyle.AsShape()._0;

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

already_AddRefed<Path> CSSClipPathInstance::CreateClipPathEllipse(
    DrawTarget* aDrawTarget, const nsRect& aRefBox) {
  const auto& basicShape = *mClipPathStyle.AsShape()._0;

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

already_AddRefed<Path> CSSClipPathInstance::CreateClipPathPolygon(
    DrawTarget* aDrawTarget, const nsRect& aRefBox) {
  const auto& basicShape = *mClipPathStyle.AsShape()._0;
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

already_AddRefed<Path> CSSClipPathInstance::CreateClipPathInset(
    DrawTarget* aDrawTarget, const nsRect& aRefBox) {
  const auto& basicShape = *mClipPathStyle.AsShape()._0;

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();

  nscoord appUnitsPerDevPixel =
      mTargetFrame->PresContext()->AppUnitsPerDevPixel();

  const nsRect insetRect = ShapeUtils::ComputeInsetRect(basicShape, aRefBox);
  const Rect insetRectPixels = NSRectToRect(insetRect, appUnitsPerDevPixel);
  nscoord appUnitsRadii[8];

  if (ShapeUtils::ComputeInsetRadii(basicShape, aRefBox, insetRect,
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

already_AddRefed<Path> CSSClipPathInstance::CreateClipPathPath(
    DrawTarget* aDrawTarget) {
  const auto& path = mClipPathStyle.AsPath();

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder(
      path.fill == StyleFillRule::Nonzero ? FillRule::FILL_WINDING
                                          : FillRule::FILL_EVEN_ODD);
  float scale = float(AppUnitsPerCSSPixel()) /
                mTargetFrame->PresContext()->AppUnitsPerDevPixel();
  return SVGPathData::BuildPath(path.path._0.AsSpan(), builder,
                                StyleStrokeLinecap::Butt, 0.0, scale);
}

}  // namespace mozilla
