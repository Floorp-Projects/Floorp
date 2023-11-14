/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfx2DGlue.h"
#include "gfxPattern.h"
#include "gfxUtils.h"
#include "nsTArray.h"

#include "mozilla/EnumSet.h"
#include "mozilla/gfx/2D.h"

typedef struct _cairo cairo_t;
class GlyphBufferAzure;

namespace mozilla {
namespace gfx {
struct RectCornerRadii;
}  // namespace gfx
namespace layout {
class TextDrawTarget;
}  // namespace layout
}  // namespace mozilla

class ClipExporter;

/* This class lives on the stack and allows gfxContext users to easily, and
 * performantly get a gfx::Pattern to use for drawing in their current context.
 */
class PatternFromState {
 public:
  explicit PatternFromState(const gfxContext* aContext)
      : mContext(aContext), mPattern(nullptr) {}
  ~PatternFromState() {
    if (mPattern) {
      mPattern->~Pattern();
    }
  }

  operator mozilla::gfx::Pattern&();

 private:
  mozilla::AlignedStorage2<mozilla::gfx::ColorPattern> mColorPattern;

  const gfxContext* mContext;
  mozilla::gfx::Pattern* mPattern;
};

/**
 * This is the main class for doing actual drawing. It is initialized using
 * a surface and can be drawn on. It manages various state information like
 * a current transformation matrix (CTM), a current path, current color,
 * etc.
 *
 * All drawing happens by creating a path and then stroking or filling it.
 * The functions like Rectangle and Arc do not do any drawing themselves.
 * When a path is drawn (stroked or filled), it is filled/stroked with a
 * pattern set by SetPattern or SetColor.
 *
 * Note that the gfxContext takes coordinates in device pixels,
 * as opposed to app units.
 */
class gfxContext final {
#ifdef DEBUG
#  define CURRENTSTATE_CHANGED() mAzureState.mContentChanged = true;
#else
#  define CURRENTSTATE_CHANGED()
#endif

  typedef mozilla::gfx::BackendType BackendType;
  typedef mozilla::gfx::CapStyle CapStyle;
  typedef mozilla::gfx::CompositionOp CompositionOp;
  typedef mozilla::gfx::DeviceColor DeviceColor;
  typedef mozilla::gfx::DrawOptions DrawOptions;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::JoinStyle JoinStyle;
  typedef mozilla::gfx::FillRule FillRule;
  typedef mozilla::gfx::Float Float;
  typedef mozilla::gfx::Matrix Matrix;
  typedef mozilla::gfx::Path Path;
  typedef mozilla::gfx::Pattern Pattern;
  typedef mozilla::gfx::Point Point;
  typedef mozilla::gfx::Rect Rect;
  typedef mozilla::gfx::RectCornerRadii RectCornerRadii;
  typedef mozilla::gfx::Size Size;

 public:
  /**
   * Initialize this context from a DrawTarget, which must be non-null.
   * Strips any transform from aTarget, unless aPreserveTransform is true.
   * aTarget will be flushed in the gfxContext's destructor.
   */
  MOZ_NONNULL(2)
  explicit gfxContext(DrawTarget* aTarget, const Point& aDeviceOffset = Point())
      : mDT(aTarget) {
    mAzureState.deviceOffset = aDeviceOffset;
    mDT->SetTransform(GetDTTransform());
  }

  MOZ_NONNULL(2)
  gfxContext(DrawTarget* aTarget, bool aPreserveTransform) : mDT(aTarget) {
    if (aPreserveTransform) {
      SetMatrix(aTarget->GetTransform());
    } else {
      mDT->SetTransform(GetDTTransform());
    }
  }

  ~gfxContext();

  /**
   * Initialize this context from a DrawTarget.
   * Strips any transform from aTarget.
   * aTarget will be flushed in the gfxContext's destructor.
   * If aTarget is null or invalid, nullptr is returned.  The caller
   * is responsible for handling this scenario as appropriate.
   */
  static mozilla::UniquePtr<gfxContext> CreateOrNull(DrawTarget* aTarget);

  DrawTarget* GetDrawTarget() const { return mDT; }

  /**
   * Returns the DrawTarget if it's actually a TextDrawTarget.
   */
  mozilla::layout::TextDrawTarget* GetTextDrawer() const;

