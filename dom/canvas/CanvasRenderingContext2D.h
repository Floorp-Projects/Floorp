/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CanvasRenderingContext2D_h
#define CanvasRenderingContext2D_h

#include "mozilla/Attributes.h"
#include <vector>
#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsICanvasRenderingContextInternal.h"
#include "mozilla/RefPtr.h"
#include "nsColor.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "gfxTextRun.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BasicRenderingContext2D.h"
#include "mozilla/dom/CanvasGradient.h"
#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/dom/CanvasPattern.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/UniquePtr.h"
#include "gfx2DGlue.h"
#include "imgIEncoder.h"
#include "nsLayoutUtils.h"
#include "mozilla/EnumeratedArray.h"
#include "FilterSupport.h"
#include "nsSVGEffects.h"
#include "Layers.h"
#include "nsBidi.h"

class nsGlobalWindow;
class nsXULElement;

namespace mozilla {
namespace gl {
class SourceSurface;
} // namespace gl

namespace dom {
class HTMLImageElementOrHTMLCanvasElementOrHTMLVideoElementOrImageBitmap;
typedef HTMLImageElementOrHTMLCanvasElementOrHTMLVideoElementOrImageBitmap CanvasImageSource;
class ImageData;
class StringOrCanvasGradientOrCanvasPattern;
class OwningStringOrCanvasGradientOrCanvasPattern;
class TextMetrics;
class CanvasFilterChainObserver;
class CanvasPath;

extern const mozilla::gfx::Float SIGMA_MAX;

template<typename T> class Optional;

struct CanvasBidiProcessor;
class CanvasRenderingContext2DUserData;
class CanvasDrawObserver;
class CanvasShutdownObserver;

/**
 ** CanvasRenderingContext2D
 **/
class CanvasRenderingContext2D final :
  public nsICanvasRenderingContextInternal,
  public nsWrapperCache,
  public BasicRenderingContext2D
{
  virtual ~CanvasRenderingContext2D();

public:
  explicit CanvasRenderingContext2D(layers::LayersBackend aCompositorBackend);

  virtual JSObject* WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  HTMLCanvasElement* GetCanvas() const
  {
    if (!mCanvasElement || mCanvasElement->IsInNativeAnonymousSubtree()) {
      return nullptr;
    }

    // corresponds to changes to the old bindings made in bug 745025
    return mCanvasElement->GetOriginalCanvas();
  }

  void Save() override;
  void Restore() override;
  void Scale(double aX, double aY, mozilla::ErrorResult& aError) override;
  void Rotate(double aAngle, mozilla::ErrorResult& aError) override;
  void Translate(double aX, double aY, mozilla::ErrorResult& aError) override;
  void Transform(double aM11, double aM12, double aM21, double aM22,
                 double aDx, double aDy, mozilla::ErrorResult& aError) override;
  void SetTransform(double aM11, double aM12, double aM21, double aM22,
                    double aDx, double aDy, mozilla::ErrorResult& aError) override;
  void ResetTransform(mozilla::ErrorResult& aError) override;

  double GlobalAlpha() override
  {
    return CurrentState().globalAlpha;
  }

  // Useful for silencing cast warnings
  static mozilla::gfx::Float ToFloat(double aValue) { return mozilla::gfx::Float(aValue); }

  void SetGlobalAlpha(double aGlobalAlpha) override
  {
    if (aGlobalAlpha >= 0.0 && aGlobalAlpha <= 1.0) {
      CurrentState().globalAlpha = ToFloat(aGlobalAlpha);
    }
  }

  void GetGlobalCompositeOperation(nsAString& aOp,
                                   mozilla::ErrorResult& aError) override;
  void SetGlobalCompositeOperation(const nsAString& aOp,
                                   mozilla::ErrorResult& aError) override;

  void
  GetStrokeStyle(OwningStringOrCanvasGradientOrCanvasPattern& aValue) override
  {
    GetStyleAsUnion(aValue, Style::STROKE);
  }

  void
  SetStrokeStyle(const StringOrCanvasGradientOrCanvasPattern& aValue) override
  {
    SetStyleFromUnion(aValue, Style::STROKE);
  }

  void
  GetFillStyle(OwningStringOrCanvasGradientOrCanvasPattern& aValue) override
  {
    GetStyleAsUnion(aValue, Style::FILL);
  }

  void
  SetFillStyle(const StringOrCanvasGradientOrCanvasPattern& aValue) override
  {
    SetStyleFromUnion(aValue, Style::FILL);
  }

  already_AddRefed<CanvasGradient>
    CreateLinearGradient(double aX0, double aY0, double aX1, double aY1) override;
  already_AddRefed<CanvasGradient>
    CreateRadialGradient(double aX0, double aY0, double aR0,
                         double aX1, double aY1, double aR1,
                         ErrorResult& aError) override;
  already_AddRefed<CanvasPattern>
    CreatePattern(const CanvasImageSource& aElement,
                  const nsAString& aRepeat, ErrorResult& aError) override;

  double ShadowOffsetX() override
  {
    return CurrentState().shadowOffset.x;
  }

  void SetShadowOffsetX(double aShadowOffsetX) override
  {
    CurrentState().shadowOffset.x = ToFloat(aShadowOffsetX);
  }

  double ShadowOffsetY() override
  {
    return CurrentState().shadowOffset.y;
  }

  void SetShadowOffsetY(double aShadowOffsetY) override
  {
    CurrentState().shadowOffset.y = ToFloat(aShadowOffsetY);
  }

  double ShadowBlur() override
  {
    return CurrentState().shadowBlur;
  }

  void SetShadowBlur(double aShadowBlur) override
  {
    if (aShadowBlur >= 0.0) {
      CurrentState().shadowBlur = ToFloat(aShadowBlur);
    }
  }

  void GetShadowColor(nsAString& aShadowColor) override
  {
    StyleColorToString(CurrentState().shadowColor, aShadowColor);
  }

  void GetFilter(nsAString& aFilter)
  {
    aFilter = CurrentState().filterString;
  }

  void SetShadowColor(const nsAString& aShadowColor) override;
  void SetFilter(const nsAString& aFilter, mozilla::ErrorResult& aError);
  void ClearRect(double aX, double aY, double aW, double aH) override;
  void FillRect(double aX, double aY, double aW, double aH) override;
  void StrokeRect(double aX, double aY, double aW, double aH) override;
  void BeginPath();
  void Fill(const CanvasWindingRule& aWinding);
  void Fill(const CanvasPath& aPath, const CanvasWindingRule& aWinding);
  void Stroke();
  void Stroke(const CanvasPath& aPath);
  void DrawFocusIfNeeded(mozilla::dom::Element& aElement, ErrorResult& aRv);
  bool DrawCustomFocusRing(mozilla::dom::Element& aElement);
  void Clip(const CanvasWindingRule& aWinding);
  void Clip(const CanvasPath& aPath, const CanvasWindingRule& aWinding);
  bool IsPointInPath(double aX, double aY, const CanvasWindingRule& aWinding);
  bool IsPointInPath(const CanvasPath& aPath, double aX, double aY, const CanvasWindingRule& aWinding);
  bool IsPointInStroke(double aX, double aY);
  bool IsPointInStroke(const CanvasPath& aPath, double aX, double aY);
  void FillText(const nsAString& aText, double aX, double aY,
                const Optional<double>& aMaxWidth,
                mozilla::ErrorResult& aError);
  void StrokeText(const nsAString& aText, double aX, double aY,
                  const Optional<double>& aMaxWidth,
                  mozilla::ErrorResult& aError);
  TextMetrics*
    MeasureText(const nsAString& aRawText, mozilla::ErrorResult& aError);

  void AddHitRegion(const HitRegionOptions& aOptions, mozilla::ErrorResult& aError);
  void RemoveHitRegion(const nsAString& aId);
  void ClearHitRegions();

  void DrawImage(const CanvasImageSource& aImage, double aDx, double aDy,
                 mozilla::ErrorResult& aError) override
  {
    DrawImage(aImage, 0.0, 0.0, 0.0, 0.0, aDx, aDy, 0.0, 0.0, 0, aError);
  }

  void DrawImage(const CanvasImageSource& aImage, double aDx, double aDy,
                 double aDw, double aDh, mozilla::ErrorResult& aError) override
  {
    DrawImage(aImage, 0.0, 0.0, 0.0, 0.0, aDx, aDy, aDw, aDh, 2, aError);
  }

  void DrawImage(const CanvasImageSource& aImage,
                 double aSx, double aSy, double aSw, double aSh,
                 double aDx, double aDy, double aDw, double aDh,
                 mozilla::ErrorResult& aError) override
  {
    DrawImage(aImage, aSx, aSy, aSw, aSh, aDx, aDy, aDw, aDh, 6, aError);
  }

  already_AddRefed<ImageData>
    CreateImageData(JSContext* aCx, double aSw, double aSh,
                    mozilla::ErrorResult& aError);
  already_AddRefed<ImageData>
    CreateImageData(JSContext* aCx, ImageData& aImagedata,
                    mozilla::ErrorResult& aError);
  already_AddRefed<ImageData>
    GetImageData(JSContext* aCx, double aSx, double aSy, double aSw, double aSh,
                 mozilla::ErrorResult& aError);
  void PutImageData(ImageData& aImageData,
                    double aDx, double aDy, mozilla::ErrorResult& aError);
  void PutImageData(ImageData& aImageData,
                    double aDx, double aDy, double aDirtyX, double aDirtyY,
                    double aDirtyWidth, double aDirtyHeight,
                    mozilla::ErrorResult& aError);

  double LineWidth() override
  {
    return CurrentState().lineWidth;
  }

  void SetLineWidth(double aWidth) override
  {
    if (aWidth > 0.0) {
      CurrentState().lineWidth = ToFloat(aWidth);
    }
  }
  void GetLineCap(nsAString& aLinecapStyle) override;
  void SetLineCap(const nsAString& aLinecapStyle) override;
  void GetLineJoin(nsAString& aLinejoinStyle,
                   mozilla::ErrorResult& aError) override;
  void SetLineJoin(const nsAString& aLinejoinStyle) override;

  double MiterLimit() override
  {
    return CurrentState().miterLimit;
  }

  void SetMiterLimit(double aMiter) override
  {
    if (aMiter > 0.0) {
      CurrentState().miterLimit = ToFloat(aMiter);
    }
  }

  void GetFont(nsAString& aFont)
  {
    aFont = GetFont();
  }

  void SetFont(const nsAString& aFont, mozilla::ErrorResult& aError);
  void GetTextAlign(nsAString& aTextAlign);
  void SetTextAlign(const nsAString& aTextAlign);
  void GetTextBaseline(nsAString& aTextBaseline);
  void SetTextBaseline(const nsAString& aTextBaseline);

  void ClosePath() override
  {
    EnsureWritablePath();

    if (mPathBuilder) {
      mPathBuilder->Close();
    } else {
      mDSPathBuilder->Close();
    }
  }

  void MoveTo(double aX, double aY) override
  {
    EnsureWritablePath();

    if (mPathBuilder) {
      mPathBuilder->MoveTo(mozilla::gfx::Point(ToFloat(aX), ToFloat(aY)));
    } else {
      mDSPathBuilder->MoveTo(mTarget->GetTransform().TransformPoint(
                             mozilla::gfx::Point(ToFloat(aX), ToFloat(aY))));
    }
  }

  void LineTo(double aX, double aY) override
  {
    EnsureWritablePath();

    LineTo(mozilla::gfx::Point(ToFloat(aX), ToFloat(aY)));
  }

  void QuadraticCurveTo(double aCpx, double aCpy, double aX, double aY) override
  {
    EnsureWritablePath();

    if (mPathBuilder) {
      mPathBuilder->QuadraticBezierTo(mozilla::gfx::Point(ToFloat(aCpx), ToFloat(aCpy)),
                                      mozilla::gfx::Point(ToFloat(aX), ToFloat(aY)));
    } else {
      mozilla::gfx::Matrix transform = mTarget->GetTransform();
      mDSPathBuilder->QuadraticBezierTo(transform.TransformPoint(
                                          mozilla::gfx::Point(ToFloat(aCpx), ToFloat(aCpy))),
                                        transform.TransformPoint(
                                          mozilla::gfx::Point(ToFloat(aX), ToFloat(aY))));
    }
  }

  void BezierCurveTo(double aCp1x, double aCp1y, double aCp2x, double aCp2y,
                     double aX, double aY) override
  {
    EnsureWritablePath();

    BezierTo(mozilla::gfx::Point(ToFloat(aCp1x), ToFloat(aCp1y)),
             mozilla::gfx::Point(ToFloat(aCp2x), ToFloat(aCp2y)),
             mozilla::gfx::Point(ToFloat(aX), ToFloat(aY)));
  }

  void ArcTo(double aX1, double aY1, double aX2, double aY2,
             double aRadius, mozilla::ErrorResult& aError) override;
  void Rect(double aX, double aY, double aW, double aH) override;
  void Arc(double aX, double aY, double aRadius, double aStartAngle,
           double aEndAngle, bool aAnticlockwise,
           mozilla::ErrorResult& aError) override;
  void Ellipse(double aX, double aY, double aRadiusX, double aRadiusY,
               double aRotation, double aStartAngle, double aEndAngle,
               bool aAnticlockwise, ErrorResult& aError) override;

  void GetMozCurrentTransform(JSContext* aCx,
                              JS::MutableHandle<JSObject*> aResult,
                              mozilla::ErrorResult& aError);
  void SetMozCurrentTransform(JSContext* aCx,
                              JS::Handle<JSObject*> aCurrentTransform,
                              mozilla::ErrorResult& aError);
  void GetMozCurrentTransformInverse(JSContext* aCx,
                                     JS::MutableHandle<JSObject*> aResult,
                                     mozilla::ErrorResult& aError);
  void SetMozCurrentTransformInverse(JSContext* aCx,
                                     JS::Handle<JSObject*> aCurrentTransform,
                                     mozilla::ErrorResult& aError);
  void GetFillRule(nsAString& aFillRule);
  void SetFillRule(const nsAString& aFillRule);

  void SetLineDash(const Sequence<double>& aSegments,
                   mozilla::ErrorResult& aRv) override;
  void GetLineDash(nsTArray<double>& aSegments) const override;

  void SetLineDashOffset(double aOffset) override;
  double LineDashOffset() const override;

  void GetMozTextStyle(nsAString& aMozTextStyle)
  {
    GetFont(aMozTextStyle);
  }

  void SetMozTextStyle(const nsAString& aMozTextStyle,
                       mozilla::ErrorResult& aError)
  {
    SetFont(aMozTextStyle, aError);
  }

  bool ImageSmoothingEnabled() override
  {
    return CurrentState().imageSmoothingEnabled;
  }

  void SetImageSmoothingEnabled(bool aImageSmoothingEnabled) override
  {
    if (aImageSmoothingEnabled != CurrentState().imageSmoothingEnabled) {
      CurrentState().imageSmoothingEnabled = aImageSmoothingEnabled;
    }
  }

  void DrawWindow(nsGlobalWindow& aWindow, double aX, double aY,
                  double aW, double aH,
                  const nsAString& aBgColor, uint32_t aFlags,
                  mozilla::ErrorResult& aError);

  enum RenderingMode {
    SoftwareBackendMode,
    OpenGLBackendMode,
    DefaultBackendMode
  };

  bool SwitchRenderingMode(RenderingMode aRenderingMode);

  // Eventually this should be deprecated. Keeping for now to keep the binding functional.
  void Demote();

  nsresult Redraw();

  virtual int32_t GetWidth() const override;
  virtual int32_t GetHeight() const override;
  gfx::IntSize GetSize() const { return gfx::IntSize(mWidth, mHeight); }

  // nsICanvasRenderingContextInternal
  /**
    * Gets the pres shell from either the canvas element or the doc shell
    */
  virtual nsIPresShell *GetPresShell() override {
    if (mCanvasElement) {
      return mCanvasElement->OwnerDoc()->GetShell();
    }
    if (mDocShell) {
      return mDocShell->GetPresShell();
    }
    return nullptr;
  }
  NS_IMETHOD SetDimensions(int32_t aWidth, int32_t aHeight) override;
  NS_IMETHOD InitializeWithDrawTarget(nsIDocShell* aShell,
                                      NotNull<gfx::DrawTarget*> aTarget) override;

  NS_IMETHOD GetInputStream(const char* aMimeType,
                            const char16_t* aEncoderOptions,
                            nsIInputStream** aStream) override;

  already_AddRefed<mozilla::gfx::SourceSurface>
  GetSurfaceSnapshot(gfxAlphaType* aOutAlphaType = nullptr) override
  {
    EnsureTarget();
    if (aOutAlphaType) {
      *aOutAlphaType = (mOpaque ? gfxAlphaType::Opaque : gfxAlphaType::Premult);
    }
    return mTarget->Snapshot();
  }

  NS_IMETHOD SetIsOpaque(bool aIsOpaque) override;
  bool GetIsOpaque() override { return mOpaque; }
  NS_IMETHOD Reset() override;
  already_AddRefed<Layer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                         Layer* aOldLayer,
                                         LayerManager* aManager,
                                         bool aMirror = false) override;
  virtual bool ShouldForceInactiveLayer(LayerManager* aManager) override;
  void MarkContextClean() override;
  void MarkContextCleanForFrameCapture() override;
  bool IsContextCleanForFrameCapture() override;
  NS_IMETHOD SetIsIPC(bool aIsIPC) override;
  // this rect is in canvas device space
  void Redraw(const mozilla::gfx::Rect& aR);
  NS_IMETHOD Redraw(const gfxRect& aR) override { Redraw(ToRect(aR)); return NS_OK; }
  NS_IMETHOD SetContextOptions(JSContext* aCx,
                               JS::Handle<JS::Value> aOptions,
                               ErrorResult& aRvForDictionaryInit) override;

