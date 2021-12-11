/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "mozilla/Alignment.h"

#include "cairo.h"

#include "gfxContext.h"

#include "gfxMatrix.h"
#include "gfxUtils.h"
#include "gfxPattern.h"
#include "gfxPlatform.h"

#include "gfx2DGlue.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/ProfilerLabels.h"
#include <algorithm>
#include "TextDrawTarget.h"

#if XP_WIN
#  include "gfxWindowsPlatform.h"
#  include "mozilla/gfx/DeviceManagerDx.h"
#endif

using namespace mozilla;
using namespace mozilla::gfx;

#ifdef DEBUG
#  define CURRENTSTATE_CHANGED() CurrentState().mContentChanged = true;
#else
#  define CURRENTSTATE_CHANGED()
#endif

PatternFromState::operator mozilla::gfx::Pattern&() {
  gfxContext::AzureState& state = mContext->CurrentState();

  if (state.pattern) {
    return *state.pattern->GetPattern(
        mContext->mDT,
        state.patternTransformChanged ? &state.patternTransform : nullptr);
  }

  mPattern = new (mColorPattern.addr()) ColorPattern(state.color);
  return *mPattern;
}

gfxContext::gfxContext(DrawTarget* aTarget, const Point& aDeviceOffset)
    : mPathIsRect(false), mTransformChanged(false), mDT(aTarget) {
  if (!aTarget) {
    gfxCriticalError() << "Don't create a gfxContext without a DrawTarget";
  }

  mStateStack.SetLength(1);
  CurrentState().drawTarget = mDT;
  CurrentState().deviceOffset = aDeviceOffset;
  mDT->SetTransform(GetDTTransform());
}

/* static */
already_AddRefed<gfxContext> gfxContext::CreateOrNull(
    DrawTarget* aTarget, const mozilla::gfx::Point& aDeviceOffset) {
  if (!aTarget || !aTarget->IsValid()) {
    gfxCriticalNote << "Invalid target in gfxContext::CreateOrNull "
                    << hexa(aTarget);
    return nullptr;
  }

  RefPtr<gfxContext> result = new gfxContext(aTarget, aDeviceOffset);
  return result.forget();
}

/* static */
already_AddRefed<gfxContext> gfxContext::CreatePreservingTransformOrNull(
    DrawTarget* aTarget) {
  if (!aTarget || !aTarget->IsValid()) {
    gfxCriticalNote
        << "Invalid target in gfxContext::CreatePreservingTransformOrNull "
        << hexa(aTarget);
    return nullptr;
  }

  Matrix transform = aTarget->GetTransform();
  RefPtr<gfxContext> result = new gfxContext(aTarget);
  result->SetMatrix(transform);
  return result.forget();
}

gfxContext::~gfxContext() {
  for (int i = mStateStack.Length() - 1; i >= 0; i--) {
    for (unsigned int c = 0; c < mStateStack[i].pushedClips.Length(); c++) {
      mStateStack[i].drawTarget->PopClip();
    }
  }
}

mozilla::layout::TextDrawTarget* gfxContext::GetTextDrawer() {
  if (mDT->GetBackendType() == BackendType::WEBRENDER_TEXT) {
    return static_cast<mozilla::layout::TextDrawTarget*>(&*mDT);
  }
  return nullptr;
}

void gfxContext::Save() {
  CurrentState().transform = mTransform;
  mStateStack.AppendElement(AzureState(CurrentState()));
  CurrentState().pushedClips.Clear();
#ifdef DEBUG
  CurrentState().mContentChanged = false;
#endif
}

