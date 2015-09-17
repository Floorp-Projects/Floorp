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
#include "mozilla/dom/CanvasGradient.h"
#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/dom/CanvasPattern.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/2D.h"
#include "gfx2DGlue.h"
#include "imgIEncoder.h"
#include "nsLayoutUtils.h"
#include "mozilla/EnumeratedArray.h"
#include "FilterSupport.h"
#include "nsSVGEffects.h"
#include "Layers.h"

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

/**
 ** CanvasRenderingContext2D
 **/
class CanvasRenderingContext2D final :
  public nsICanvasRenderingContextInternal,
  public nsWrapperCache
{
  virtual ~CanvasRenderingContext2D();

public:
  CanvasRenderingContext2D();

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  HTMLCanvasElement* GetCanvas() const
  {
    // corresponds to changes to the old bindings made in bug 745025
    return mCanvasElement->GetOriginalCanvas();
  }

  void Save();
  void Restore();
  void Scale(double x, double y, mozilla::ErrorResult& error);
  void Rotate(double angle, mozilla::ErrorResult& error);
  void Translate(double x, double y, mozilla::ErrorResult& error);
  void Transform(double m11, double m12, double m21, double m22, double dx,
                 double dy, mozilla::ErrorResult& error);
  void SetTransform(double m11, double m12, double m21, double m22, double dx,
                    double dy, mozilla::ErrorResult& error);
  void ResetTransform(mozilla::ErrorResult& error);

  double GlobalAlpha()
  {
    return CurrentState().globalAlpha;
  }

  // Useful for silencing cast warnings
  static mozilla::gfx::Float ToFloat(double aValue) { return mozilla::gfx::Float(aValue); }

  void SetGlobalAlpha(double globalAlpha)
  {
    if (globalAlpha >= 0.0 && globalAlpha <= 1.0) {
      CurrentState().globalAlpha = ToFloat(globalAlpha);
    }
  }

  void GetGlobalCompositeOperation(nsAString& op, mozilla::ErrorResult& error);
  void SetGlobalCompositeOperation(const nsAString& op,
                                   mozilla::ErrorResult& error);

  void GetStrokeStyle(OwningStringOrCanvasGradientOrCanvasPattern& value)
  {
    GetStyleAsUnion(value, Style::STROKE);
  }

  void SetStrokeStyle(const StringOrCanvasGradientOrCanvasPattern& value)
  {
    SetStyleFromUnion(value, Style::STROKE);
  }

  void GetFillStyle(OwningStringOrCanvasGradientOrCanvasPattern& value)
  {
    GetStyleAsUnion(value, Style::FILL);
  }

  void SetFillStyle(const StringOrCanvasGradientOrCanvasPattern& value)
  {
    SetStyleFromUnion(value, Style::FILL);
  }

  already_AddRefed<CanvasGradient>
    CreateLinearGradient(double x0, double y0, double x1, double y1);
  already_AddRefed<CanvasGradient>
    CreateRadialGradient(double x0, double y0, double r0, double x1, double y1,
                         double r1, ErrorResult& aError);
  already_AddRefed<CanvasPattern>
    CreatePattern(const CanvasImageSource& element,
                  const nsAString& repeat, ErrorResult& error);

  double ShadowOffsetX()
  {
    return CurrentState().shadowOffset.x;
  }

  void SetShadowOffsetX(double shadowOffsetX)
  {
    CurrentState().shadowOffset.x = ToFloat(shadowOffsetX);
  }

  double ShadowOffsetY()
  {
    return CurrentState().shadowOffset.y;
  }

  void SetShadowOffsetY(double shadowOffsetY)
  {
    CurrentState().shadowOffset.y = ToFloat(shadowOffsetY);
  }

  double ShadowBlur()
  {
    return CurrentState().shadowBlur;
  }

  void SetShadowBlur(double shadowBlur)
  {
    if (shadowBlur >= 0.0) {
      CurrentState().shadowBlur = ToFloat(shadowBlur);
    }
  }

  void GetShadowColor(nsAString& shadowColor)
  {
    StyleColorToString(CurrentState().shadowColor, shadowColor);
  }

  void GetFilter(nsAString& filter)
  {
    filter = CurrentState().filterString;
  }

  void SetShadowColor(const nsAString& shadowColor);
  void SetFilter(const nsAString& filter, mozilla::ErrorResult& error);
  void ClearRect(double x, double y, double w, double h);
  void FillRect(double x, double y, double w, double h);
  void StrokeRect(double x, double y, double w, double h);
  void BeginPath();
  void Fill(const CanvasWindingRule& winding);
  void Fill(const CanvasPath& path, const CanvasWindingRule& winding);
  void Stroke();
  void Stroke(const CanvasPath& path);
  void DrawFocusIfNeeded(mozilla::dom::Element& element, ErrorResult& aRv);
  bool DrawCustomFocusRing(mozilla::dom::Element& element);
  void Clip(const CanvasWindingRule& winding);
  void Clip(const CanvasPath& path, const CanvasWindingRule& winding);
  bool IsPointInPath(double x, double y, const CanvasWindingRule& winding);
  bool IsPointInPath(const CanvasPath& path, double x, double y, const CanvasWindingRule& winding);
  bool IsPointInStroke(double x, double y);
  bool IsPointInStroke(const CanvasPath& path, double x, double y);
  void FillText(const nsAString& text, double x, double y,
                const Optional<double>& maxWidth,
                mozilla::ErrorResult& error);
  void StrokeText(const nsAString& text, double x, double y,
                  const Optional<double>& maxWidth,
                  mozilla::ErrorResult& error);
  TextMetrics*
    MeasureText(const nsAString& rawText, mozilla::ErrorResult& error);

  void AddHitRegion(const HitRegionOptions& options, mozilla::ErrorResult& error);
  void RemoveHitRegion(const nsAString& id);
  void ClearHitRegions();

  void DrawImage(const CanvasImageSource& image,
                 double dx, double dy, mozilla::ErrorResult& error)
  {
    DrawImage(image, 0.0, 0.0, 0.0, 0.0, dx, dy, 0.0, 0.0, 0, error);
  }

  void DrawImage(const CanvasImageSource& image,
                 double dx, double dy, double dw, double dh,
                 mozilla::ErrorResult& error)
  {
    DrawImage(image, 0.0, 0.0, 0.0, 0.0, dx, dy, dw, dh, 2, error);
  }

  void DrawImage(const CanvasImageSource& image,
                 double sx, double sy, double sw, double sh, double dx,
                 double dy, double dw, double dh, mozilla::ErrorResult& error)
  {
    DrawImage(image, sx, sy, sw, sh, dx, dy, dw, dh, 6, error);
  }

  already_AddRefed<ImageData>
    CreateImageData(JSContext* cx, double sw, double sh,
                    mozilla::ErrorResult& error);
  already_AddRefed<ImageData>
    CreateImageData(JSContext* cx, ImageData& imagedata,
                    mozilla::ErrorResult& error);
  already_AddRefed<ImageData>
    GetImageData(JSContext* cx, double sx, double sy, double sw, double sh,
                 mozilla::ErrorResult& error);
  void PutImageData(ImageData& imageData,
                    double dx, double dy, mozilla::ErrorResult& error);
  void PutImageData(ImageData& imageData,
                    double dx, double dy, double dirtyX, double dirtyY,
                    double dirtyWidth, double dirtyHeight,
                    mozilla::ErrorResult& error);

  double LineWidth()
  {
    return CurrentState().lineWidth;
  }

  void SetLineWidth(double width)
  {
    if (width > 0.0) {
      CurrentState().lineWidth = ToFloat(width);
    }
  }
  void GetLineCap(nsAString& linecap);
  void SetLineCap(const nsAString& linecap);
  void GetLineJoin(nsAString& linejoin, mozilla::ErrorResult& error);
  void SetLineJoin(const nsAString& linejoin);

  double MiterLimit()
  {
    return CurrentState().miterLimit;
  }

  void SetMiterLimit(double miter)
  {
    if (miter > 0.0) {
      CurrentState().miterLimit = ToFloat(miter);
    }
  }

  void GetFont(nsAString& font)
  {
    font = GetFont();
  }

  void SetFont(const nsAString& font, mozilla::ErrorResult& error);
  void GetTextAlign(nsAString& textAlign);
  void SetTextAlign(const nsAString& textAlign);
  void GetTextBaseline(nsAString& textBaseline);
  void SetTextBaseline(const nsAString& textBaseline);

  void ClosePath()
  {
    EnsureWritablePath();

    if (mPathBuilder) {
      mPathBuilder->Close();
    } else {
      mDSPathBuilder->Close();
    }
  }

  void MoveTo(double x, double y)
  {
    EnsureWritablePath();

    if (mPathBuilder) {
      mPathBuilder->MoveTo(mozilla::gfx::Point(ToFloat(x), ToFloat(y)));
    } else {
      mDSPathBuilder->MoveTo(mTarget->GetTransform() *
                             mozilla::gfx::Point(ToFloat(x), ToFloat(y)));
    }
  }

  void LineTo(double x, double y)
  {
    EnsureWritablePath();

    LineTo(mozilla::gfx::Point(ToFloat(x), ToFloat(y)));
  }

  void QuadraticCurveTo(double cpx, double cpy, double x, double y)
  {
    EnsureWritablePath();

    if (mPathBuilder) {
      mPathBuilder->QuadraticBezierTo(mozilla::gfx::Point(ToFloat(cpx), ToFloat(cpy)),
                                      mozilla::gfx::Point(ToFloat(x), ToFloat(y)));
    } else {
      mozilla::gfx::Matrix transform = mTarget->GetTransform();
      mDSPathBuilder->QuadraticBezierTo(transform *
                                        mozilla::gfx::Point(ToFloat(cpx), ToFloat(cpy)),
                                        transform *
                                        mozilla::gfx::Point(ToFloat(x), ToFloat(y)));
    }
  }

  void BezierCurveTo(double cp1x, double cp1y, double cp2x, double cp2y, double x, double y)
  {
    EnsureWritablePath();

    BezierTo(mozilla::gfx::Point(ToFloat(cp1x), ToFloat(cp1y)),
             mozilla::gfx::Point(ToFloat(cp2x), ToFloat(cp2y)),
             mozilla::gfx::Point(ToFloat(x), ToFloat(y)));
  }

  void ArcTo(double x1, double y1, double x2, double y2, double radius,
             mozilla::ErrorResult& error);
  void Rect(double x, double y, double w, double h);
  void Arc(double x, double y, double radius, double startAngle,
           double endAngle, bool anticlockwise, mozilla::ErrorResult& error);

  void GetMozCurrentTransform(JSContext* cx,
                              JS::MutableHandle<JSObject*> result,
                              mozilla::ErrorResult& error) const;
  void SetMozCurrentTransform(JSContext* cx,
                              JS::Handle<JSObject*> currentTransform,
                              mozilla::ErrorResult& error);
  void GetMozCurrentTransformInverse(JSContext* cx,
                                     JS::MutableHandle<JSObject*> result,
                                     mozilla::ErrorResult& error) const;
  void SetMozCurrentTransformInverse(JSContext* cx,
                                     JS::Handle<JSObject*> currentTransform,
                                     mozilla::ErrorResult& error);
  void GetFillRule(nsAString& fillRule);
  void SetFillRule(const nsAString& fillRule);
  void GetMozDash(JSContext* cx, JS::MutableHandle<JS::Value> retval,
                  mozilla::ErrorResult& error);
  void SetMozDash(JSContext* cx, const JS::Value& mozDash,
                  mozilla::ErrorResult& error);

  void SetLineDash(const Sequence<double>& mSegments,
                   mozilla::ErrorResult& aRv);
  void GetLineDash(nsTArray<double>& mSegments) const;

  void SetLineDashOffset(double mOffset);
  double LineDashOffset() const;

  double MozDashOffset()
  {
    return CurrentState().dashOffset;
  }
  void SetMozDashOffset(double mozDashOffset);

  void GetMozTextStyle(nsAString& mozTextStyle)
  {
    GetFont(mozTextStyle);
  }

  void SetMozTextStyle(const nsAString& mozTextStyle,
                       mozilla::ErrorResult& error)
  {
    SetFont(mozTextStyle, error);
  }

  bool ImageSmoothingEnabled()
  {
    return CurrentState().imageSmoothingEnabled;
  }

  void SetImageSmoothingEnabled(bool imageSmoothingEnabled)
  {
    if (imageSmoothingEnabled != CurrentState().imageSmoothingEnabled) {
      CurrentState().imageSmoothingEnabled = imageSmoothingEnabled;
    }
  }

  void DrawWindow(nsGlobalWindow& window, double x, double y, double w, double h,
                  const nsAString& bgColor, uint32_t flags,
                  mozilla::ErrorResult& error);
  void DrawWidgetAsOnScreen(nsGlobalWindow& aWindow, mozilla::ErrorResult& error);
  void AsyncDrawXULElement(nsXULElement& elem, double x, double y, double w,
                           double h, const nsAString& bgColor, uint32_t flags,
                           mozilla::ErrorResult& error);

  enum RenderingMode {
    SoftwareBackendMode,
    OpenGLBackendMode,
    DefaultBackendMode
  };

  bool SwitchRenderingMode(RenderingMode aRenderingMode);

  // Eventually this should be deprecated. Keeping for now to keep the binding functional.
  void Demote();

  nsresult Redraw();

#ifdef DEBUG
    virtual int32_t GetWidth() const override;
    virtual int32_t GetHeight() const override;
#endif
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
  NS_IMETHOD SetDimensions(int32_t width, int32_t height) override;
  NS_IMETHOD InitializeWithSurface(nsIDocShell *shell, gfxASurface *surface, int32_t width, int32_t height) override;

  NS_IMETHOD GetInputStream(const char* aMimeType,
                            const char16_t* aEncoderOptions,
                            nsIInputStream **aStream) override;

  already_AddRefed<mozilla::gfx::SourceSurface> GetSurfaceSnapshot(bool* aPremultAlpha = nullptr) override
  {
    EnsureTarget();
    if (aPremultAlpha) {
      *aPremultAlpha = true;
    }
    return mTarget->Snapshot();
  }

  NS_IMETHOD SetIsOpaque(bool isOpaque) override;
  bool GetIsOpaque() override { return mOpaque; }
  NS_IMETHOD Reset() override;
  mozilla::layers::PersistentBufferProvider* GetBufferProvider(mozilla::layers::LayerManager* aManager);
  already_AddRefed<CanvasLayer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                               CanvasLayer *aOldLayer,
                                               LayerManager *aManager) override;
  virtual bool ShouldForceInactiveLayer(LayerManager *aManager) override;
  void MarkContextClean() override;
  void MarkContextCleanForFrameCapture() override;
  bool IsContextCleanForFrameCapture() override;
  NS_IMETHOD SetIsIPC(bool isIPC) override;
  // this rect is in canvas device space
  void Redraw(const mozilla::gfx::Rect &r);
  NS_IMETHOD Redraw(const gfxRect &r) override { Redraw(ToRect(r)); return NS_OK; }
  NS_IMETHOD SetContextOptions(JSContext* aCx, JS::Handle<JS::Value> aOptions) override;

  /**
   * An abstract base class to be implemented by callers wanting to be notified
   * that a refresh has occurred. Callers must ensure an observer is removed
   * before it is destroyed.
   */
  virtual void DidRefresh() override;

  // this rect is in mTarget's current user space
  void RedrawUser(const gfxRect &r);

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
      mDSPathBuilder->LineTo(mTarget->GetTransform() * aPoint);
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
      mDSPathBuilder->BezierTo(transform * aCP1,
                                transform * aCP2,
                                transform * aCP3);
    }
  }

  friend class CanvasRenderingContext2DUserData;

  virtual void GetImageBuffer(uint8_t** aImageBuffer, int32_t* aFormat) override;


  // Given a point, return hit region ID if it exists
  nsString GetHitRegion(const mozilla::gfx::Point& aPoint) override;


  // return true and fills in the bound rect if element has a hit region.
  bool GetHitRegionRect(Element* aElement, nsRect& aRect) override;