  /**
   ** State
   **/
  // XXX document exactly what bits are saved
  void Save();
  void Restore();

  /**
   ** Paths & Drawing
   **/

  /**
   * Fill the current path according to the current settings.
   *
   * Does not consume the current path.
   */
  void Fill() { Fill(PatternFromState(this)); }
  void Fill(const Pattern& aPattern);

  /**
   * Forgets the current path.
   */
  void NewPath() {
    mPath = nullptr;
    mPathBuilder = nullptr;
    mPathIsRect = false;
    mTransformChanged = false;
  }

  /**
   * Returns the current path.
   */
  already_AddRefed<Path> GetPath() {
    EnsurePath();
    RefPtr<Path> path(mPath);
    return path.forget();
  }

  /**
   * Sets the given path as the current path.
   */
  void SetPath(Path* path) {
    MOZ_ASSERT(path->GetBackendType() == mDT->GetBackendType() ||
               path->GetBackendType() == BackendType::RECORDING ||
               (mDT->GetBackendType() == BackendType::DIRECT2D1_1 &&
                path->GetBackendType() == BackendType::DIRECT2D));
    mPath = path;
    mPathBuilder = nullptr;
    mPathIsRect = false;
    mTransformChanged = false;
  }

  /**
   * Draws the rectangle given by rect.
   */
  void Rectangle(const gfxRect& rect) { return Rectangle(rect, false); }
  void SnappedRectangle(const gfxRect& rect) { return Rectangle(rect, true); }

 private:
  void Rectangle(const gfxRect& rect, bool snapToPixels);

 public:
  /**
   ** Transformation Matrix manipulation
   **/

  /**
   * Post-multiplies 'other' onto the current CTM, i.e. this
   * matrix's transformation will take place before the previously set
   * transformations.
   */
  void Multiply(const gfxMatrix& aMatrix) { Multiply(ToMatrix(aMatrix)); }
  void Multiply(const Matrix& aOther) {
    CURRENTSTATE_CHANGED()
    ChangeTransform(aOther * mAzureState.transform);
  }

  /**
   * Replaces the current transformation matrix with matrix.
   */
  void SetMatrix(const Matrix& aMatrix) {
    CURRENTSTATE_CHANGED()
    ChangeTransform(aMatrix);
  }
  void SetMatrixDouble(const gfxMatrix& aMatrix) {
    SetMatrix(ToMatrix(aMatrix));
  }

  void SetCrossProcessPaintScale(float aScale) {
    MOZ_ASSERT(mCrossProcessPaintScale == 1.0f,
               "Should only be initialized once");
    mCrossProcessPaintScale = aScale;
  }

  float GetCrossProcessPaintScale() const { return mCrossProcessPaintScale; }

  /**
   * Returns the current transformation matrix.
   */
  Matrix CurrentMatrix() const { return mAzureState.transform; }
  gfxMatrix CurrentMatrixDouble() const {
    return ThebesMatrix(CurrentMatrix());
  }

  /**
   * Converts a point from device to user coordinates using the inverse
   * transformation matrix.
   */
  gfxPoint DeviceToUser(const gfxPoint& aPoint) const {
    return ThebesPoint(
        mAzureState.transform.Inverse().TransformPoint(ToPoint(aPoint)));
  }

  /**
   * Converts a size from device to user coordinates. This does not apply
   * translation components of the matrix.
   */
  Size DeviceToUser(const Size& aSize) const {
    return mAzureState.transform.Inverse().TransformSize(aSize);
  }

  /**
   * Converts a rectangle from device to user coordinates; this has the
   * same effect as using DeviceToUser on both the rectangle's point and
   * size.
   */
  gfxRect DeviceToUser(const gfxRect& aRect) const {
    return ThebesRect(
        mAzureState.transform.Inverse().TransformBounds(ToRect(aRect)));
  }

  /**
   * Converts a point from user to device coordinates using the transformation
   * matrix.
   */
  gfxPoint UserToDevice(const gfxPoint& aPoint) const {
    return ThebesPoint(mAzureState.transform.TransformPoint(ToPoint(aPoint)));
  }

  /**
   * Converts a size from user to device coordinates. This does not apply
   * translation components of the matrix.
   */
  Size UserToDevice(const Size& aSize) const {
    const auto& mtx = mAzureState.transform;
    return Size(aSize.width * mtx._11 + aSize.height * mtx._12,
                aSize.width * mtx._21 + aSize.height * mtx._22);
  }