void gfxContext::Restore() {
#ifdef DEBUG
  // gfxContext::Restore is used to restore AzureState. We need to restore it
  // only if it was altered. The following APIs do change the content of
  // AzureState, a user should save the state before using them and restore it
  // after finishing painting:
  // 1. APIs to setup how to paint, such as SetColor()/SetAntialiasMode(). All
  //    gfxContext SetXXXX public functions belong to this category, except
  //    gfxContext::SetPath & gfxContext::SetMatrix.
  // 2. Clip functions, such as Clip() or PopClip(). You may call PopClip()
  //    directly instead of using gfxContext::Save if the clip region is the
  //    only thing that you altered in the target context.
  // 3. Function of setup transform matrix, such as Multiply() and
  //    SetMatrix(). Using gfxContextMatrixAutoSaveRestore is more recommended
  //    if transform data is the only thing that you are going to alter.
  //
  // You will hit the assertion message below if there is no above functions
  // been used between a pair of gfxContext::Save and gfxContext::Restore.
  // Considerate to remove that pair of Save/Restore if hitting that assertion.
  //
  // In the other hand, the following APIs do not alter the content of the
  // current AzureState, therefore, there is no need to save & restore
  // AzureState:
  // 1. constant member functions of gfxContext.
  // 2. Paint calls, such as Line()/Rectangle()/Fill(). Those APIs change the
  //    content of drawing buffer, which is not part of AzureState.
  // 3. Path building APIs, such as SetPath()/MoveTo()/LineTo()/NewPath().
  //    Surprisingly, path information is not stored in AzureState either.
  // Save current AzureState before using these type of APIs does nothing but
  // make performance worse.
  NS_ASSERTION(
      CurrentState().mContentChanged || CurrentState().pushedClips.Length() > 0,
      "The context of the current AzureState is not altered after "
      "Save() been called. you may consider to remove this pair of "
      "gfxContext::Save/Restore.");
#endif

  for (unsigned int c = 0; c < CurrentState().pushedClips.Length(); c++) {
    mDT->PopClip();
  }

  mStateStack.RemoveLastElement();

  mDT = CurrentState().drawTarget;

  ChangeTransform(CurrentState().transform, false);
}

// drawing
void gfxContext::NewPath() {
  mPath = nullptr;
  mPathBuilder = nullptr;
  mPathIsRect = false;
  mTransformChanged = false;
}

void gfxContext::ClosePath() {
  EnsurePathBuilder();
  mPathBuilder->Close();
}

already_AddRefed<Path> gfxContext::GetPath() {
  EnsurePath();
  RefPtr<Path> path(mPath);
  return path.forget();
}

void gfxContext::SetPath(Path* path) {
  MOZ_ASSERT(path->GetBackendType() == mDT->GetBackendType() ||
             path->GetBackendType() == BackendType::RECORDING ||
             (mDT->GetBackendType() == BackendType::DIRECT2D1_1 &&
              path->GetBackendType() == BackendType::DIRECT2D));
  mPath = path;
  mPathBuilder = nullptr;
  mPathIsRect = false;
  mTransformChanged = false;
}

void gfxContext::Fill() { Fill(PatternFromState(this)); }

void gfxContext::Fill(const Pattern& aPattern) {
  AUTO_PROFILER_LABEL("gfxContext::Fill", GRAPHICS);
  AzureState& state = CurrentState();

  CompositionOp op = GetOp();

  if (mPathIsRect) {
    MOZ_ASSERT(!mTransformChanged);

    if (op == CompositionOp::OP_SOURCE) {
      // Emulate cairo operator source which is bound by mask!
      mDT->ClearRect(mRect);
      mDT->FillRect(mRect, aPattern, DrawOptions(1.0f));
    } else {
      mDT->FillRect(mRect, aPattern, DrawOptions(1.0f, op, state.aaMode));
    }
  } else {
    EnsurePath();
    mDT->Fill(mPath, aPattern, DrawOptions(1.0f, op, state.aaMode));
  }
}

void gfxContext::MoveTo(const gfxPoint& pt) {
  EnsurePathBuilder();
  mPathBuilder->MoveTo(ToPoint(pt));
}