protected:
  nsresult GetImageDataArray(JSContext* aCx, int32_t aX, int32_t aY,
                             uint32_t aWidth, uint32_t aHeight,
                             JSObject** aRetval);

  nsresult PutImageData_explicit(int32_t x, int32_t y, uint32_t w, uint32_t h,
                                 dom::Uint8ClampedArray* aArray,
                                 bool hasDirtyRect, int32_t dirtyX, int32_t dirtyY,
                                 int32_t dirtyWidth, int32_t dirtyHeight);

  /**
   * Internal method to complete initialisation, expects mTarget to have been set
   */
  nsresult Initialize(int32_t width, int32_t height);

  nsresult InitializeWithTarget(mozilla::gfx::DrawTarget *surface,
                                int32_t width, int32_t height);

  /**
    * The number of living nsCanvasRenderingContexts.  When this goes down to
    * 0, we free the premultiply and unpremultiply tables, if they exist.
    */
  static uint32_t sNumLivingContexts;

  /**
    * Lookup table used to speed up GetImageData().
    */
  static uint8_t (*sUnpremultiplyTable)[256];

  /**
    * Lookup table used to speed up PutImageData().
    */
  static uint8_t (*sPremultiplyTable)[256];

  static mozilla::gfx::DrawTarget* sErrorTarget;

  // Some helpers.  Doesn't modify a color on failure.
  void SetStyleFromUnion(const StringOrCanvasGradientOrCanvasPattern& value,
                         Style whichStyle);
  void SetStyleFromString(const nsAString& str, Style whichStyle);

  void SetStyleFromGradient(CanvasGradient& gradient, Style whichStyle)
  {
    CurrentState().SetGradientStyle(whichStyle, &gradient);
  }

  void SetStyleFromPattern(CanvasPattern& pattern, Style whichStyle)
  {
    CurrentState().SetPatternStyle(whichStyle, &pattern);
  }

  void GetStyleAsUnion(OwningStringOrCanvasGradientOrCanvasPattern& aValue,
                       Style aWhichStyle);

  // Returns whether a color was successfully parsed.
  bool ParseColor(const nsAString& aString, nscolor* aColor);

  static void StyleColorToString(const nscolor& aColor, nsAString& aStr);

   // Returns whether a filter was successfully parsed.
  bool ParseFilter(const nsAString& aString,
                   nsTArray<nsStyleFilter>& aFilterChain,
                   ErrorResult& error);

  // Returns whether the font was successfully updated.
  bool SetFontInternal(const nsAString& font, mozilla::ErrorResult& error);


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
  void EnsureUserSpacePath(const CanvasWindingRule& winding = CanvasWindingRule::Nonzero);

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
  RenderingMode EnsureTarget(RenderingMode aRenderMode = RenderingMode::DefaultBackendMode);

  /**
   * Disposes an old target and prepares to lazily create a new target.
   */
  void ClearTarget();

  /*
   * Returns the target to the buffer provider. i.e. this will queue a frame for
   * rendering.
   */
  void ReturnTarget();

  /**
   * Check if the target is valid after calling EnsureTarget.
   */
  bool IsTargetValid() const {
    return (sErrorTarget == nullptr || mTarget != sErrorTarget) && (mBufferProvider != nullptr || mTarget);
  }

  /**
    * Returns the surface format this canvas should be allocated using. Takes
    * into account mOpaque, platform requirements, etc.
    */
  mozilla::gfx::SurfaceFormat GetSurfaceFormat() const;

  /**
   * Update CurrentState().filter with the filter description for
   * CurrentState().filterChain.
   */
  void UpdateFilter();

  nsLayoutUtils::SurfaceFromElementResult
    CachedSurfaceFromElement(Element* aElement);

  void DrawImage(const CanvasImageSource &imgElt,
                 double sx, double sy, double sw, double sh,
                 double dx, double dy, double dw, double dh,
                 uint8_t optional_argc, mozilla::ErrorResult& error);

  void DrawDirectlyToCanvas(const nsLayoutUtils::DirectDrawInfo& image,
                            mozilla::gfx::Rect* bounds,
                            mozilla::gfx::Rect dest,
                            mozilla::gfx::Rect src,
                            gfx::IntSize imgSize);

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

  static void AddDemotableContext(CanvasRenderingContext2D* context);
  static void RemoveDemotableContext(CanvasRenderingContext2D* context);

  RenderingMode mRenderingMode;

  // Texture informations for fast video rendering
  unsigned int mVideoTexture;
  nsIntSize mCurrentVideoSize;

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

  nsTArray<CanvasRenderingContext2DUserData*> mUserDatas;

  // If mCanvasElement is not provided, then a docshell is
  nsCOMPtr<nsIDocShell> mDocShell;

  // This is created lazily so it is necessary to call EnsureTarget before
  // accessing it. In the event of an error it will be equal to
  // sErrorTarget.
  mozilla::RefPtr<mozilla::gfx::DrawTarget> mTarget;

  mozilla::RefPtr<mozilla::layers::PersistentBufferProvider> mBufferProvider;

  uint32_t SkiaGLTex() const;

  // This observes our draw calls at the beginning of the canvas
  // lifetime and switches to software or GPU mode depending on
  // what it thinks is best
  CanvasDrawObserver* mDrawObserver;
  void RemoveDrawObserver();

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

  // This is stored after GetThebesSurface has been called once to avoid
  // excessive ThebesSurface initialization overhead.
  nsRefPtr<gfxASurface> mThebesSurface;

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
  mozilla::RefPtr<mozilla::gfx::Path> mPath;
  mozilla::RefPtr<mozilla::gfx::PathBuilder> mDSPathBuilder;
  mozilla::RefPtr<mozilla::gfx::PathBuilder> mPathBuilder;
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
    nsRefPtr<Element> mElement;
    // Path of the hit region in the 2d context coordinate space (not user space)
    RefPtr<gfx::Path> mPath;
  };

  nsTArray<RegionInfo> mHitRegionsOptions;

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
    const ContextState& state = CurrentState();
    return state.filter.mPrimitives.Length() > 0;
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
  nsresult DrawOrMeasureText(const nsAString& text,
                             float x,
                             float y,
                             const Optional<double>& maxWidth,
                             TextDrawOperation op,
                             float* aWidth);

  bool CheckSizeForSkiaGL(mozilla::gfx::IntSize size);

  // state stack handling
  class ContextState {
  public:
    ContextState() : textAlign(TextAlign::START),
                     textBaseline(TextBaseline::ALPHABETIC),
                     lineWidth(1.0f),
                     miterLimit(10.0f),
                     globalAlpha(1.0f),
                     shadowBlur(0.0),
                     dashOffset(0.0f),
                     op(mozilla::gfx::CompositionOp::OP_OVER),
                     fillRule(mozilla::gfx::FillRule::FILL_WINDING),
                     lineCap(mozilla::gfx::CapStyle::BUTT),
                     lineJoin(mozilla::gfx::JoinStyle::MITER_OR_BEVEL),
                     filterString(MOZ_UTF16("none")),
                     imageSmoothingEnabled(true),
                     fontExplicitLanguage(false)
    { }

    ContextState(const ContextState& other)
        : fontGroup(other.fontGroup),
          fontLanguage(other.fontLanguage),
          fontFont(other.fontFont),
          gradientStyles(other.gradientStyles),
          patternStyles(other.patternStyles),
          colorStyles(other.colorStyles),
          font(other.font),
          textAlign(other.textAlign),
          textBaseline(other.textBaseline),
          shadowColor(other.shadowColor),
          transform(other.transform),
          shadowOffset(other.shadowOffset),
          lineWidth(other.lineWidth),
          miterLimit(other.miterLimit),
          globalAlpha(other.globalAlpha),
          shadowBlur(other.shadowBlur),
          dash(other.dash),
          dashOffset(other.dashOffset),
          op(other.op),
          fillRule(other.fillRule),
          lineCap(other.lineCap),
          lineJoin(other.lineJoin),
          filterString(other.filterString),
          filterChain(other.filterChain),
          filterChainObserver(other.filterChainObserver),
          filter(other.filter),
          filterAdditionalImages(other.filterAdditionalImages),
          imageSmoothingEnabled(other.imageSmoothingEnabled),
          fontExplicitLanguage(other.fontExplicitLanguage)
    { }

    void SetColorStyle(Style whichStyle, nscolor color)
    {
      colorStyles[whichStyle] = color;
      gradientStyles[whichStyle] = nullptr;
      patternStyles[whichStyle] = nullptr;
    }

    void SetPatternStyle(Style whichStyle, CanvasPattern* pat)
    {
      gradientStyles[whichStyle] = nullptr;
      patternStyles[whichStyle] = pat;
    }

    void SetGradientStyle(Style whichStyle, CanvasGradient* grad)
    {
      gradientStyles[whichStyle] = grad;
      patternStyles[whichStyle] = nullptr;
    }

    /**
      * returns true iff the given style is a solid color.
      */
    bool StyleIsColor(Style whichStyle) const
    {
      return !(patternStyles[whichStyle] || gradientStyles[whichStyle]);
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

    std::vector<mozilla::RefPtr<mozilla::gfx::Path> > clipsPushed;

    nsRefPtr<gfxFontGroup> fontGroup;
    nsCOMPtr<nsIAtom> fontLanguage;
    nsFont fontFont;

    EnumeratedArray<Style, Style::MAX, nsRefPtr<CanvasGradient>> gradientStyles;
    EnumeratedArray<Style, Style::MAX, nsRefPtr<CanvasPattern>> patternStyles;
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
    nsRefPtr<nsSVGFilterChainObserver> filterChainObserver;
    mozilla::gfx::FilterDescription filter;
    nsTArray<mozilla::RefPtr<mozilla::gfx::SourceSurface>> filterAdditionalImages;

    bool imageSmoothingEnabled;
    bool fontExplicitLanguage;
  };

  nsAutoTArray<ContextState, 3> mStyleStack;

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
  void GetAppUnitsValues(int32_t *perDevPixel, int32_t *perCSSPixel)
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
    if (perDevPixel)
      *perDevPixel = devPixel;
    if (perCSSPixel)
      *perCSSPixel = cssPixel;
  }

  friend struct CanvasBidiProcessor;
  friend class CanvasDrawObserver;
};

} // namespace dom
} // namespace mozilla

#endif /* CanvasRenderingContext2D_h */
