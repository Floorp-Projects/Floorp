/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsCSSClipPathInstance.h"

#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/ShapeUtils.h"
#include "nsCSSRendering.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsRenderingContext.h"
#include "nsRuleNode.h"
#include "nsSVGElement.h"
#include "nsSVGUtils.h"
#include "nsSVGViewBox.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

/* static*/ void
nsCSSClipPathInstance::ApplyBasicShapeClip(gfxContext& aContext,
                                           nsIFrame* aFrame)
{
  auto& clipPathStyle = aFrame->StyleSVGReset()->mClipPath;

#ifdef DEBUG
  StyleShapeSourceType type = clipPathStyle.GetType();
  MOZ_ASSERT(type == StyleShapeSourceType::Shape ||
             type == StyleShapeSourceType::Box,
             "This function is used with basic-shape and geometry-box only.");
#endif

  nsCSSClipPathInstance instance(aFrame, clipPathStyle);

  aContext.NewPath();
  RefPtr<Path> path = instance.CreateClipPath(aContext.GetDrawTarget());
  aContext.SetPath(path);
  aContext.Clip();
}

/* static*/ bool
nsCSSClipPathInstance::HitTestBasicShapeClip(nsIFrame* aFrame,
                                             const gfxPoint& aPoint)
{
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
  RefPtr<Path> path = instance.CreateClipPath(drawTarget);
  float pixelRatio = float(nsPresContext::AppUnitsPerCSSPixel()) /
                     aFrame->PresContext()->AppUnitsPerDevPixel();
  return path->ContainsPoint(ToPoint(aPoint) * pixelRatio, Matrix());
}

already_AddRefed<Path>
nsCSSClipPathInstance::CreateClipPath(DrawTarget* aDrawTarget)
{
  nsRect r =
    nsLayoutUtils::ComputeGeometryBox(mTargetFrame,
                                      mClipPathStyle.GetReferenceBox());

  if (mClipPathStyle.GetType() != StyleShapeSourceType::Shape) {
    // TODO Clip to border-radius/reference box if no shape
    // was specified.
    RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
    return builder->Finish();
  }

  nscoord appUnitsPerDevPixel =
    mTargetFrame->PresContext()->AppUnitsPerDevPixel();
  r = ToAppUnits(r.ToNearestPixels(appUnitsPerDevPixel), appUnitsPerDevPixel);

  StyleBasicShape* basicShape = mClipPathStyle.GetBasicShape();
  switch (basicShape->GetShapeType()) {
    case StyleBasicShapeType::Circle:
      return CreateClipPathCircle(aDrawTarget, r);
    case StyleBasicShapeType::Ellipse:
      return CreateClipPathEllipse(aDrawTarget, r);
    case StyleBasicShapeType::Polygon:
      return CreateClipPathPolygon(aDrawTarget, r);
    case StyleBasicShapeType::Inset:
      return CreateClipPathInset(aDrawTarget, r);
      break;
    default:
      MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unexpected shape type");
  }
  // Return an empty Path:
  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();
  return builder->Finish();
}

already_AddRefed<Path>
nsCSSClipPathInstance::CreateClipPathCircle(DrawTarget* aDrawTarget,
                                            const nsRect& aRefBox)
{
  StyleBasicShape* basicShape = mClipPathStyle.GetBasicShape();

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();

  nsPoint center =
    ShapeUtils::ComputeCircleOrEllipseCenter(basicShape, aRefBox);
  nscoord r = ShapeUtils::ComputeCircleRadius(basicShape, center, aRefBox);
  nscoord appUnitsPerDevPixel =
    mTargetFrame->PresContext()->AppUnitsPerDevPixel();
  builder->Arc(Point(center.x, center.y) / appUnitsPerDevPixel,
               r / appUnitsPerDevPixel,
               0, Float(2 * M_PI));
  builder->Close();
  return builder->Finish();
}

already_AddRefed<Path>
nsCSSClipPathInstance::CreateClipPathEllipse(DrawTarget* aDrawTarget,
                                             const nsRect& aRefBox)
{
  StyleBasicShape* basicShape = mClipPathStyle.GetBasicShape();

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

already_AddRefed<Path>
nsCSSClipPathInstance::CreateClipPathPolygon(DrawTarget* aDrawTarget,
                                             const nsRect& aRefBox)
{
  StyleBasicShape* basicShape = mClipPathStyle.GetBasicShape();
  FillRule fillRule = basicShape->GetFillRule() == StyleFillRule::Nonzero ?
                        FillRule::FILL_WINDING : FillRule::FILL_EVEN_ODD;
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

already_AddRefed<Path>
nsCSSClipPathInstance::CreateClipPathInset(DrawTarget* aDrawTarget,
                                           const nsRect& aRefBox)
{
  StyleBasicShape* basicShape = mClipPathStyle.GetBasicShape();

  RefPtr<PathBuilder> builder = aDrawTarget->CreatePathBuilder();

  nscoord appUnitsPerDevPixel =
    mTargetFrame->PresContext()->AppUnitsPerDevPixel();

  nsRect insetRect = ShapeUtils::ComputeInsetRect(basicShape, aRefBox);
  const Rect insetRectPixels = NSRectToRect(insetRect, appUnitsPerDevPixel);
  nscoord appUnitsRadii[8];

  if (ShapeUtils::ComputeInsetRadii(basicShape, insetRect, aRefBox,
                                    appUnitsRadii)) {
    RectCornerRadii corners;
    nsCSSRendering::ComputePixelRadii(appUnitsRadii,
                                      appUnitsPerDevPixel, &corners);

    AppendRoundedRectToPath(builder, insetRectPixels, corners, true);
  } else {
    AppendRectToPath(builder, insetRectPixels, true);
  }
  return builder->Finish();
}