  /**
   * Converts a rectangle from user to device coordinates.  The
   * resulting rectangle is the minimum device-space rectangle that
   * encloses the user-space rectangle given.
   */
  gfxRect UserToDevice(const gfxRect& rect) const {
    return ThebesRect(mAzureState.transform.TransformBounds(ToRect(rect)));
  }

  /**
   * Takes the given rect and tries to align it to device pixels.  If
   * this succeeds, the method will return true, and the rect will
   * be in device coordinates (already transformed by the CTM).  If it
   * fails, the method will return false, and the rect will not be
   * changed.
   *
   * aOptions parameter:
   *   If IgnoreScale is set, then snapping will take place even if the CTM
   *   has a scale applied. Snapping never takes place if there is a rotation
   *   in the CTM.
   *
   *   If PrioritizeSize is set, the rect's dimensions will first be snapped
   *   and then its position aligned to device pixels, rather than snapping
   *   the position of each edge independently.
   */
  enum class SnapOption : uint8_t {
    IgnoreScale = 1,
    PrioritizeSize = 2,
  };
  using SnapOptions = mozilla::EnumSet<SnapOption>;
  bool UserToDevicePixelSnapped(gfxRect& rect, SnapOptions aOptions = {}) const;

  /**
   * Takes the given point and tries to align it to device pixels.  If
   * this succeeds, the method will return true, and the point will
   * be in device coordinates (already transformed by the CTM).  If it
   * fails, the method will return false, and the point will not be
   * changed.
   *
   * If ignoreScale is true, then snapping will take place even if
   * the CTM has a scale applied.  Snapping never takes place if
   * there is a rotation in the CTM.
   */
  bool UserToDevicePixelSnapped(gfxPoint& pt, bool ignoreScale = false) const;

  /**
   ** Painting sources
   **/

  /**
   * Set a solid color to use for drawing.  This color is in the device color
   * space and is not transformed.
   */
  void SetDeviceColor(const DeviceColor& aColor) {
    CURRENTSTATE_CHANGED()
    mAzureState.pattern = nullptr;
    mAzureState.color = aColor;
  }

  /**
   * Gets the current color.  It's returned in the device color space.
   * returns false if there is something other than a color
   *         set as the current source (pattern, surface, etc)
   */
  bool GetDeviceColor(DeviceColor& aColorOut) const;

  /**
   * Returns true if color is neither opaque nor transparent (i.e. alpha is not
   * 0 or 1), and false otherwise. If true, aColorOut is set on output.
   */
  bool HasNonOpaqueNonTransparentColor(DeviceColor& aColorOut) const {
    return GetDeviceColor(aColorOut) && 0.f < aColorOut.a && aColorOut.a < 1.f;
  }

  /**
   * Set a solid color in the sRGB color space to use for drawing.
   * If CMS is not enabled, the color is treated as a device-space color
   * and this call is identical to SetDeviceColor().
   */
  void SetColor(const mozilla::gfx::sRGBColor& aColor) {
    CURRENTSTATE_CHANGED()
    mAzureState.pattern = nullptr;
    mAzureState.color = ToDeviceColor(aColor);
  }

  /**
   * Uses a pattern for drawing.
   */
  void SetPattern(gfxPattern* pattern) {
    CURRENTSTATE_CHANGED()
    mAzureState.patternTransformChanged = false;
    mAzureState.pattern = pattern;
  }

  /**
   * Get the source pattern (solid color, normal pattern, surface, etc)
   */
  already_AddRefed<gfxPattern> GetPattern() const;

  /**
   ** Painting
   **/
  /**
   * Paints the current source surface/pattern everywhere in the current
   * clip region.
   */
  void Paint(Float alpha = 1.0) const;

  /**
   ** Line Properties
   **/

  // Set the dash pattern, applying devPxScale to convert passed-in lengths
  // to device pixels (used by the SVGUtils::SetupStrokeGeometry caller,
  // which has the desired dash pattern in CSS px).
  void SetDash(const Float* dashes, int ndash, Float offset, Float devPxScale);

  // Return true if dashing is set, false if it's not enabled or the
  // context is in an error state.  |offset| can be nullptr to mean
  // "don't care".
  bool CurrentDash(FallibleTArray<Float>& dashes, Float* offset) const;