void gfxContext::LineTo(const gfxPoint& pt) {
  EnsurePathBuilder();
  mPathBuilder->LineTo(ToPoint(pt));
}

void gfxContext::Line(const gfxPoint& start, const gfxPoint& end) {
  EnsurePathBuilder();
  mPathBuilder->MoveTo(ToPoint(start));
  mPathBuilder->LineTo(ToPoint(end));
}

// XXX snapToPixels is only valid when snapping for filled
// rectangles and for even-width stroked rectangles.
// For odd-width stroked rectangles, we need to offset x/y by
// 0.5...
void gfxContext::Rectangle(const gfxRect& rect, bool snapToPixels) {
  Rect rec = ToRect(rect);

  if (snapToPixels) {
    gfxRect newRect(rect);
    if (UserToDevicePixelSnapped(newRect, SnapOption::IgnoreScale)) {
      gfxMatrix mat = ThebesMatrix(mTransform);
      if (mat.Invert()) {
        // We need the user space rect.
        rec = ToRect(mat.TransformBounds(newRect));
      } else {
        rec = Rect();
      }
    }
  }

  if (!mPathBuilder && !mPathIsRect) {
    mPathIsRect = true;
    mRect = rec;
    return;
  }

  EnsurePathBuilder();

  mPathBuilder->MoveTo(rec.TopLeft());
  mPathBuilder->LineTo(rec.TopRight());
  mPathBuilder->LineTo(rec.BottomRight());
  mPathBuilder->LineTo(rec.BottomLeft());
  mPathBuilder->Close();
}

void gfxContext::SnappedClip(const gfxRect& rect) {
  Rect rec = ToRect(rect);

  gfxRect newRect(rect);
  if (UserToDevicePixelSnapped(newRect, SnapOption::IgnoreScale)) {
    gfxMatrix mat = ThebesMatrix(mTransform);
    if (mat.Invert()) {
      // We need the user space rect.
      rec = ToRect(mat.TransformBounds(newRect));
    } else {
      rec = Rect();
    }
  }

  Clip(rec);
}

// transform stuff
void gfxContext::Multiply(const gfxMatrix& matrix) {
  Multiply(ToMatrix(matrix));
}

// transform stuff
void gfxContext::Multiply(const Matrix& matrix) {
  CURRENTSTATE_CHANGED()
  ChangeTransform(matrix * mTransform);
}

void gfxContext::SetMatrix(const gfx::Matrix& matrix) {
  CURRENTSTATE_CHANGED()
  ChangeTransform(matrix);
}

void gfxContext::SetMatrixDouble(const gfxMatrix& matrix) {
  SetMatrix(ToMatrix(matrix));
}

gfx::Matrix gfxContext::CurrentMatrix() const { return mTransform; }

gfxMatrix gfxContext::CurrentMatrixDouble() const {
  return ThebesMatrix(CurrentMatrix());
}

gfxPoint gfxContext::DeviceToUser(const gfxPoint& point) const {
  return ThebesPoint(mTransform.Inverse().TransformPoint(ToPoint(point)));
}

Size gfxContext::DeviceToUser(const Size& size) const {
  return mTransform.Inverse().TransformSize(size);
}

gfxRect gfxContext::DeviceToUser(const gfxRect& rect) const {
  return ThebesRect(mTransform.Inverse().TransformBounds(ToRect(rect)));
}

gfxPoint gfxContext::UserToDevice(const gfxPoint& point) const {
  return ThebesPoint(mTransform.TransformPoint(ToPoint(point)));
}

Size gfxContext::UserToDevice(const Size& size) const {
  const Matrix& matrix = mTransform;

  Size newSize;
  newSize.width = size.width * matrix._11 + size.height * matrix._12;
  newSize.height = size.width * matrix._21 + size.height * matrix._22;
  return newSize;
}

gfxRect gfxContext::UserToDevice(const gfxRect& rect) const {
  const Matrix& matrix = mTransform;
  return ThebesRect(matrix.TransformBounds(ToRect(rect)));
}

