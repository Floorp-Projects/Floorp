/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfxTypes.h"

#include "gfxPoint.h"
#include "gfxRect.h"
#include "gfxMatrix.h"
#include "gfxPattern.h"
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
  typedef mozilla::gfx::CapStyle CapStyle;
  typedef mozilla::gfx::CompositionOp CompositionOp;
  typedef mozilla::gfx::JoinStyle JoinStyle;
  typedef mozilla::gfx::FillRule FillRule;
  typedef mozilla::gfx::Float Float;
  typedef mozilla::gfx::Path Path;
  typedef mozilla::gfx::Pattern Pattern;
  typedef mozilla::gfx::Rect Rect;
  typedef mozilla::gfx::RectCornerRadii RectCornerRadii;
  typedef mozilla::gfx::Size Size;

  NS_INLINE_DECL_REFCOUNTING(gfxContext)

 public:
  /**
   * Initialize this context from a DrawTarget.
   * Strips any transform from aTarget.
   * aTarget will be flushed in the gfxContext's destructor.
   * If aTarget is null or invalid, nullptr is returned.  The caller
   * is responsible for handling this scenario as appropriate.
   */
  static already_AddRefed<gfxContext> CreateOrNull(
      mozilla::gfx::DrawTarget* aTarget,
      const mozilla::gfx::Point& aDeviceOffset = mozilla::gfx::Point());

  /**
   * Create a new gfxContext wrapping aTarget and preserving aTarget's
   * transform. Note that the transform is moved from aTarget to the resulting
   * gfxContext, aTarget will no longer have its transform.
   * If aTarget is null or invalid, nullptr is returned.  The caller
   * is responsible for handling this scenario as appropriate.
   */
  static already_AddRefed<gfxContext> CreatePreservingTransformOrNull(
      mozilla::gfx::DrawTarget* aTarget);

  mozilla::gfx::DrawTarget* GetDrawTarget() { return mDT; }

  /**
   * Returns the DrawTarget if it's actually a TextDrawTarget.
   */
  mozilla::layout::TextDrawTarget* GetTextDrawer();

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
  void Fill();
  void Fill(const Pattern& aPattern);

  /**
   * Forgets the current path.
   */
  void NewPath();

  /**
   * Closes the path, i.e. connects the last drawn point to the first one.
   *
   * Filling a path will implicitly close it.
   */
  void ClosePath();

  /**
   * Returns the current path.
   */
  already_AddRefed<Path> GetPath();

  /**
   * Sets the given path as the current path.
   */
  void SetPath(Path* path);

  /**
   * Moves the pen to a new point without drawing a line.
   */
  void MoveTo(const gfxPoint& pt);

  /**
   * Draws a line from the current point to pt.
   *
   * @see MoveTo
   */
  void LineTo(const gfxPoint& pt);

  // path helpers
  /**
   * Draws a line from start to end.
   */
  void Line(const gfxPoint& start,
            const gfxPoint& end);  // XXX snapToPixels option?

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
  void Multiply(const gfxMatrix& other);
  void Multiply(const mozilla::gfx::Matrix& other);

  /**
   * Replaces the current transformation matrix with matrix.
   */
  void SetMatrix(const mozilla::gfx::Matrix& matrix);
  void SetMatrixDouble(const gfxMatrix& matrix);

  /**
   * Returns the current transformation matrix.
   */
  mozilla::gfx::Matrix CurrentMatrix() const;
  gfxMatrix CurrentMatrixDouble() const;

  /**
   * Converts a point from device to user coordinates using the inverse
   * transformation matrix.
   */
  gfxPoint DeviceToUser(const gfxPoint& point) const;

  /**
   * Converts a size from device to user coordinates. This does not apply
   * translation components of the matrix.
   */
  Size DeviceToUser(const Size& size) const;

  /**
   * Converts a rectangle from device to user coordinates; this has the
   * same effect as using DeviceToUser on both the rectangle's point and
   * size.
   */
  gfxRect DeviceToUser(const gfxRect& rect) const;

  /**
   * Converts a point from user to device coordinates using the transformation
   * matrix.
   */
  gfxPoint UserToDevice(const gfxPoint& point) const;

  /**
   * Converts a size from user to device coordinates. This does not apply
   * translation components of the matrix.
   */
  Size UserToDevice(const Size& size) const;

  /**
   * Converts a rectangle from user to device coordinates.  The
   * resulting rectangle is the minimum device-space rectangle that
   * encloses the user-space rectangle given.
   */
  gfxRect UserToDevice(const gfxRect& rect) const;

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
  void SetDeviceColor(const mozilla::gfx::DeviceColor& aColor);

  /**
   * Gets the current color.  It's returned in the device color space.
   * returns false if there is something other than a color
   *         set as the current source (pattern, surface, etc)
   */
  bool GetDeviceColor(mozilla::gfx::DeviceColor& aColorOut);

  /**
   * Returns true if color is neither opaque nor transparent (i.e. alpha is not
   * 0 or 1), and false otherwise. If true, aColorOut is set on output.
   */
  bool HasNonOpaqueNonTransparentColor(mozilla::gfx::DeviceColor& aColorOut) {
    return GetDeviceColor(aColorOut) && 0.f < aColorOut.a && aColorOut.a < 1.f;
  }

  /**
   * Set a solid color in the sRGB color space to use for drawing.
   * If CMS is not enabled, the color is treated as a device-space color
   * and this call is identical to SetDeviceColor().
   */
  void SetColor(const mozilla::gfx::sRGBColor& aColor);

  /**
   * Uses a pattern for drawing.
   */
  void SetPattern(gfxPattern* pattern);

  /**
   * Get the source pattern (solid color, normal pattern, surface, etc)
   */
  already_AddRefed<gfxPattern> GetPattern();

  /**
   ** Painting
   **/
  /**
   * Paints the current source surface/pattern everywhere in the current
   * clip region.
   */
  void Paint(Float alpha = 1.0);

  /**
   ** Painting with a Mask
   **/
  /**
   * Like Paint, except that it only draws the source where pattern is
   * non-transparent.
   */
  void Mask(mozilla::gfx::SourceSurface* aSurface, mozilla::gfx::Float aAlpha,
            const mozilla::gfx::Matrix& aTransform);
  void Mask(mozilla::gfx::SourceSurface* aSurface,
            const mozilla::gfx::Matrix& aTransform) {
    Mask(aSurface, 1.0f, aTransform);
  }
  void Mask(mozilla::gfx::SourceSurface* surface, float alpha = 1.0f,
            const mozilla::gfx::Point& offset = mozilla::gfx::Point());

  /**
   ** Line Properties
   **/

  void SetDash(const Float* dashes, int ndash, Float offset);
  // Return true if dashing is set, false if it's not enabled or the
  // context is in an error state.  |offset| can be nullptr to mean
  // "don't care".
  bool CurrentDash(FallibleTArray<Float>& dashes, Float* offset) const;

  /**
   * Sets the line width that's used for line drawing.
   */
  void SetLineWidth(Float width);

  /**
   * Returns the currently set line width.
   *
   * @see SetLineWidth
   */
  Float CurrentLineWidth() const;

  /**
   * Sets the line caps, i.e. how line endings are drawn.
   */
  void SetLineCap(CapStyle cap);
  CapStyle CurrentLineCap() const;

  /**
   * Sets the line join, i.e. how the connection between two lines is
   * drawn.
   */
  void SetLineJoin(JoinStyle join);
  JoinStyle CurrentLineJoin() const;

  void SetMiterLimit(Float limit);
  Float CurrentMiterLimit() const;

  /**
   * Sets the operator used for all further drawing. The operator affects
   * how drawing something will modify the destination. For example, the
   * OVER operator will do alpha blending of source and destination, while
   * SOURCE will replace the destination with the source.
   */
  void SetOp(CompositionOp op);
  CompositionOp CurrentOp() const;

  void SetAntialiasMode(mozilla::gfx::AntialiasMode mode);
  mozilla::gfx::AntialiasMode CurrentAntialiasMode() const;

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
  void Clip(const Rect& rect);
  void Clip(const gfxRect& rect);         // will clip to a rect
  void SnappedClip(const gfxRect& rect);  // snap rect and clip to the result
  void Clip(Path* aPath);

  void PopClip();

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
   * Returns true if the given rectangle is fully contained in the current clip.
   * This is conservative; it may return false even when the given rectangle is
   * fully contained by the current clip.
   */
  bool ClipContainsRect(const gfxRect& aRect);

  /**
   * Exports the current clip using the provided exporter.
   */
  bool ExportClip(ClipExporter& aExporter);

  /**
   * Groups
   */
  void PushGroupForBlendBack(
      gfxContentType content, mozilla::gfx::Float aOpacity = 1.0f,
      mozilla::gfx::SourceSurface* aMask = nullptr,
      const mozilla::gfx::Matrix& aMaskTransform = mozilla::gfx::Matrix());

  void PopGroupAndBlend();

  mozilla::gfx::Point GetDeviceOffset() const;
  void SetDeviceOffset(const mozilla::gfx::Point& aOffset);

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
  /**
   * Initialize this context from a DrawTarget.
   * Strips any transform from aTarget.
   * aTarget will be flushed in the gfxContext's destructor.  Use the static
   * ContextForDrawTargetNoTransform() when you want this behavior, as that
   * version deals with null DrawTarget better.
   */
  explicit gfxContext(
      mozilla::gfx::DrawTarget* aTarget,
      const mozilla::gfx::Point& aDeviceOffset = mozilla::gfx::Point());
  ~gfxContext();

  friend class PatternFromState;
  friend class GlyphBufferAzure;

  typedef mozilla::gfx::Matrix Matrix;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::sRGBColor sRGBColor;
  typedef mozilla::gfx::DeviceColor DeviceColor;
  typedef mozilla::gfx::StrokeOptions StrokeOptions;
  typedef mozilla::gfx::PathBuilder PathBuilder;
  typedef mozilla::gfx::SourceSurface SourceSurface;

  struct AzureState {
    AzureState()
        : op(mozilla::gfx::CompositionOp::OP_OVER),
          color(0, 0, 0, 1.0f),
          aaMode(mozilla::gfx::AntialiasMode::SUBPIXEL),
          patternTransformChanged(false)
#ifdef DEBUG
          ,
          mContentChanged(false)
#endif
    {
    }

    mozilla::gfx::CompositionOp op;
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
    RefPtr<DrawTarget> drawTarget;
    mozilla::gfx::AntialiasMode aaMode;
    bool patternTransformChanged;
    Matrix patternTransform;
    DeviceColor fontSmoothingBackgroundColor;
    // This is used solely for using minimal intermediate surface size.
    mozilla::gfx::Point deviceOffset;
#ifdef DEBUG
    // Whether the content of this AzureState changed after construction.
    bool mContentChanged;
#endif
  };

  // This ensures mPath contains a valid path (in user space!)
  void EnsurePath();
  // This ensures mPathBuilder contains a valid PathBuilder (in user space!)
  void EnsurePathBuilder();
  CompositionOp GetOp();
  void ChangeTransform(const mozilla::gfx::Matrix& aNewMatrix,
                       bool aUpdatePatternTransform = true);
  Rect GetAzureDeviceSpaceClipBounds() const;
  Matrix GetDTTransform() const;

  bool mPathIsRect;
  bool mTransformChanged;
  Matrix mPathTransform;
  Rect mRect;
  RefPtr<PathBuilder> mPathBuilder;
  RefPtr<Path> mPath;
  Matrix mTransform;
  nsTArray<AzureState> mStateStack;

  AzureState& CurrentState() { return mStateStack[mStateStack.Length() - 1]; }
  const AzureState& CurrentState() const {
    return mStateStack[mStateStack.Length() - 1];
  }

  RefPtr<DrawTarget> mDT;
};