  /**
   * Sets the line width that's used for line drawing.
   */
  void SetLineWidth(Float width) {
    CURRENTSTATE_CHANGED()
    mAzureState.strokeOptions.mLineWidth = width;
  }

  /**
   * Returns the currently set line width.
   *
   * @see SetLineWidth
   */
  Float CurrentLineWidth() const {
    return mAzureState.strokeOptions.mLineWidth;
  }

  /**
   * Sets the line caps, i.e. how line endings are drawn.
   */
  void SetLineCap(CapStyle cap) {
    CURRENTSTATE_CHANGED()
    mAzureState.strokeOptions.mLineCap = cap;
  }
  CapStyle CurrentLineCap() const { return mAzureState.strokeOptions.mLineCap; }

  /**
   * Sets the line join, i.e. how the connection between two lines is
   * drawn.
   */
  void SetLineJoin(JoinStyle join) {
    CURRENTSTATE_CHANGED()
    mAzureState.strokeOptions.mLineJoin = join;
  }
  JoinStyle CurrentLineJoin() const {
    return mAzureState.strokeOptions.mLineJoin;
  }

  void SetMiterLimit(Float limit) {
    CURRENTSTATE_CHANGED()
    mAzureState.strokeOptions.mMiterLimit = limit;
  }
  Float CurrentMiterLimit() const {
    return mAzureState.strokeOptions.mMiterLimit;
  }

  /**
   * Sets the operator used for all further drawing. The operator affects
   * how drawing something will modify the destination. For example, the
   * OVER operator will do alpha blending of source and destination, while
   * SOURCE will replace the destination with the source.
   */
  void SetOp(CompositionOp aOp) {
    CURRENTSTATE_CHANGED()
    mAzureState.op = aOp;
  }
  CompositionOp CurrentOp() const { return mAzureState.op; }

  void SetAntialiasMode(mozilla::gfx::AntialiasMode aMode) {
    CURRENTSTATE_CHANGED()
    mAzureState.aaMode = aMode;
  }
  mozilla::gfx::AntialiasMode CurrentAntialiasMode() const {
    return mAzureState.aaMode;
  }

  /**
   ** Clipping
   **/

  /**
   * Clips all further drawing to the current path.
   * This does not consume the current path.
   */
  void Clip();

  /**
   * Helper functions that will create a rect path and call Clip().
   * Any current path will be destroyed by these functions!
   */
  void Clip(const gfxRect& aRect) { Clip(ToRect(aRect)); }
  void Clip(const Rect& rect);            // will clip to a rect
  void SnappedClip(const gfxRect& rect);  // snap rect and clip to the result
  void Clip(Path* aPath);

  void PopClip() {
    MOZ_ASSERT(!mAzureState.pushedClips.IsEmpty());
    mAzureState.pushedClips.RemoveLastElement();
    mDT->PopClip();
  }

  enum ClipExtentsSpace {
    eUserSpace = 0,
    eDeviceSpace = 1,
  };

  /**
   * According to aSpace, this function will return the current bounds of
   * the clip region in user space or device space.
   */
  gfxRect GetClipExtents(ClipExtentsSpace aSpace = eUserSpace) const;

  /**
   * Exports the current clip using the provided exporter.
   */
  bool ExportClip(ClipExporter& aExporter) const;

  /**
   * Groups
   */
  void PushGroupForBlendBack(gfxContentType content, Float aOpacity = 1.0f,
                             mozilla::gfx::SourceSurface* aMask = nullptr,
                             const Matrix& aMaskTransform = Matrix()) const {
    mDT->PushLayer(content == gfxContentType::COLOR, aOpacity, aMask,
                   aMaskTransform);
  }

  void PopGroupAndBlend() const { mDT->PopLayer(); }

  Point GetDeviceOffset() const { return mAzureState.deviceOffset; }
  void SetDeviceOffset(const Point& aOffset) {
    mAzureState.deviceOffset = aOffset;
  }

#ifdef MOZ_DUMP_PAINTING
  /**
   * Debug functions to encode the current surface as a PNG and export it.
   */

  /**
   * Writes a binary PNG file.
   */
  void WriteAsPNG(const char* aFile);