  /**
   * An abstract base class to be implemented by callers wanting to be notified
   * that a refresh has occurred. Callers must ensure an observer is removed
   * before it is destroyed.
   */
  virtual void DidRefresh() override;

  // this rect is in mTarget's current user space
  void RedrawUser(const gfxRect& aR);

  // nsISupports interface + CC
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(CanvasRenderingContext2D)

  enum class CanvasMultiGetterType : uint8_t {
    STRING = 0,
    PATTERN = 1,
    GRADIENT = 2
  };

  enum class Style : uint8_t {
    STROKE = 0,
    FILL,
    MAX
  };

  nsINode* GetParentObject()
  {
    return mCanvasElement;
  }

  void LineTo(const mozilla::gfx::Point& aPoint)
  {
    if (mPathBuilder) {
      mPathBuilder->LineTo(aPoint);
    } else {
      mDSPathBuilder->LineTo(mTarget->GetTransform().TransformPoint(aPoint));
    }
  }

  void BezierTo(const mozilla::gfx::Point& aCP1,
                const mozilla::gfx::Point& aCP2,
                const mozilla::gfx::Point& aCP3)
  {
    if (mPathBuilder) {
      mPathBuilder->BezierTo(aCP1, aCP2, aCP3);
    } else {
      mozilla::gfx::Matrix transform = mTarget->GetTransform();
      mDSPathBuilder->BezierTo(transform.TransformPoint(aCP1),
                               transform.TransformPoint(aCP2),
                               transform.TransformPoint(aCP3));
    }
  }