bool gfxContext::UserToDevicePixelSnapped(gfxRect& rect,
                                          SnapOptions aOptions) const {
  if (mDT->GetUserData(&sDisablePixelSnapping)) {
    return false;
  }

  // if we're not at 1.0 scale, don't snap, unless we're
  // ignoring the scale.  If we're not -just- a scale,
  // never snap.
  const gfxFloat epsilon = 0.0000001;
#define WITHIN_E(a, b) (fabs((a) - (b)) < epsilon)
  Matrix mat = mTransform;
  if (!aOptions.contains(SnapOption::IgnoreScale) &&
      (!WITHIN_E(mat._11, 1.0) || !WITHIN_E(mat._22, 1.0) ||
       !WITHIN_E(mat._12, 0.0) || !WITHIN_E(mat._21, 0.0))) {
    return false;
  }
#undef WITHIN_E

  gfxPoint p1 = UserToDevice(rect.TopLeft());
  gfxPoint p2 = UserToDevice(rect.TopRight());
  gfxPoint p3 = UserToDevice(rect.BottomRight());

  // Check that the rectangle is axis-aligned. For an axis-aligned rectangle,
  // two opposite corners define the entire rectangle. So check if
  // the axis-aligned rectangle with opposite corners p1 and p3
  // define an axis-aligned rectangle whose other corners are p2 and p4.
  // We actually only need to check one of p2 and p4, since an affine
  // transform maps parallelograms to parallelograms.
  if (!(p2 == gfxPoint(p1.x, p3.y) || p2 == gfxPoint(p3.x, p1.y))) {
    return false;
  }

  if (aOptions.contains(SnapOption::PrioritizeSize)) {
    // Snap the dimensions of the rect, to minimize distortion; only after that
    // will we snap its position. In particular, this guarantees that a square
    // remains square after snapping, which may not be the case if each edge is
    // independently snapped to device pixels.

    // Use the same rounding approach as gfx::BasePoint::Round.
    rect.SizeTo(std::floor(rect.width + 0.5), std::floor(rect.height + 0.5));

    // Find the top-left corner based on the original center and the snapped
    // size, then snap this new corner to the grid.
    gfxPoint center = (p1 + p3) / 2;
    gfxPoint topLeft = center - gfxPoint(rect.width / 2.0, rect.height / 2.0);
    topLeft.Round();
    rect.MoveTo(topLeft);
  } else {
    p1.Round();
    p3.Round();
    rect.MoveTo(gfxPoint(std::min(p1.x, p3.x), std::min(p1.y, p3.y)));
    rect.SizeTo(gfxSize(std::max(p1.x, p3.x) - rect.X(),
                        std::max(p1.y, p3.y) - rect.Y()));
  }

  return true;
}

bool gfxContext::UserToDevicePixelSnapped(gfxPoint& pt,
                                          bool ignoreScale) const {
  if (mDT->GetUserData(&sDisablePixelSnapping)) {
    return false;
  }

  // if we're not at 1.0 scale, don't snap, unless we're
  // ignoring the scale.  If we're not -just- a scale,
  // never snap.
  const gfxFloat epsilon = 0.0000001;
#define WITHIN_E(a, b) (fabs((a) - (b)) < epsilon)
  Matrix mat = mTransform;
  if (!ignoreScale && (!WITHIN_E(mat._11, 1.0) || !WITHIN_E(mat._22, 1.0) ||
                       !WITHIN_E(mat._12, 0.0) || !WITHIN_E(mat._21, 0.0))) {
    return false;
  }
#undef WITHIN_E

  pt = UserToDevice(pt);
  pt.Round();
  return true;
}

void gfxContext::SetAntialiasMode(AntialiasMode mode) {
  CURRENTSTATE_CHANGED()
  CurrentState().aaMode = mode;
}

AntialiasMode gfxContext::CurrentAntialiasMode() const {
  return CurrentState().aaMode;
}