/**
 * Sentry helper class for functions with multiple return points that need to
 * call Save() on a gfxContext and have Restore() called automatically on the
 * gfxContext before they return.
 */
class gfxContextAutoSaveRestore {
 public:
  gfxContextAutoSaveRestore() : mContext(nullptr) {}

  explicit gfxContextAutoSaveRestore(gfxContext* aContext)
      : mContext(aContext) {
    mContext->Save();
  }

  ~gfxContextAutoSaveRestore() { Restore(); }

  void SetContext(gfxContext* aContext) {
    NS_ASSERTION(!mContext, "Not going to call Restore() on some context!!!");
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
class gfxContextMatrixAutoSaveRestore {
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

class DrawTargetAutoDisableSubpixelAntialiasing {
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

/* This class lives on the stack and allows gfxContext users to easily, and
 * performantly get a gfx::Pattern to use for drawing in their current context.
 */
class PatternFromState {
 public:
  explicit PatternFromState(gfxContext* aContext)
      : mContext(aContext), mPattern(nullptr) {}
  ~PatternFromState() {
    if (mPattern) {
      mPattern->~Pattern();
    }
  }

  operator mozilla::gfx::Pattern&();

 private:
  union {
    mozilla::AlignedStorage2<mozilla::gfx::ColorPattern> mColorPattern;
    mozilla::AlignedStorage2<mozilla::gfx::SurfacePattern> mSurfacePattern;
  };

  gfxContext* mContext;
  mozilla::gfx::Pattern* mPattern;
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