  friend class CanvasRenderingContext2DUserData;

  virtual UniquePtr<uint8_t[]> GetImageBuffer(int32_t* aFormat) override;


  // Given a point, return hit region ID if it exists
  nsString GetHitRegion(const mozilla::gfx::Point& aPoint) override;


  // return true and fills in the bound rect if element has a hit region.
  bool GetHitRegionRect(Element* aElement, nsRect& aRect) override;

  void OnShutdown();

  // Check the global setup, as well as the compositor type:
  bool AllowOpenGLCanvas() const;

protected:
  nsresult GetImageDataArray(JSContext* aCx, int32_t aX, int32_t aY,
                             uint32_t aWidth, uint32_t aHeight,
                             JSObject** aRetval);

  nsresult PutImageData_explicit(int32_t aX, int32_t aY, uint32_t aW, uint32_t aH,
                                 dom::Uint8ClampedArray* aArray,
                                 bool aHasDirtyRect, int32_t aDirtyX, int32_t aDirtyY,
                                 int32_t aDirtyWidth, int32_t aDirtyHeight);

  bool CopyBufferProvider(layers::PersistentBufferProvider& aOld,
                          gfx::DrawTarget& aTarget,
                          gfx::IntRect aCopyRect);

  /**
   * Internal method to complete initialisation, expects mTarget to have been set
   */
  nsresult Initialize(int32_t aWidth, int32_t aHeight);