void gfxContext::SetDash(const Float* dashes, int ndash, Float offset) {
  CURRENTSTATE_CHANGED()
  AzureState& state = CurrentState();

  state.dashPattern.SetLength(ndash);
  for (int i = 0; i < ndash; i++) {
    state.dashPattern[i] = dashes[i];
  }
  state.strokeOptions.mDashLength = ndash;
  state.strokeOptions.mDashOffset = offset;
  state.strokeOptions.mDashPattern =
      ndash ? state.dashPattern.Elements() : nullptr;
}

bool gfxContext::CurrentDash(FallibleTArray<Float>& dashes,
                             Float* offset) const {
  const AzureState& state = CurrentState();
  int count = state.strokeOptions.mDashLength;

  if (count <= 0 || !dashes.Assign(state.dashPattern, fallible)) {
    return false;
  }

  *offset = state.strokeOptions.mDashOffset;

  return true;
}

void gfxContext::SetLineWidth(Float width) {
  CurrentState().strokeOptions.mLineWidth = width;
}

Float gfxContext::CurrentLineWidth() const {
  return CurrentState().strokeOptions.mLineWidth;
}

void gfxContext::SetOp(CompositionOp aOp) {
  CURRENTSTATE_CHANGED()
  CurrentState().op = aOp;
}

CompositionOp gfxContext::CurrentOp() const { return CurrentState().op; }

void gfxContext::SetLineCap(CapStyle cap) {
  CURRENTSTATE_CHANGED()
  CurrentState().strokeOptions.mLineCap = cap;
}

CapStyle gfxContext::CurrentLineCap() const {
  return CurrentState().strokeOptions.mLineCap;
}

void gfxContext::SetLineJoin(JoinStyle join) {
  CURRENTSTATE_CHANGED()
  CurrentState().strokeOptions.mLineJoin = join;
}

JoinStyle gfxContext::CurrentLineJoin() const {
  return CurrentState().strokeOptions.mLineJoin;
}

void gfxContext::SetMiterLimit(Float limit) {
  CURRENTSTATE_CHANGED()
  CurrentState().strokeOptions.mMiterLimit = limit;
}

Float gfxContext::CurrentMiterLimit() const {
  return CurrentState().strokeOptions.mMiterLimit;
}

// clipping
void gfxContext::Clip(const Rect& rect) {
  AzureState::PushedClip clip = {nullptr, rect, mTransform};
  CurrentState().pushedClips.AppendElement(clip);
  mDT->PushClipRect(rect);
  NewPath();
}

void gfxContext::Clip(const gfxRect& rect) { Clip(ToRect(rect)); }

void gfxContext::Clip(Path* aPath) {
  mDT->PushClip(aPath);
  AzureState::PushedClip clip = {aPath, Rect(), mTransform};
  CurrentState().pushedClips.AppendElement(clip);
}

void gfxContext::Clip() {
  if (mPathIsRect) {
    MOZ_ASSERT(!mTransformChanged);

    AzureState::PushedClip clip = {nullptr, mRect, mTransform};
    CurrentState().pushedClips.AppendElement(clip);
    mDT->PushClipRect(mRect);
  } else {
    EnsurePath();
    mDT->PushClip(mPath);
    AzureState::PushedClip clip = {mPath, Rect(), mTransform};
    CurrentState().pushedClips.AppendElement(clip);
  }
}

void gfxContext::PopClip() {
  MOZ_ASSERT(CurrentState().pushedClips.Length() > 0);

  CurrentState().pushedClips.RemoveLastElement();
  mDT->PopClip();
}

gfxRect gfxContext::GetClipExtents(ClipExtentsSpace aSpace) const {
  Rect rect = GetAzureDeviceSpaceClipBounds();

  if (rect.IsZeroArea()) {
    return gfxRect(0, 0, 0, 0);
  }

  if (aSpace == eUserSpace) {
    Matrix mat = mTransform;
    mat.Invert();
    rect = mat.TransformBounds(rect);
  }

  return ThebesRect(rect);
}