  /**
   * Write as a PNG encoded Data URL to stdout.
   */
  void DumpAsDataURI();

  /**
   * Copy a PNG encoded Data URL to the clipboard.
   */
  void CopyAsDataURI();
#endif

 private:
  friend class PatternFromState;
  friend class GlyphBufferAzure;

  typedef mozilla::gfx::sRGBColor sRGBColor;
  typedef mozilla::gfx::StrokeOptions StrokeOptions;
  typedef mozilla::gfx::PathBuilder PathBuilder;
  typedef mozilla::gfx::SourceSurface SourceSurface;

  struct AzureState {
    AzureState()
        : op(CompositionOp::OP_OVER),
          color(0, 0, 0, 1.0f),
          aaMode(mozilla::gfx::AntialiasMode::SUBPIXEL),
          patternTransformChanged(false)
#ifdef DEBUG
          ,
          mContentChanged(false)
#endif
    {
    }

    CompositionOp op;
    DeviceColor color;
    RefPtr<gfxPattern> pattern;
    Matrix transform;
    struct PushedClip {
      RefPtr<Path> path;
      Rect rect;
      Matrix transform;
    };
    CopyableTArray<PushedClip> pushedClips;
    CopyableTArray<Float> dashPattern;
    StrokeOptions strokeOptions;
    mozilla::gfx::AntialiasMode aaMode;
    bool patternTransformChanged;
    Matrix patternTransform;
    // This is used solely for using minimal intermediate surface size.
    Point deviceOffset;
#ifdef DEBUG
    // Whether the content of this AzureState changed after construction.
    bool mContentChanged;
#endif
  };

  // This ensures mPath contains a valid path (in user space!)
  void EnsurePath();
  // This ensures mPathBuilder contains a valid PathBuilder (in user space!)
  void EnsurePathBuilder();
  CompositionOp GetOp() const;
  void ChangeTransform(const Matrix& aNewMatrix,
                       bool aUpdatePatternTransform = true);
  Rect GetAzureDeviceSpaceClipBounds() const;
  Matrix GetDTTransform() const {
    Matrix mat = mAzureState.transform;
    mat.PostTranslate(-mAzureState.deviceOffset);
    return mat;
  }

  bool mPathIsRect = false;
  bool mTransformChanged = false;
  Matrix mPathTransform;
  Rect mRect;
  RefPtr<PathBuilder> mPathBuilder;
  RefPtr<Path> mPath;
  AzureState mAzureState;
  nsTArray<AzureState> mSavedStates;

  // Iterate over all clips in the saved and current states, calling aLambda
  // with each of them.
  template <typename F>
  void ForAllClips(F&& aLambda) const;

  const AzureState& CurrentState() const { return mAzureState; }

  RefPtr<DrawTarget> const mDT;
  float mCrossProcessPaintScale = 1.0f;

#ifdef DEBUG
#  undef CURRENTSTATE_CHANGED
#endif
};

/**
 * Sentry helper class for functions with multiple return points that need to
 * call Save() on a gfxContext and have Restore() called automatically on the
 * gfxContext before they return.
 */
class MOZ_STACK_CLASS gfxContextAutoSaveRestore final {
 public:
  gfxContextAutoSaveRestore() : mContext(nullptr) {}

  explicit gfxContextAutoSaveRestore(gfxContext* aContext)
      : mContext(aContext) {
    mContext->Save();
  }

  ~gfxContextAutoSaveRestore() { Restore(); }

  void SetContext(gfxContext* aContext) {
    MOZ_ASSERT(!mContext, "no context?");
    mContext = aContext;
    mContext->Save();
  }

  void EnsureSaved(gfxContext* aContext) {
    MOZ_ASSERT(!mContext || mContext == aContext, "wrong context");
    if (!mContext) {
      mContext = aContext;
      mContext->Save();
    }
  }

  void Restore() {
    if (mContext) {
      mContext->Restore();
      mContext = nullptr;
    }
  }

 private:
  gfxContext* mContext;
};

/**
 * Sentry helper class for functions with multiple return points that need to
 * back up the current matrix of a context and have it automatically restored
 * before they return.
 */
class MOZ_STACK_CLASS gfxContextMatrixAutoSaveRestore final {
 public:
  gfxContextMatrixAutoSaveRestore() : mContext(nullptr) {}