  nsresult InitializeWithTarget(mozilla::gfx::DrawTarget* aSurface,
                                int32_t aWidth, int32_t aHeight);

  /**
    * The number of living nsCanvasRenderingContexts.  When this goes down to
    * 0, we free the premultiply and unpremultiply tables, if they exist.
    */
  static uintptr_t sNumLivingContexts;

  static mozilla::gfx::DrawTarget* sErrorTarget;

  void SetTransformInternal(const mozilla::gfx::Matrix& aTransform);

  // Some helpers.  Doesn't modify a color on failure.
  void SetStyleFromUnion(const StringOrCanvasGradientOrCanvasPattern& aValue,
                         Style aWhichStyle);
  void SetStyleFromString(const nsAString& aStr, Style aWhichStyle);

  void SetStyleFromGradient(CanvasGradient& aGradient, Style aWhichStyle)
  {
    CurrentState().SetGradientStyle(aWhichStyle, &aGradient);
  }

  void SetStyleFromPattern(CanvasPattern& aPattern, Style aWhichStyle)
  {
    CurrentState().SetPatternStyle(aWhichStyle, &aPattern);
  }

  void GetStyleAsUnion(OwningStringOrCanvasGradientOrCanvasPattern& aValue,
                       Style aWhichStyle);

  // Returns whether a color was successfully parsed.
  bool ParseColor(const nsAString& aString, nscolor* aColor);