bool gfxContext::ExportClip(ClipExporter& aExporter) {
  for (unsigned int i = 0; i < mStateStack.Length(); i++) {
    for (unsigned int c = 0; c < mStateStack[i].pushedClips.Length(); c++) {
      AzureState::PushedClip& clip = mStateStack[i].pushedClips[c];
      gfx::Matrix transform = clip.transform;
      transform.PostTranslate(-GetDeviceOffset());

      aExporter.BeginClip(transform);
      if (clip.path) {
        clip.path->StreamToSink(&aExporter);
      } else {
        aExporter.MoveTo(clip.rect.TopLeft());
        aExporter.LineTo(clip.rect.TopRight());
        aExporter.LineTo(clip.rect.BottomRight());
        aExporter.LineTo(clip.rect.BottomLeft());
        aExporter.Close();
      }
      aExporter.EndClip();
    }
  }

  return true;
}

bool gfxContext::ClipContainsRect(const gfxRect& aRect) {
  // Since we always return false when the clip list contains a
  // non-rectangular clip or a non-rectilinear transform, our 'total' clip
  // is always a rectangle if we hit the end of this function.
  Rect clipBounds(0, 0, Float(mDT->GetSize().width),
                  Float(mDT->GetSize().height));

  for (unsigned int i = 0; i < mStateStack.Length(); i++) {
    for (unsigned int c = 0; c < mStateStack[i].pushedClips.Length(); c++) {
      AzureState::PushedClip& clip = mStateStack[i].pushedClips[c];
      if (clip.path || !clip.transform.IsRectilinear()) {
        // Cairo behavior is we return false if the clip contains a non-
        // rectangle.
        return false;
      } else {
        Rect clipRect = mTransform.TransformBounds(clip.rect);

        clipBounds.IntersectRect(clipBounds, clipRect);
      }
    }
  }

  return clipBounds.Contains(ToRect(aRect));
}

// rendering sources

void gfxContext::SetColor(const sRGBColor& aColor) {
  CURRENTSTATE_CHANGED()
  CurrentState().pattern = nullptr;
  CurrentState().color = ToDeviceColor(aColor);
}

void gfxContext::SetDeviceColor(const DeviceColor& aColor) {
  CURRENTSTATE_CHANGED()
  CurrentState().pattern = nullptr;
  CurrentState().color = aColor;
}

bool gfxContext::GetDeviceColor(DeviceColor& aColorOut) {
  if (CurrentState().pattern) {
    return CurrentState().pattern->GetSolidColor(aColorOut);
  }

  aColorOut = CurrentState().color;
  return true;
}

void gfxContext::SetPattern(gfxPattern* pattern) {
  CURRENTSTATE_CHANGED()
  CurrentState().patternTransformChanged = false;
  CurrentState().pattern = pattern;
}

already_AddRefed<gfxPattern> gfxContext::GetPattern() {
  RefPtr<gfxPattern> pat;

  AzureState& state = CurrentState();
  if (state.pattern) {
    pat = state.pattern;
  } else {
    pat = new gfxPattern(state.color);
  }
  return pat.forget();
}

// masking
void gfxContext::Mask(SourceSurface* aSurface, Float aAlpha,
                      const Matrix& aTransform) {
  Matrix old = mTransform;
  Matrix mat = aTransform * mTransform;

  ChangeTransform(mat);
  mDT->MaskSurface(
      PatternFromState(this), aSurface, Point(),
      DrawOptions(aAlpha, CurrentState().op, CurrentState().aaMode));
  ChangeTransform(old);
}

void gfxContext::Mask(SourceSurface* surface, float alpha,
                      const Point& offset) {
  // We clip here to bind to the mask surface bounds, see above.
  mDT->MaskSurface(
      PatternFromState(this), surface, offset,
      DrawOptions(alpha, CurrentState().op, CurrentState().aaMode));
}