  explicit gfxContextMatrixAutoSaveRestore(gfxContext* aContext)
      : mContext(aContext), mMatrix(aContext->CurrentMatrix()) {}

  ~gfxContextMatrixAutoSaveRestore() {
    if (mContext) {
      mContext->SetMatrix(mMatrix);
    }
  }

  void SetContext(gfxContext* aContext) {
    NS_ASSERTION(!mContext, "Not going to restore the matrix on some context!");
    mContext = aContext;
    mMatrix = aContext->CurrentMatrix();
  }

  void Restore() {
    if (mContext) {
      mContext->SetMatrix(mMatrix);
      mContext = nullptr;
    }
  }

  const mozilla::gfx::Matrix& Matrix() {
    MOZ_ASSERT(mContext, "mMatrix doesn't contain a useful matrix");
    return mMatrix;
  }

  bool HasMatrix() const { return !!mContext; }

 private:
  gfxContext* mContext;
  mozilla::gfx::Matrix mMatrix;
};

class MOZ_STACK_CLASS gfxGroupForBlendAutoSaveRestore final {
 public:
  using Float = mozilla::gfx::Float;
  using Matrix = mozilla::gfx::Matrix;

  explicit gfxGroupForBlendAutoSaveRestore(gfxContext* aContext)
      : mContext(aContext) {}

  ~gfxGroupForBlendAutoSaveRestore() {
    if (mPushedGroup) {
      mContext->PopGroupAndBlend();
    }
  }

  void PushGroupForBlendBack(gfxContentType aContent, Float aOpacity = 1.0f,
                             mozilla::gfx::SourceSurface* aMask = nullptr,
                             const Matrix& aMaskTransform = Matrix()) {
    MOZ_ASSERT(!mPushedGroup, "Already called PushGroupForBlendBack once");
    mContext->PushGroupForBlendBack(aContent, aOpacity, aMask, aMaskTransform);
    mPushedGroup = true;
  }

 private:
  gfxContext* mContext;
  bool mPushedGroup = false;
};

class MOZ_STACK_CLASS gfxClipAutoSaveRestore final {
 public:
  using Rect = mozilla::gfx::Rect;

  explicit gfxClipAutoSaveRestore(gfxContext* aContext) : mContext(aContext) {}

  void Clip(const gfxRect& aRect) { Clip(ToRect(aRect)); }

  void Clip(const Rect& aRect) {
    MOZ_ASSERT(!mClipped, "Already called Clip once");
    mContext->Clip(aRect);
    mClipped = true;
  }

  void TransformedClip(const gfxMatrix& aTransform, const gfxRect& aRect) {
    MOZ_ASSERT(!mClipped, "Already called Clip once");
    if (aTransform.IsSingular()) {
      return;
    }
    gfxContextMatrixAutoSaveRestore matrixAutoSaveRestore(mContext);
    mContext->Multiply(aTransform);
    mContext->Clip(aRect);
    mClipped = true;
  }

  ~gfxClipAutoSaveRestore() {
    if (mClipped) {
      mContext->PopClip();
    }
  }

 private:
  gfxContext* mContext;
  bool mClipped = false;
};

class MOZ_STACK_CLASS DrawTargetAutoDisableSubpixelAntialiasing final {
 public:
  typedef mozilla::gfx::DrawTarget DrawTarget;

  DrawTargetAutoDisableSubpixelAntialiasing(DrawTarget* aDT, bool aDisable)
      : mSubpixelAntialiasingEnabled(false) {
    if (aDisable) {
      mDT = aDT;
      mSubpixelAntialiasingEnabled = mDT->GetPermitSubpixelAA();
      mDT->SetPermitSubpixelAA(false);
    }
  }
  ~DrawTargetAutoDisableSubpixelAntialiasing() {
    if (mDT) {
      mDT->SetPermitSubpixelAA(mSubpixelAntialiasingEnabled);
    }
  }

 private:
  RefPtr<DrawTarget> mDT;
  bool mSubpixelAntialiasingEnabled;
};

/* This interface should be implemented to handle exporting the clip from a
 * context.
 */
class ClipExporter : public mozilla::gfx::PathSink {
 public:
  virtual void BeginClip(const mozilla::gfx::Matrix& aMatrix) = 0;
  virtual void EndClip() = 0;
};

#endif /* GFX_CONTEXT_H */