  static void StyleColorToString(const nscolor& aColor, nsAString& aStr);

   // Returns whether a filter was successfully parsed.
  bool ParseFilter(const nsAString& aString,
                   nsTArray<nsStyleFilter>& aFilterChain,
                   ErrorResult& aError);

  // Returns whether the font was successfully updated.
  bool SetFontInternal(const nsAString& aFont, mozilla::ErrorResult& aError);


  /**
   * Creates the error target, if it doesn't exist
   */
  static void EnsureErrorTarget();

  /* This function ensures there is a writable pathbuilder available, this
   * pathbuilder may be working in user space or in device space or
   * device space.
   * After calling this function mPathTransformWillUpdate will be false
   */
  void EnsureWritablePath();

  // Ensures a path in UserSpace is available.
  void EnsureUserSpacePath(const CanvasWindingRule& aWinding = CanvasWindingRule::Nonzero);

  /**
   * Needs to be called before updating the transform. This makes a call to
   * EnsureTarget() so you don't have to.
   */
  void TransformWillUpdate();

  // Report the fillRule has changed.
  void FillRuleChanged();

   /**
   * Create the backing surfacing, if it doesn't exist. If there is an error
   * in creating the target then it will put sErrorTarget in place. If there
   * is in turn an error in creating the sErrorTarget then they would both
   * be null so IsTargetValid() would still return null.
   *
   * Returns the actual rendering mode being used by the created target.
   */
  RenderingMode EnsureTarget(const gfx::Rect* aCoveredRect = nullptr,
                             RenderingMode aRenderMode = RenderingMode::DefaultBackendMode);

  void RestoreClipsAndTransformToTarget();

  bool TrySkiaGLTarget(RefPtr<gfx::DrawTarget>& aOutDT,
                       RefPtr<layers::PersistentBufferProvider>& aOutProvider);

  bool TrySharedTarget(RefPtr<gfx::DrawTarget>& aOutDT,
                       RefPtr<layers::PersistentBufferProvider>& aOutProvider);

  bool TryBasicTarget(RefPtr<gfx::DrawTarget>& aOutDT,
                      RefPtr<layers::PersistentBufferProvider>& aOutProvider);

  void RegisterAllocation();

  void SetInitialState();

  void SetErrorState();

  /**
   * This method is run at the end of the event-loop spin where
   * ScheduleStableStateCallback was called.
   *
   * We use it to unlock resources that need to be locked while drawing.
   */
  void OnStableState();

  /**
   * Cf. OnStableState.
   */
  void ScheduleStableStateCallback();

  /**
   * Disposes an old target and prepares to lazily create a new target.
   */
  void ClearTarget();

  /*
   * Returns the target to the buffer provider. i.e. this will queue a frame for
   * rendering.
   */
  void ReturnTarget(bool aForceReset = false);

  /**
   * Check if the target is valid after calling EnsureTarget.
   */
  bool IsTargetValid() const {
    return !!mTarget && mTarget != sErrorTarget;
  }

  /**
    * Returns the surface format this canvas should be allocated using. Takes
    * into account mOpaque, platform requirements, etc.
    */
  mozilla::gfx::SurfaceFormat GetSurfaceFormat() const;

  /**
   * Returns true if we know for sure that the pattern for a given style is opaque.
   * Usefull to know if we can discard the content below in certain situations.
   */
  bool PatternIsOpaque(Style aStyle) const;

  /**
   * Update CurrentState().filter with the filter description for
   * CurrentState().filterChain.
   * Flushes the PresShell, so the world can change if you call this function.
   */
  void UpdateFilter();