void gfxContext::Paint(Float alpha) {
  AUTO_PROFILER_LABEL("gfxContext::Paint", GRAPHICS);

  Matrix mat = mDT->GetTransform();
  mat.Invert();
  Rect paintRect = mat.TransformBounds(Rect(Point(0, 0), Size(mDT->GetSize())));

  mDT->FillRect(paintRect, PatternFromState(this), DrawOptions(alpha, GetOp()));
}

void gfxContext::PushGroupForBlendBack(gfxContentType content, Float aOpacity,
                                       SourceSurface* aMask,
                                       const Matrix& aMaskTransform) {
  mDT->PushLayer(content == gfxContentType::COLOR, aOpacity, aMask,
                 aMaskTransform);
}

void gfxContext::PopGroupAndBlend() { mDT->PopLayer(); }

#ifdef MOZ_DUMP_PAINTING
void gfxContext::WriteAsPNG(const char* aFile) {
  gfxUtils::WriteAsPNG(mDT, aFile);
}

void gfxContext::DumpAsDataURI() { gfxUtils::DumpAsDataURI(mDT); }

void gfxContext::CopyAsDataURI() { gfxUtils::CopyAsDataURI(mDT); }
#endif

void gfxContext::EnsurePath() {
  if (mPathBuilder) {
    mPath = mPathBuilder->Finish();
    mPathBuilder = nullptr;
  }

  if (mPath) {
    if (mTransformChanged) {
      Matrix mat = mTransform;
      mat.Invert();
      mat = mPathTransform * mat;
      mPathBuilder = mPath->TransformedCopyToBuilder(mat);
      mPath = mPathBuilder->Finish();
      mPathBuilder = nullptr;

      mTransformChanged = false;
    }
    return;
  }

  EnsurePathBuilder();
  mPath = mPathBuilder->Finish();
  mPathBuilder = nullptr;
}

void gfxContext::EnsurePathBuilder() {
  if (mPathBuilder && !mTransformChanged) {
    return;
  }

  if (mPath) {
    if (!mTransformChanged) {
      mPathBuilder = mPath->CopyToBuilder();
      mPath = nullptr;
    } else {
      Matrix invTransform = mTransform;
      invTransform.Invert();
      Matrix toNewUS = mPathTransform * invTransform;
      mPathBuilder = mPath->TransformedCopyToBuilder(toNewUS);
    }
    return;
  }

  DebugOnly<PathBuilder*> oldPath = mPathBuilder.get();

  if (!mPathBuilder) {
    mPathBuilder = mDT->CreatePathBuilder(FillRule::FILL_WINDING);

    if (mPathIsRect) {
      mPathBuilder->MoveTo(mRect.TopLeft());
      mPathBuilder->LineTo(mRect.TopRight());
      mPathBuilder->LineTo(mRect.BottomRight());
      mPathBuilder->LineTo(mRect.BottomLeft());
      mPathBuilder->Close();
    }
  }

  if (mTransformChanged) {
    // This could be an else if since this should never happen when
    // mPathBuilder is nullptr and mPath is nullptr. But this way we can
    // assert if all the state is as expected.
    MOZ_ASSERT(oldPath);
    MOZ_ASSERT(!mPathIsRect);

    Matrix invTransform = mTransform;
    invTransform.Invert();
    Matrix toNewUS = mPathTransform * invTransform;

    RefPtr<Path> path = mPathBuilder->Finish();
    if (!path) {
      gfxCriticalError()
          << "gfxContext::EnsurePathBuilder failed in PathBuilder::Finish";
    }
    mPathBuilder = path->TransformedCopyToBuilder(toNewUS);
  }

  mPathIsRect = false;
}

