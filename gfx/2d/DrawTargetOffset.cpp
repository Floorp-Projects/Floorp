/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetOffset.h"
#include "Logging.h"
#include "PathHelpers.h"

using namespace std;

namespace mozilla {
namespace gfx {

DrawTargetOffset::DrawTargetOffset() {}

bool DrawTargetOffset::Init(DrawTarget* aDrawTarget, IntPoint aOrigin) {
  mDrawTarget = aDrawTarget;
  mOrigin = aOrigin;
  mDrawTarget->SetTransform(Matrix::Translation(-mOrigin.x, -mOrigin.y));
  mFormat = mDrawTarget->GetFormat();
  SetPermitSubpixelAA(IsOpaque(mFormat));
  return true;
}

already_AddRefed<SourceSurface> DrawTargetOffset::Snapshot() {
  return MakeAndAddRef<SourceSurfaceOffset>(mDrawTarget->Snapshot(), mOrigin);
}

void DrawTargetOffset::DetachAllSnapshots() {}

// Skip the mClippedOut check since this is only used for Flush() which
// should happen even if we're clipped.
#define OFFSET_COMMAND(command) \
  void DrawTargetOffset::command() { mDrawTarget->command(); }
#define OFFSET_COMMAND1(command, type1) \
  void DrawTargetOffset::command(type1 arg1) { mDrawTarget->command(arg1); }
#define OFFSET_COMMAND3(command, type1, type2, type3)                  \
  void DrawTargetOffset::command(type1 arg1, type2 arg2, type3 arg3) { \
    mDrawTarget->command(arg1, arg2, arg3);                            \
  }
#define OFFSET_COMMAND4(command, type1, type2, type3, type4)         \
  void DrawTargetOffset::command(type1 arg1, type2 arg2, type3 arg3, \
                                 type4 arg4) {                       \
    mDrawTarget->command(arg1, arg2, arg3, arg4);                    \
  }
#define OFFSET_COMMAND5(command, type1, type2, type3, type4, type5)  \
  void DrawTargetOffset::command(type1 arg1, type2 arg2, type3 arg3, \
                                 type4 arg4, type5 arg5) {           \
    mDrawTarget->command(arg1, arg2, arg3, arg4, arg5);              \
  }

OFFSET_COMMAND(Flush)
OFFSET_COMMAND1(ClearRect, const Rect&)
OFFSET_COMMAND4(MaskSurface, const Pattern&, SourceSurface*, Point,
                const DrawOptions&)
OFFSET_COMMAND4(FillGlyphs, ScaledFont*, const GlyphBuffer&, const Pattern&,
                const DrawOptions&)
OFFSET_COMMAND3(Mask, const Pattern&, const Pattern&, const DrawOptions&)

void DrawTargetOffset::DrawFilter(FilterNode* aNode, const Rect& aSourceRect,
                                  const Point& aDestPoint,
                                  const DrawOptions& aOptions) {
  auto clone = mTransform;
  bool invertible = clone.Invert();
  // aSourceRect is in filter space. The filter outputs from aSourceRect need
  // to be drawn at aDestPoint in user space.
  Rect userSpaceSource = Rect(aDestPoint, aSourceRect.Size());
  if (invertible) {
    // Try to reduce the source rect so that it's not much bigger
    // than the draw target. The result is not minimal. Examples
    // are left as an exercise for the reader.
    auto destRect = Rect(mOrigin.x, mOrigin.y, mDrawTarget->GetSize().width,
                         mDrawTarget->GetSize().height);
    Rect userSpaceBounds = clone.TransformBounds(destRect);
    userSpaceSource = userSpaceSource.Intersect(userSpaceBounds);
  }

  // Compute how much we moved the top-left of the source rect by, and use that
  // to compute the new dest point, and move our intersected source rect back
  // into the (new) filter space.
  Point shift = userSpaceSource.TopLeft() - aDestPoint;
  Rect filterSpaceSource =
      Rect(aSourceRect.TopLeft() + shift, userSpaceSource.Size());
  mDrawTarget->DrawFilter(aNode, filterSpaceSource, aDestPoint + shift,
                          aOptions);
}

void DrawTargetOffset::PushClip(const Path* aPath) {
  mDrawTarget->PushClip(aPath);
}

void DrawTargetOffset::PushClipRect(const Rect& aRect) {
  mDrawTarget->PushClipRect(aRect);
}

void DrawTargetOffset::PopClip() { mDrawTarget->PopClip(); }

void DrawTargetOffset::CopySurface(SourceSurface* aSurface,
                                   const IntRect& aSourceRect,
                                   const IntPoint& aDestination) {
  IntPoint tileOrigin = mOrigin;
  // CopySurface ignores the transform, account for that here.
  mDrawTarget->CopySurface(aSurface, aSourceRect, aDestination - tileOrigin);
}

void DrawTargetOffset::SetTransform(const Matrix& aTransform) {
  Matrix mat = aTransform;
  mat.PostTranslate(Float(-mOrigin.x), Float(-mOrigin.y));
  mDrawTarget->SetTransform(mat);

  DrawTarget::SetTransform(aTransform);
}

void DrawTargetOffset::SetPermitSubpixelAA(bool aPermitSubpixelAA) {
  mDrawTarget->SetPermitSubpixelAA(aPermitSubpixelAA);
}

void DrawTargetOffset::DrawSurface(SourceSurface* aSurface, const Rect& aDest,
                                   const Rect& aSource,
                                   const DrawSurfaceOptions& aSurfaceOptions,
                                   const DrawOptions& aDrawOptions) {
  mDrawTarget->DrawSurface(aSurface, aDest, aSource, aSurfaceOptions,
                           aDrawOptions);
}

void DrawTargetOffset::FillRect(const Rect& aRect, const Pattern& aPattern,
                                const DrawOptions& aDrawOptions) {
  mDrawTarget->FillRect(aRect, aPattern, aDrawOptions);
}

void DrawTargetOffset::Stroke(const Path* aPath, const Pattern& aPattern,
                              const StrokeOptions& aStrokeOptions,
                              const DrawOptions& aDrawOptions) {
  mDrawTarget->Stroke(aPath, aPattern, aStrokeOptions, aDrawOptions);
}

void DrawTargetOffset::StrokeRect(const Rect& aRect, const Pattern& aPattern,
                                  const StrokeOptions& aStrokeOptions,
                                  const DrawOptions& aDrawOptions) {
  mDrawTarget->StrokeRect(aRect, aPattern, aStrokeOptions, aDrawOptions);
}

void DrawTargetOffset::StrokeLine(const Point& aStart, const Point& aEnd,
                                  const Pattern& aPattern,
                                  const StrokeOptions& aStrokeOptions,
                                  const DrawOptions& aDrawOptions) {
  mDrawTarget->StrokeLine(aStart, aEnd, aPattern, aStrokeOptions, aDrawOptions);
}

void DrawTargetOffset::Fill(const Path* aPath, const Pattern& aPattern,
                            const DrawOptions& aDrawOptions) {
  mDrawTarget->Fill(aPath, aPattern, aDrawOptions);
}

void DrawTargetOffset::PushLayer(bool aOpaque, Float aOpacity,
                                 SourceSurface* aMask,
                                 const Matrix& aMaskTransform,
                                 const IntRect& aBounds, bool aCopyBackground) {
  IntRect bounds = aBounds - mOrigin;

  mDrawTarget->PushLayer(aOpaque, aOpacity, aMask, aMaskTransform, bounds,
                         aCopyBackground);
  SetPermitSubpixelAA(mDrawTarget->GetPermitSubpixelAA());
}

void DrawTargetOffset::PushLayerWithBlend(bool aOpaque, Float aOpacity,
                                          SourceSurface* aMask,
                                          const Matrix& aMaskTransform,
                                          const IntRect& aBounds,
                                          bool aCopyBackground,
                                          CompositionOp aOp) {
  IntRect bounds = aBounds - mOrigin;

  mDrawTarget->PushLayerWithBlend(aOpaque, aOpacity, aMask, aMaskTransform,
                                  bounds, aCopyBackground, aOp);
  SetPermitSubpixelAA(mDrawTarget->GetPermitSubpixelAA());
}

void DrawTargetOffset::PopLayer() {
  mDrawTarget->PopLayer();
  SetPermitSubpixelAA(mDrawTarget->GetPermitSubpixelAA());
}

}  // namespace gfx
}  // namespace mozilla