  nsLayoutUtils::SurfaceFromElementResult
    CachedSurfaceFromElement(Element* aElement);

  void DrawImage(const CanvasImageSource& aImgElt,
                 double aSx, double aSy, double aSw, double aSh,
                 double aDx, double aDy, double aDw, double aDh,
                 uint8_t aOptional_argc, mozilla::ErrorResult& aError);

  void DrawDirectlyToCanvas(const nsLayoutUtils::DirectDrawInfo& aImage,
                            mozilla::gfx::Rect* aBounds,
                            mozilla::gfx::Rect aDest,
                            mozilla::gfx::Rect aSrc,
                            gfx::IntSize aImgSize);

  nsString& GetFont()
  {
    /* will initilize the value if not set, else does nothing */
    GetCurrentFontStyle();

    return CurrentState().font;
  }

  // This function maintains a list of raw pointers to cycle-collected
  // objects. We need to ensure that no entries persist beyond unlink,
  // since the objects are logically destructed at that point.
  static std::vector<CanvasRenderingContext2D*>& DemotableContexts();
  static void DemoteOldestContextIfNecessary();

  static void AddDemotableContext(CanvasRenderingContext2D* aContext);
  static void RemoveDemotableContext(CanvasRenderingContext2D* aContext);

  RenderingMode mRenderingMode;

  layers::LayersBackend mCompositorBackend;

  // Member vars
  int32_t mWidth, mHeight;

  // This is true when the canvas is valid, but of zero size, this requires
  // specific behavior on some operations.
  bool mZero;

  bool mOpaque;

  // This is true when the next time our layer is retrieved we need to
  // recreate it (i.e. our backing surface changed)
  bool mResetLayer;
  // This is needed for drawing in drawAsyncXULElement
  bool mIPC;
  // True if the current DrawTarget is using skia-gl, used so we can avoid
  // requesting the DT from mBufferProvider to check.
  bool mIsSkiaGL;

  bool mHasPendingStableStateCallback;

  nsTArray<CanvasRenderingContext2DUserData*> mUserDatas;

  // If mCanvasElement is not provided, then a docshell is
  nsCOMPtr<nsIDocShell> mDocShell;

  // This is created lazily so it is necessary to call EnsureTarget before
  // accessing it. In the event of an error it will be equal to
  // sErrorTarget.
  RefPtr<mozilla::gfx::DrawTarget> mTarget;

  RefPtr<mozilla::layers::PersistentBufferProvider> mBufferProvider;

  uint32_t SkiaGLTex() const;

  // This observes our draw calls at the beginning of the canvas
  // lifetime and switches to software or GPU mode depending on
  // what it thinks is best
  CanvasDrawObserver* mDrawObserver;
  void RemoveDrawObserver();

  RefPtr<CanvasShutdownObserver> mShutdownObserver;
  void RemoveShutdownObserver();
  bool AlreadyShutDown() const { return !mShutdownObserver; }

  /**
    * Flag to avoid duplicate calls to InvalidateFrame. Set to true whenever
    * Redraw is called, reset to false when Render is called.
    */
  bool mIsEntireFrameInvalid;
  /**
    * When this is set, the first call to Redraw(gfxRect) should set
    * mIsEntireFrameInvalid since we expect it will be followed by
    * many more Redraw calls.
    */
  bool mPredictManyRedrawCalls;

  /**
   * Flag to avoid unnecessary surface copies to FrameCaptureListeners in the
   * case when the canvas is not currently being drawn into and not rendered
   * but canvas capturing is still ongoing.
   */
  bool mIsCapturedFrameInvalid;

  /**
    * We also have a device space pathbuilder. The reason for this is as
    * follows, when a path is being built, but the transform changes, we
    * can no longer keep a single path in userspace, considering there's
    * several 'user spaces' now. We therefore transform the current path
    * into device space, and add all operations to this path in device
    * space.
    *
    * When then finally executing a render, the Azure drawing API expects
    * the path to be in userspace. We could then set an identity transform
    * on the DrawTarget and do all drawing in device space. This is
    * undesirable because it requires transforming patterns, gradients,
    * clips, etc. into device space and it would not work for stroking.
    * What we do instead is convert the path back to user space when it is
    * drawn, and draw it with the current transform. This makes all drawing
    * occur correctly.
    *
    * There's never both a device space path builder and a user space path
    * builder present at the same time. There is also never a path and a
    * path builder present at the same time. When writing proceeds on an
    * existing path the Path is cleared and a new builder is created.
    *
    * mPath is always in user-space.
    */
  RefPtr<mozilla::gfx::Path> mPath;
  RefPtr<mozilla::gfx::PathBuilder> mDSPathBuilder;
  RefPtr<mozilla::gfx::PathBuilder> mPathBuilder;
  bool mPathTransformWillUpdate;
  mozilla::gfx::Matrix mPathToDS;

  /**
    * Number of times we've invalidated before calling redraw
    */
  uint32_t mInvalidateCount;
  static const uint32_t kCanvasMaxInvalidateCount = 100;

  /**
    * State information for hit regions
    */
  struct RegionInfo
  {
    nsString          mId;
    // fallback element for a11y
    RefPtr<Element> mElement;
    // Path of the hit region in the 2d context coordinate space (not user space)
    RefPtr<gfx::Path> mPath;
  };