CompositionOp gfxContext::GetOp() {
  if (CurrentState().op != CompositionOp::OP_SOURCE) {
    return CurrentState().op;
  }

  AzureState& state = CurrentState();
  if (state.pattern) {
    if (state.pattern->IsOpaque()) {
      return CompositionOp::OP_OVER;
    } else {
      return CompositionOp::OP_SOURCE;
    }
  } else {
    if (state.color.a > 0.999) {
      return CompositionOp::OP_OVER;
    } else {
      return CompositionOp::OP_SOURCE;
    }
  }
}

/* SVG font code can change the transform after having set the pattern on the
 * context. When the pattern is set it is in user space, if the transform is
 * changed after doing so the pattern needs to be converted back into userspace.
 * We just store the old pattern transform here so that we only do the work
 * needed here if the pattern is actually used.
 * We need to avoid doing this when this ChangeTransform comes from a restore,
 * since the current pattern and the current transform are both part of the
 * state we know the new CurrentState()'s values are valid. But if we assume
 * a change they might become invalid since patternTransformChanged is part of
 * the state and might be false for the restored AzureState.
 */
void gfxContext::ChangeTransform(const Matrix& aNewMatrix,
                                 bool aUpdatePatternTransform) {
  AzureState& state = CurrentState();

  if (aUpdatePatternTransform && (state.pattern) &&
      !state.patternTransformChanged) {
    state.patternTransform = GetDTTransform();
    state.patternTransformChanged = true;
  }

  if (mPathIsRect) {
    Matrix invMatrix = aNewMatrix;

    invMatrix.Invert();

    Matrix toNewUS = mTransform * invMatrix;

    if (toNewUS.IsRectilinear()) {
      mRect = toNewUS.TransformBounds(mRect);
      mRect.NudgeToIntegers();
    } else {
      mPathBuilder = mDT->CreatePathBuilder(FillRule::FILL_WINDING);

      mPathBuilder->MoveTo(toNewUS.TransformPoint(mRect.TopLeft()));
      mPathBuilder->LineTo(toNewUS.TransformPoint(mRect.TopRight()));
      mPathBuilder->LineTo(toNewUS.TransformPoint(mRect.BottomRight()));
      mPathBuilder->LineTo(toNewUS.TransformPoint(mRect.BottomLeft()));
      mPathBuilder->Close();

      mPathIsRect = false;
    }

    // No need to consider the transform changed now!
    mTransformChanged = false;
  } else if ((mPath || mPathBuilder) && !mTransformChanged) {
    mTransformChanged = true;
    mPathTransform = mTransform;
  }

  mTransform = aNewMatrix;

  mDT->SetTransform(GetDTTransform());
}

Rect gfxContext::GetAzureDeviceSpaceClipBounds() const {
  Rect rect(CurrentState().deviceOffset.x + Float(mDT->GetRect().x),
            CurrentState().deviceOffset.y + Float(mDT->GetRect().y),
            Float(mDT->GetSize().width), Float(mDT->GetSize().height));
  for (unsigned int i = 0; i < mStateStack.Length(); i++) {
    for (unsigned int c = 0; c < mStateStack[i].pushedClips.Length(); c++) {
      const AzureState::PushedClip& clip = mStateStack[i].pushedClips[c];
      if (clip.path) {
        Rect bounds = clip.path->GetBounds(clip.transform);
        rect.IntersectRect(rect, bounds);
      } else {
        rect.IntersectRect(rect, clip.transform.TransformBounds(clip.rect));
      }
    }
  }

  return rect;
}

Point gfxContext::GetDeviceOffset() const {
  return CurrentState().deviceOffset;
}

void gfxContext::SetDeviceOffset(const Point& aOffset) {
  CurrentState().deviceOffset = aOffset;
}

Matrix gfxContext::GetDTTransform() const {
  Matrix mat = mTransform;
  mat._31 -= CurrentState().deviceOffset.x;
  mat._32 -= CurrentState().deviceOffset.y;
  return mat;
}