  nsTArray<RegionInfo> mHitRegionsOptions;

  nsBidi mBidiEngine;

  /**
    * Returns true if a shadow should be drawn along with a
    * drawing operation.
    */
  bool NeedToDrawShadow()
  {
    const ContextState& state = CurrentState();

    // The spec says we should not draw shadows if the operator is OVER.
    // If it's over and the alpha value is zero, nothing needs to be drawn.
    return NS_GET_A(state.shadowColor) != 0 &&
      (state.shadowBlur != 0.f || state.shadowOffset.x != 0.f || state.shadowOffset.y != 0.f);
  }

  /**
    * Returns true if the result of a drawing operation should be
    * drawn with a filter.
    */
  bool NeedToApplyFilter()
  {
    return EnsureUpdatedFilter().mPrimitives.Length() > 0;
  }

  /**
   * Calls UpdateFilter if the canvas's WriteOnly state has changed between the
   * last call to UpdateFilter and now.
   */
  const gfx::FilterDescription& EnsureUpdatedFilter() {
    bool isWriteOnly = mCanvasElement && mCanvasElement->IsWriteOnly();
    if (CurrentState().filterSourceGraphicTainted != isWriteOnly) {
      UpdateFilter();
      EnsureTarget();
    }
    MOZ_ASSERT(CurrentState().filterSourceGraphicTainted == isWriteOnly);
    return CurrentState().filter;
  }

  bool NeedToCalculateBounds()
  {
    return NeedToDrawShadow() || NeedToApplyFilter();
  }

  mozilla::gfx::CompositionOp UsedOperation()
  {
    if (NeedToDrawShadow() || NeedToApplyFilter()) {
      // In this case the shadow or filter rendering will use the operator.
      return mozilla::gfx::CompositionOp::OP_OVER;
    }

    return CurrentState().op;
  }

  // text

protected:
  enum class TextAlign : uint8_t {
    START,
    END,
    LEFT,
    RIGHT,
    CENTER
  };

  enum class TextBaseline : uint8_t {
    TOP,
    HANGING,
    MIDDLE,
    ALPHABETIC,
    IDEOGRAPHIC,
    BOTTOM
  };

  enum class TextDrawOperation : uint8_t {
    FILL,
    STROKE,
    MEASURE
  };

protected:
  gfxFontGroup *GetCurrentFontStyle();

  /**
   * Implementation of the fillText, strokeText, and measure functions with
   * the operation abstracted to a flag.
   */
  nsresult DrawOrMeasureText(const nsAString& aText,
                             float aX,
                             float aY,
                             const Optional<double>& aMaxWidth,
                             TextDrawOperation aOp,
                             float* aWidth);

  bool CheckSizeForSkiaGL(mozilla::gfx::IntSize aSize);

  // A clip or a transform, recorded and restored in order.
  struct ClipState {
    explicit ClipState(mozilla::gfx::Path* aClip)
      : clip(aClip)
    {}

    explicit ClipState(const mozilla::gfx::Matrix& aTransform)
      : transform(aTransform)
    {}

    bool IsClip() const { return !!clip; }

    RefPtr<mozilla::gfx::Path> clip;
    mozilla::gfx::Matrix transform;
  };

  // state stack handling
  class ContextState {
  public:
    ContextState() : textAlign(TextAlign::START),
                     textBaseline(TextBaseline::ALPHABETIC),
                     shadowColor(0),
                     lineWidth(1.0f),
                     miterLimit(10.0f),
                     globalAlpha(1.0f),
                     shadowBlur(0.0),
                     dashOffset(0.0f),
                     op(mozilla::gfx::CompositionOp::OP_OVER),
                     fillRule(mozilla::gfx::FillRule::FILL_WINDING),
                     lineCap(mozilla::gfx::CapStyle::BUTT),
                     lineJoin(mozilla::gfx::JoinStyle::MITER_OR_BEVEL),
                     filterString(u"none"),
                     filterSourceGraphicTainted(false),
                     imageSmoothingEnabled(true),
                     fontExplicitLanguage(false)
    { }

    ContextState(const ContextState& aOther)
        : fontGroup(aOther.fontGroup),
          fontLanguage(aOther.fontLanguage),
          fontFont(aOther.fontFont),
          gradientStyles(aOther.gradientStyles),
          patternStyles(aOther.patternStyles),
          colorStyles(aOther.colorStyles),
          font(aOther.font),
          textAlign(aOther.textAlign),
          textBaseline(aOther.textBaseline),
          shadowColor(aOther.shadowColor),
          transform(aOther.transform),
          shadowOffset(aOther.shadowOffset),
          lineWidth(aOther.lineWidth),
          miterLimit(aOther.miterLimit),
          globalAlpha(aOther.globalAlpha),
          shadowBlur(aOther.shadowBlur),
          dash(aOther.dash),
          dashOffset(aOther.dashOffset),
          op(aOther.op),
          fillRule(aOther.fillRule),
          lineCap(aOther.lineCap),
          lineJoin(aOther.lineJoin),
          filterString(aOther.filterString),
          filterChain(aOther.filterChain),
          filterChainObserver(aOther.filterChainObserver),
          filter(aOther.filter),
          filterAdditionalImages(aOther.filterAdditionalImages),
          filterSourceGraphicTainted(aOther.filterSourceGraphicTainted),
          imageSmoothingEnabled(aOther.imageSmoothingEnabled),
          fontExplicitLanguage(aOther.fontExplicitLanguage)
    { }

    void SetColorStyle(Style aWhichStyle, nscolor aColor)
    {
      colorStyles[aWhichStyle] = aColor;
      gradientStyles[aWhichStyle] = nullptr;
      patternStyles[aWhichStyle] = nullptr;
    }

    void SetPatternStyle(Style aWhichStyle, CanvasPattern* aPat)
    {
      gradientStyles[aWhichStyle] = nullptr;
      patternStyles[aWhichStyle] = aPat;
    }

    void SetGradientStyle(Style aWhichStyle, CanvasGradient* aGrad)
    {
      gradientStyles[aWhichStyle] = aGrad;
      patternStyles[aWhichStyle] = nullptr;
    }

    /**
      * returns true iff the given style is a solid color.
      */
    bool StyleIsColor(Style aWhichStyle) const
    {
      return !(patternStyles[aWhichStyle] || gradientStyles[aWhichStyle]);
    }

    int32_t ShadowBlurRadius() const
    {
      static const gfxFloat GAUSSIAN_SCALE_FACTOR = (3 * sqrt(2 * M_PI) / 4) * 1.5;
      return (int32_t)floor(ShadowBlurSigma() * GAUSSIAN_SCALE_FACTOR + 0.5);
    }

    mozilla::gfx::Float ShadowBlurSigma() const
    {
      return std::min(SIGMA_MAX, shadowBlur / 2.0f);
    }

    nsTArray<ClipState> clipsAndTransforms;

    RefPtr<gfxFontGroup> fontGroup;
    nsCOMPtr<nsIAtom> fontLanguage;
    nsFont fontFont;

    EnumeratedArray<Style, Style::MAX, RefPtr<CanvasGradient>> gradientStyles;
    EnumeratedArray<Style, Style::MAX, RefPtr<CanvasPattern>> patternStyles;
    EnumeratedArray<Style, Style::MAX, nscolor> colorStyles;

    nsString font;
    TextAlign textAlign;
    TextBaseline textBaseline;

    nscolor shadowColor;

    mozilla::gfx::Matrix transform;
    mozilla::gfx::Point shadowOffset;
    mozilla::gfx::Float lineWidth;
    mozilla::gfx::Float miterLimit;
    mozilla::gfx::Float globalAlpha;
    mozilla::gfx::Float shadowBlur;
    nsTArray<mozilla::gfx::Float> dash;
    mozilla::gfx::Float dashOffset;

    mozilla::gfx::CompositionOp op;
    mozilla::gfx::FillRule fillRule;
    mozilla::gfx::CapStyle lineCap;
    mozilla::gfx::JoinStyle lineJoin;

    nsString filterString;
    nsTArray<nsStyleFilter> filterChain;
    RefPtr<nsSVGFilterChainObserver> filterChainObserver;
    mozilla::gfx::FilterDescription filter;
    nsTArray<RefPtr<mozilla::gfx::SourceSurface>> filterAdditionalImages;

    // This keeps track of whether the canvas was "tainted" or not when
    // we last used a filter. This is a security measure, whereby the
    // canvas is flipped to write-only if a cross-origin image is drawn to it.
    // This is to stop bad actors from reading back data they shouldn't have
    // access to.
    //
    // This also limits what filters we can apply to the context; in particular
    // feDisplacementMap is restricted.
    //
    // We keep track of this to ensure that if this gets out of sync with the
    // tainted state of the canvas itself, we update our filters accordingly.
    bool filterSourceGraphicTainted;

    bool imageSmoothingEnabled;
    bool fontExplicitLanguage;
  };

  AutoTArray<ContextState, 3> mStyleStack;

  inline ContextState& CurrentState() {
    return mStyleStack[mStyleStack.Length() - 1];
  }

  inline const ContextState& CurrentState() const {
    return mStyleStack[mStyleStack.Length() - 1];
  }

  friend class CanvasGeneralPattern;
  friend class CanvasFilterChainObserver;
  friend class AdjustedTarget;
  friend class AdjustedTargetForShadow;
  friend class AdjustedTargetForFilter;

  // other helpers
  void GetAppUnitsValues(int32_t* aPerDevPixel, int32_t* aPerCSSPixel)
  {
    // If we don't have a canvas element, we just return something generic.
    int32_t devPixel = 60;
    int32_t cssPixel = 60;

    nsIPresShell *ps = GetPresShell();
    nsPresContext *pc;

    if (!ps) goto FINISH;
    pc = ps->GetPresContext();
    if (!pc) goto FINISH;
    devPixel = pc->AppUnitsPerDevPixel();
    cssPixel = pc->AppUnitsPerCSSPixel();

  FINISH:
    if (aPerDevPixel)
      *aPerDevPixel = devPixel;
    if (aPerCSSPixel)
      *aPerCSSPixel = cssPixel;
  }

  friend struct CanvasBidiProcessor;
  friend class CanvasDrawObserver;
};

} // namespace dom
} // namespace mozilla

#endif /* CanvasRenderingContext2D_h */
