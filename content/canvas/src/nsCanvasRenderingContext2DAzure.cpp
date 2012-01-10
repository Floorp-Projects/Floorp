/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "base/basictypes.h"

#include "nsIDOMXULElement.h"

#include "prmem.h"
#include "prenv.h"

#include "nsIServiceManager.h"
#include "nsMathUtils.h"

#include "nsContentUtils.h"

#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsHTMLCanvasElement.h"
#include "nsSVGEffects.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIVariant.h"

#include "nsIInterfaceRequestorUtils.h"
#include "nsIFrame.h"
#include "nsDOMError.h"
#include "nsIScriptError.h"

#include "nsCSSParser.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/css/Declaration.h"
#include "nsComputedDOMStyle.h"
#include "nsStyleSet.h"

#include "nsPrintfCString.h"

#include "nsReadableUtils.h"

#include "nsColor.h"
#include "nsGfxCIID.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDocShell.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "nsDisplayList.h"

#include "nsTArray.h"

#include "imgIEncoder.h"

#include "gfxContext.h"
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "gfxFont.h"
#include "gfxBlur.h"
#include "gfxUtils.h"

#include "nsFrameManager.h"
#include "nsFrameLoader.h"
#include "nsBidi.h"
#include "nsBidiPresUtils.h"
#include "Layers.h"
#include "CanvasUtils.h"
#include "nsIMemoryReporter.h"
#include "nsStyleUtil.h"
#include "CanvasImageCache.h"

#include <algorithm>
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/PDocumentRendererParent.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/ipc/DocumentRendererParent.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/Preferences.h"

#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#endif

// windows.h (included by chromium code) defines this, in its infinite wisdom
#undef DrawText

using namespace mozilla;
using namespace mozilla::CanvasUtils;
using namespace mozilla::css;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::ipc;
using namespace mozilla::layers;

namespace mgfx = mozilla::gfx;

static float kDefaultFontSize = 10.0;
static NS_NAMED_LITERAL_STRING(kDefaultFontName, "sans-serif");
static NS_NAMED_LITERAL_STRING(kDefaultFontStyle, "10px sans-serif");

/* Memory reporter stuff */
static nsIMemoryReporter *gCanvasAzureMemoryReporter = nsnull;
static PRInt64 gCanvasAzureMemoryUsed = 0;

static PRInt64 GetCanvasAzureMemoryUsed() {
  return gCanvasAzureMemoryUsed;
}

// This is KIND_OTHER because it's not always clear where in memory the pixels
// of a canvas are stored.  Furthermore, this memory will be tracked by the
// underlying surface implementations.  See bug 655638 for details.
NS_MEMORY_REPORTER_IMPLEMENT(CanvasAzureMemory,
  "canvas-2d-pixel-bytes",
  KIND_OTHER,
  UNITS_BYTES,
  GetCanvasAzureMemoryUsed,
  "Memory used by 2D canvases. Each canvas requires (width * height * 4) "
  "bytes.")

/**
 ** nsCanvasGradientAzure
 **/
#define NS_CANVASGRADIENTAZURE_PRIVATE_IID \
    {0x28425a6a, 0x90e0, 0x4d42, {0x9c, 0x75, 0xff, 0x60, 0x09, 0xb3, 0x10, 0xa8}}
class nsCanvasGradientAzure : public nsIDOMCanvasGradient
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CANVASGRADIENTAZURE_PRIVATE_IID)

  enum Type
  {
    LINEAR = 0,
    RADIAL
  };

  Type GetType()
  {
    return mType;
  }


  GradientStops *GetGradientStopsForTarget(DrawTarget *aRT)
  {
    if (mStops && mStops->GetBackendType() == aRT->GetType()) {
      return mStops;
    }

    mStops = aRT->CreateGradientStops(mRawStops.Elements(), mRawStops.Length());

    return mStops;
  }

  NS_DECL_ISUPPORTS

  /* nsIDOMCanvasGradient */
  NS_IMETHOD AddColorStop (float offset,
                            const nsAString& colorstr)
  {
    if (!FloatValidate(offset) || offset < 0.0 || offset > 1.0) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    nscolor color;
    nsCSSParser parser;
    nsresult rv = parser.ParseColorString(nsString(colorstr),
                                          nsnull, 0, &color);
    if (NS_FAILED(rv)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }

    mStops = nsnull;

    GradientStop newStop;

    newStop.offset = offset;
    newStop.color = Color::FromABGR(color);

    mRawStops.AppendElement(newStop);

    return NS_OK;
  }

protected:
  nsCanvasGradientAzure(Type aType) : mType(aType)
  {}

  nsTArray<GradientStop> mRawStops;
  RefPtr<GradientStops> mStops;
  Type mType;
};

class nsCanvasRadialGradientAzure : public nsCanvasGradientAzure
{
public:
  nsCanvasRadialGradientAzure(const Point &aBeginOrigin, Float aBeginRadius,
                              const Point &aEndOrigin, Float aEndRadius)
    : nsCanvasGradientAzure(RADIAL)
    , mCenter1(aBeginOrigin)
    , mCenter2(aEndOrigin)
    , mRadius1(aBeginRadius)
    , mRadius2(aEndRadius)
  {
  }

  Point mCenter1;
  Point mCenter2;
  Float mRadius1;
  Float mRadius2;
};

class nsCanvasLinearGradientAzure : public nsCanvasGradientAzure
{
public:
  nsCanvasLinearGradientAzure(const Point &aBegin, const Point &aEnd)
    : nsCanvasGradientAzure(LINEAR)
    , mBegin(aBegin)
    , mEnd(aEnd)
  {
  }

protected:
  friend class nsCanvasRenderingContext2DAzure;

  // Beginning of linear gradient.
  Point mBegin;
  // End of linear gradient.
  Point mEnd;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsCanvasGradientAzure, NS_CANVASGRADIENTAZURE_PRIVATE_IID)

NS_IMPL_ADDREF(nsCanvasGradientAzure)
NS_IMPL_RELEASE(nsCanvasGradientAzure)

// XXX
// DOMCI_DATA(CanvasGradient, nsCanvasGradientAzure)

NS_INTERFACE_MAP_BEGIN(nsCanvasGradientAzure)
  NS_INTERFACE_MAP_ENTRY(nsCanvasGradientAzure)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCanvasGradient)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CanvasGradient)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/**
 ** nsCanvasPatternAzure
 **/
#define NS_CANVASPATTERNAZURE_PRIVATE_IID \
    {0xc9bacc25, 0x28da, 0x421e, {0x9a, 0x4b, 0xbb, 0xd6, 0x93, 0x05, 0x12, 0xbc}}
class nsCanvasPatternAzure : public nsIDOMCanvasPattern
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CANVASPATTERNAZURE_PRIVATE_IID)

  enum RepeatMode
  {
    REPEAT,
    REPEATX,
    REPEATY,
    NOREPEAT
  };

  nsCanvasPatternAzure(SourceSurface* aSurface,
                       RepeatMode aRepeat,
                       nsIPrincipal* principalForSecurityCheck,
                       bool forceWriteOnly,
                       bool CORSUsed)
    : mSurface(aSurface)
    , mRepeat(aRepeat)
    , mPrincipal(principalForSecurityCheck)
    , mForceWriteOnly(forceWriteOnly)
    , mCORSUsed(CORSUsed)
  {
  }

  NS_DECL_ISUPPORTS

  RefPtr<SourceSurface> mSurface;
  const RepeatMode mRepeat;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  const bool mForceWriteOnly;
  const bool mCORSUsed;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsCanvasPatternAzure, NS_CANVASPATTERNAZURE_PRIVATE_IID)

NS_IMPL_ADDREF(nsCanvasPatternAzure)
NS_IMPL_RELEASE(nsCanvasPatternAzure)

// XXX
// DOMCI_DATA(CanvasPattern, nsCanvasPatternAzure)

NS_INTERFACE_MAP_BEGIN(nsCanvasPatternAzure)
  NS_INTERFACE_MAP_ENTRY(nsCanvasPatternAzure)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCanvasPattern)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CanvasPattern)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/**
 ** nsTextMetricsAzure
 **/
#define NS_TEXTMETRICSAZURE_PRIVATE_IID \
  {0x9793f9e7, 0x9dc1, 0x4e9c, {0x81, 0xc8, 0xfc, 0xa7, 0x14, 0xf4, 0x30, 0x79}}
class nsTextMetricsAzure : public nsIDOMTextMetrics
{
public:
  nsTextMetricsAzure(float w) : width(w) { }

  virtual ~nsTextMetricsAzure() { }

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_TEXTMETRICSAZURE_PRIVATE_IID)

  NS_IMETHOD GetWidth(float* w) {
    *w = width;
    return NS_OK;
  }

  NS_DECL_ISUPPORTS

private:
  float width;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsTextMetricsAzure, NS_TEXTMETRICSAZURE_PRIVATE_IID)

NS_IMPL_ADDREF(nsTextMetricsAzure)
NS_IMPL_RELEASE(nsTextMetricsAzure)

// XXX
// DOMCI_DATA(TextMetrics, nsTextMetricsAzure)

NS_INTERFACE_MAP_BEGIN(nsTextMetricsAzure)
  NS_INTERFACE_MAP_ENTRY(nsTextMetricsAzure)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTextMetrics)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TextMetrics)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

struct nsCanvasBidiProcessorAzure;

// Cap sigma to avoid overly large temp surfaces.
static const Float SIGMA_MAX = 100;

/**
 ** nsCanvasRenderingContext2DAzure
 **/
class nsCanvasRenderingContext2DAzure :
  public nsIDOMCanvasRenderingContext2D,
  public nsICanvasRenderingContextInternal
{
public:
  nsCanvasRenderingContext2DAzure();
  virtual ~nsCanvasRenderingContext2DAzure();

  nsresult Redraw();

  // nsICanvasRenderingContextInternal
  NS_IMETHOD SetCanvasElement(nsHTMLCanvasElement* aParentCanvas);
  NS_IMETHOD SetDimensions(PRInt32 width, PRInt32 height);
  NS_IMETHOD InitializeWithSurface(nsIDocShell *shell, gfxASurface *surface, PRInt32 width, PRInt32 height)
  { return NS_ERROR_NOT_IMPLEMENTED; }

  NS_IMETHOD Render(gfxContext *ctx, gfxPattern::GraphicsFilter aFilter);
  NS_IMETHOD GetInputStream(const char* aMimeType,
                            const PRUnichar* aEncoderOptions,
                            nsIInputStream **aStream);
  NS_IMETHOD GetThebesSurface(gfxASurface **surface);

  TemporaryRef<SourceSurface> GetSurfaceSnapshot()
  { return mTarget ? mTarget->Snapshot() : nsnull; }

  NS_IMETHOD SetIsOpaque(bool isOpaque);
  NS_IMETHOD Reset();
  already_AddRefed<CanvasLayer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                                CanvasLayer *aOldLayer,
                                                LayerManager *aManager);
  void MarkContextClean();
  NS_IMETHOD SetIsIPC(bool isIPC);
  // this rect is in canvas device space
  void Redraw(const mgfx::Rect &r);
  NS_IMETHOD Redraw(const gfxRect &r) { Redraw(ToRect(r)); return NS_OK; }

  // this rect is in mTarget's current user space
  nsresult RedrawUser(const gfxRect &r);

  // nsISupports interface + CC
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsCanvasRenderingContext2DAzure, nsIDOMCanvasRenderingContext2D)

  // nsIDOMCanvasRenderingContext2D interface
  NS_DECL_NSIDOMCANVASRENDERINGCONTEXT2D

  enum Style {
    STYLE_STROKE = 0,
    STYLE_FILL,
    STYLE_MAX
  };
  
  nsresult LineTo(const Point& aPoint);
  nsresult BezierTo(const Point& aCP1, const Point& aCP2, const Point& aCP3);

protected:
  nsresult InitializeWithTarget(DrawTarget *surface, PRInt32 width, PRInt32 height);

  /**
    * The number of living nsCanvasRenderingContexts.  When this goes down to
    * 0, we free the premultiply and unpremultiply tables, if they exist.
    */
  static PRUint32 sNumLivingContexts;

  /**
    * Lookup table used to speed up GetImageData().
    */
  static PRUint8 (*sUnpremultiplyTable)[256];

  /**
    * Lookup table used to speed up PutImageData().
    */
  static PRUint8 (*sPremultiplyTable)[256];

  // Some helpers.  Doesn't modify a color on failure.
  nsresult SetStyleFromStringOrInterface(const nsAString& aStr, nsISupports *aInterface, Style aWhichStyle);
  nsresult GetStyleAsStringOrInterface(nsAString& aStr, nsISupports **aInterface, PRInt32 *aType, Style aWhichStyle);

  void StyleColorToString(const nscolor& aColor, nsAString& aStr);

  /**
    * Creates the unpremultiply lookup table, if it doesn't exist.
    */
  void EnsureUnpremultiplyTable();

  /**
    * Creates the premultiply lookup table, if it doesn't exist.
    */
  void EnsurePremultiplyTable();

  /* This function ensures there is a writable pathbuilder available, this
   * pathbuilder may be working in user space or in device space or
   * device space.
   */
  void EnsureWritablePath();

  // Ensures a path in UserSpace is available.
  void EnsureUserSpacePath();

  void TransformWillUpdate();

  // Report the fillRule has changed.
  void FillRuleChanged();

  /**
    * Returns the surface format this canvas should be allocated using. Takes
    * into account mOpaque, platform requirements, etc.
    */
  SurfaceFormat GetSurfaceFormat() const;

  nsHTMLCanvasElement *HTMLCanvasElement() {
    return static_cast<nsHTMLCanvasElement*>(mCanvasElement.get());
  }

  // Member vars
  PRInt32 mWidth, mHeight;

  // This is true when the canvas is valid, false otherwise, this occurs when
  // for some reason initialization of the drawtarget fails. If the canvas
  // is invalid certain behavior is expected.
  bool mValid;
  // This is true when the canvas is valid, but of zero size, this requires
  // specific behavior on some operations.
  bool mZero;

  bool mOpaque;

  // This is true when the next time our layer is retrieved we need to
  // recreate it (i.e. our backing surface changed)
  bool mResetLayer;
  // This is needed for drawing in drawAsyncXULElement
  bool mIPC;

  // the canvas element we're a context of
  nsCOMPtr<nsIDOMHTMLCanvasElement> mCanvasElement;

  // If mCanvasElement is not provided, then a docshell is
  nsCOMPtr<nsIDocShell> mDocShell;

  // our drawing surfaces, contexts, and layers
  RefPtr<DrawTarget> mTarget;

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
  RefPtr<Path> mPath;
  RefPtr<PathBuilder> mDSPathBuilder;
  RefPtr<PathBuilder> mPathBuilder;
  bool mPathTransformWillUpdate;
  Matrix mPathToDS;

  /**
    * Number of times we've invalidated before calling redraw
    */
  PRUint32 mInvalidateCount;
  static const PRUint32 kCanvasMaxInvalidateCount = 100;

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
      (state.shadowBlur != 0 || state.shadowOffset.x != 0 || state.shadowOffset.y != 0);
  }

  CompositionOp UsedOperation()
  {
    if (NeedToDrawShadow()) {
      // In this case the shadow rendering will use the operator.
      return OP_OVER;
    }

    return CurrentState().op;
  }

  /**
    * Gets the pres shell from either the canvas element or the doc shell
    */
  nsIPresShell *GetPresShell() {
    nsCOMPtr<nsIContent> content = do_QueryObject(mCanvasElement);
    if (content) {
      return content->OwnerDoc()->GetShell();
    }
    if (mDocShell) {
      nsCOMPtr<nsIPresShell> shell;
      mDocShell->GetPresShell(getter_AddRefs(shell));
      return shell.get();
    }
    return nsnull;
  }

  // text
  enum TextAlign {
    TEXT_ALIGN_START,
    TEXT_ALIGN_END,
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_RIGHT,
    TEXT_ALIGN_CENTER
  };

  enum TextBaseline {
    TEXT_BASELINE_TOP,
    TEXT_BASELINE_HANGING,
    TEXT_BASELINE_MIDDLE,
    TEXT_BASELINE_ALPHABETIC,
    TEXT_BASELINE_IDEOGRAPHIC,
    TEXT_BASELINE_BOTTOM
  };

  gfxFontGroup *GetCurrentFontStyle();

  enum TextDrawOperation {
    TEXT_DRAW_OPERATION_FILL,
    TEXT_DRAW_OPERATION_STROKE,
    TEXT_DRAW_OPERATION_MEASURE
  };

  /*
    * Implementation of the fillText, strokeText, and measure functions with
    * the operation abstracted to a flag.
    */
  nsresult DrawOrMeasureText(const nsAString& text,
                              float x,
                              float y,
                              float maxWidth,
                              TextDrawOperation op,
                              float* aWidth);

  // state stack handling
  class ContextState {
  public:
      ContextState() : textAlign(TEXT_ALIGN_START),
                       textBaseline(TEXT_BASELINE_ALPHABETIC),
                       lineWidth(1.0f),
                       miterLimit(10.0f),
                       globalAlpha(1.0f),
                       shadowBlur(0.0),
                       dashOffset(0.0f),
                       op(OP_OVER),
                       fillRule(FILL_WINDING),
                       lineCap(CAP_BUTT),
                       lineJoin(JOIN_MITER_OR_BEVEL),
                       imageSmoothingEnabled(true)
      { }

      ContextState(const ContextState& other)
          : fontGroup(other.fontGroup),
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
            fillRule(FILL_WINDING),
            lineCap(other.lineCap),
            lineJoin(other.lineJoin),
            imageSmoothingEnabled(other.imageSmoothingEnabled)
      {
          for (int i = 0; i < STYLE_MAX; i++) {
              colorStyles[i] = other.colorStyles[i];
              gradientStyles[i] = other.gradientStyles[i];
              patternStyles[i] = other.patternStyles[i];
          }
      }

      void SetColorStyle(Style whichStyle, nscolor color) {
          colorStyles[whichStyle] = color;
          gradientStyles[whichStyle] = nsnull;
          patternStyles[whichStyle] = nsnull;
      }

      void SetPatternStyle(Style whichStyle, nsCanvasPatternAzure* pat) {
          gradientStyles[whichStyle] = nsnull;
          patternStyles[whichStyle] = pat;
      }

      void SetGradientStyle(Style whichStyle, nsCanvasGradientAzure* grad) {
          gradientStyles[whichStyle] = grad;
          patternStyles[whichStyle] = nsnull;
      }

      /**
        * returns true iff the given style is a solid color.
        */
      bool StyleIsColor(Style whichStyle) const
      {
          return !(patternStyles[whichStyle] ||
                    gradientStyles[whichStyle]);
      }


      std::vector<RefPtr<Path> > clipsPushed;

      nsRefPtr<gfxFontGroup> fontGroup;
      nsRefPtr<nsCanvasGradientAzure> gradientStyles[STYLE_MAX];
      nsRefPtr<nsCanvasPatternAzure> patternStyles[STYLE_MAX];

      nsString font;
      TextAlign textAlign;
      TextBaseline textBaseline;

      nscolor colorStyles[STYLE_MAX];
      nscolor shadowColor;

      Matrix transform;
      Point shadowOffset;
      Float lineWidth;
      Float miterLimit;
      Float globalAlpha;
      Float shadowBlur;
      FallibleTArray<Float> dash;
      Float dashOffset;

      CompositionOp op;
      FillRule fillRule;
      CapStyle lineCap;
      JoinStyle lineJoin;

      bool imageSmoothingEnabled;
  };

  class GeneralPattern
  {
  public:
    GeneralPattern() : mPattern(nsnull) {}
    ~GeneralPattern()
    {
      if (mPattern) {
        mPattern->~Pattern();
      }
    }

    Pattern& ForStyle(nsCanvasRenderingContext2DAzure *aCtx,
                      Style aStyle,
                      DrawTarget *aRT)
    {
      // This should only be called once or the mPattern destructor will
      // not be executed.
      NS_ASSERTION(!mPattern, "ForStyle() should only be called once on GeneralPattern!");

      const nsCanvasRenderingContext2DAzure::ContextState &state = aCtx->CurrentState();

      if (state.StyleIsColor(aStyle)) {
        mPattern = new (mColorPattern.addr()) ColorPattern(Color::FromABGR(state.colorStyles[aStyle]));
      } else if (state.gradientStyles[aStyle] &&
                 state.gradientStyles[aStyle]->GetType() == nsCanvasGradientAzure::LINEAR) {
        nsCanvasLinearGradientAzure *gradient =
          static_cast<nsCanvasLinearGradientAzure*>(state.gradientStyles[aStyle].get());

        mPattern = new (mLinearGradientPattern.addr())
          LinearGradientPattern(gradient->mBegin, gradient->mEnd,
                                gradient->GetGradientStopsForTarget(aRT));
      } else if (state.gradientStyles[aStyle] &&
                 state.gradientStyles[aStyle]->GetType() == nsCanvasGradientAzure::RADIAL) {
        nsCanvasRadialGradientAzure *gradient =
          static_cast<nsCanvasRadialGradientAzure*>(state.gradientStyles[aStyle].get());

        mPattern = new (mRadialGradientPattern.addr())
          RadialGradientPattern(gradient->mCenter1, gradient->mCenter2, gradient->mRadius1,
                                gradient->mRadius2, gradient->GetGradientStopsForTarget(aRT));
      } else if (state.patternStyles[aStyle]) {
        if (aCtx->mCanvasElement) {
          CanvasUtils::DoDrawImageSecurityCheck(aCtx->HTMLCanvasElement(),
                                                state.patternStyles[aStyle]->mPrincipal,
                                                state.patternStyles[aStyle]->mForceWriteOnly,
                                                state.patternStyles[aStyle]->mCORSUsed);
        }

        ExtendMode mode;
        if (state.patternStyles[aStyle]->mRepeat == nsCanvasPatternAzure::NOREPEAT) {
          mode = EXTEND_CLAMP;
        } else {
          mode = EXTEND_REPEAT;
        }
        mPattern = new (mSurfacePattern.addr())
          SurfacePattern(state.patternStyles[aStyle]->mSurface, mode);
      }

      return *mPattern;
    }

    union {
      AlignedStorage2<ColorPattern> mColorPattern;
      AlignedStorage2<LinearGradientPattern> mLinearGradientPattern;
      AlignedStorage2<RadialGradientPattern> mRadialGradientPattern;
      AlignedStorage2<SurfacePattern> mSurfacePattern;
    };
    Pattern *mPattern;
  };

  /* This is an RAII based class that can be used as a drawtarget for
   * operations that need a shadow drawn. It will automatically provide a
   * temporary target when needed, and if so blend it back with a shadow.
   *
   * aBounds specifies the bounds of the drawing operation that will be
   * drawn to the target, it is given in device space! This function will
   * change aBounds to incorporate shadow bounds. If this is NULL the drawing
   * operation will be assumed to cover an infinite rect.
   */
  class AdjustedTarget
  {
  public:
    AdjustedTarget(nsCanvasRenderingContext2DAzure *ctx,
                   mgfx::Rect *aBounds = nsnull)
      : mCtx(nsnull)
    {
      if (!ctx->NeedToDrawShadow()) {
        mTarget = ctx->mTarget;
        return;
      }
      mCtx = ctx;

      const ContextState &state = mCtx->CurrentState();

      mSigma = state.shadowBlur / 2.0f;

      if (mSigma > SIGMA_MAX) {
        mSigma = SIGMA_MAX;
      }
        
      Matrix transform = mCtx->mTarget->GetTransform();

      mTempRect = mgfx::Rect(0, 0, ctx->mWidth, ctx->mHeight);

      Float blurRadius = mSigma * 3;

      // We need to enlarge and possibly offset our temporary surface
      // so that things outside of the canvas may cast shadows.
      mTempRect.Inflate(Margin(blurRadius + NS_MAX<Float>(state.shadowOffset.x, 0),
                               blurRadius + NS_MAX<Float>(state.shadowOffset.y, 0),
                               blurRadius + NS_MAX<Float>(-state.shadowOffset.x, 0),
                               blurRadius + NS_MAX<Float>(-state.shadowOffset.y, 0)));

      if (aBounds) {
        // We actually include the bounds of the shadow blur, this makes it
        // easier to execute the actual blur on hardware, and shouldn't affect
        // the amount of pixels that need to be touched.
        aBounds->Inflate(Margin(blurRadius, blurRadius,
                                blurRadius, blurRadius));
        mTempRect = mTempRect.Intersect(*aBounds);
      }

      mTempRect.ScaleRoundOut(1.0f);

      transform._31 -= mTempRect.x;
      transform._32 -= mTempRect.y;
        
      mTarget =
        mCtx->mTarget->CreateSimilarDrawTarget(IntSize(int32_t(mTempRect.width), int32_t(mTempRect.height)),
                                               FORMAT_B8G8R8A8);

      if (!mTarget) {
        // XXX - Deal with the situation where our temp size is too big to
        // fit in a texture.
        mTarget = ctx->mTarget;
        mCtx = nsnull;
      } else {
        mTarget->SetTransform(transform);
      }
    }

    ~AdjustedTarget()
    {
      if (!mCtx) {
        return;
      }

      RefPtr<SourceSurface> snapshot = mTarget->Snapshot();
      
      mCtx->mTarget->DrawSurfaceWithShadow(snapshot, mTempRect.TopLeft(),
                                           Color::FromABGR(mCtx->CurrentState().shadowColor),
                                           mCtx->CurrentState().shadowOffset, mSigma,
                                           mCtx->CurrentState().op);
    }

    DrawTarget* operator->()
    {
      return mTarget;
    }

  private:
    RefPtr<DrawTarget> mTarget;
    nsCanvasRenderingContext2DAzure *mCtx;
    Float mSigma;
    mgfx::Rect mTempRect;
  };

  nsAutoTArray<ContextState, 3> mStyleStack;

  inline ContextState& CurrentState() {
    return mStyleStack[mStyleStack.Length() - 1];
  }
    
  // other helpers
  void GetAppUnitsValues(PRUint32 *perDevPixel, PRUint32 *perCSSPixel) {
    // If we don't have a canvas element, we just return something generic.
    PRUint32 devPixel = 60;
    PRUint32 cssPixel = 60;

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

  friend struct nsCanvasBidiProcessorAzure;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsCanvasRenderingContext2DAzure)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsCanvasRenderingContext2DAzure)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsCanvasRenderingContext2DAzure)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsCanvasRenderingContext2DAzure)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCanvasElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsCanvasRenderingContext2DAzure)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCanvasElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// XXX
// DOMCI_DATA(CanvasRenderingContext2D, nsCanvasRenderingContext2DAzure)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsCanvasRenderingContext2DAzure)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCanvasRenderingContext2D)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMCanvasRenderingContext2D)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CanvasRenderingContext2D)
NS_INTERFACE_MAP_END

/**
 ** CanvasRenderingContext2D impl
 **/


// Initialize our static variables.
PRUint32 nsCanvasRenderingContext2DAzure::sNumLivingContexts = 0;
PRUint8 (*nsCanvasRenderingContext2DAzure::sUnpremultiplyTable)[256] = nsnull;
PRUint8 (*nsCanvasRenderingContext2DAzure::sPremultiplyTable)[256] = nsnull;

nsresult
NS_NewCanvasRenderingContext2DAzure(nsIDOMCanvasRenderingContext2D** aResult)
{
#ifdef XP_WIN
  if ((gfxWindowsPlatform::GetPlatform()->GetRenderMode() !=
      gfxWindowsPlatform::RENDER_DIRECT2D ||
      !gfxWindowsPlatform::GetPlatform()->DWriteEnabled()) &&
      !Preferences::GetBool("gfx.canvas.azure.prefer-skia", false)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
#elif !defined(XP_MACOSX) && !defined(ANDROID) && !defined(XP_LINUX)
  return NS_ERROR_NOT_AVAILABLE;
#endif

  nsRefPtr<nsIDOMCanvasRenderingContext2D> ctx = new nsCanvasRenderingContext2DAzure();
  if (!ctx)
    return NS_ERROR_OUT_OF_MEMORY;

  *aResult = ctx.forget().get();
  return NS_OK;
}

nsCanvasRenderingContext2DAzure::nsCanvasRenderingContext2DAzure()
  : mValid(false), mZero(false), mOpaque(false), mResetLayer(true)
  , mIPC(false)
  , mCanvasElement(nsnull)
  , mIsEntireFrameInvalid(false)
  , mPredictManyRedrawCalls(false), mPathTransformWillUpdate(false)
  , mInvalidateCount(0)
{
  sNumLivingContexts++;
}

nsCanvasRenderingContext2DAzure::~nsCanvasRenderingContext2DAzure()
{
  Reset();
  sNumLivingContexts--;
  if (!sNumLivingContexts) {
    delete[] sUnpremultiplyTable;
    delete[] sPremultiplyTable;
    sUnpremultiplyTable = nsnull;
    sPremultiplyTable = nsnull;
  }
}

nsresult
nsCanvasRenderingContext2DAzure::Reset()
{
  if (mCanvasElement) {
    HTMLCanvasElement()->InvalidateCanvas();
  }

  // only do this for non-docshell created contexts,
  // since those are the ones that we created a surface for
  if (mValid && !mDocShell) {
    gCanvasAzureMemoryUsed -= mWidth * mHeight * 4;
  }

  mTarget = nsnull;

  // Since the target changes the backing texture will change, and this will
  // no longer be valid.
  mThebesSurface = nsnull;
  mValid = false;
  mIsEntireFrameInvalid = false;
  mPredictManyRedrawCalls = false;

  return NS_OK;
}

nsresult
nsCanvasRenderingContext2DAzure::SetStyleFromStringOrInterface(const nsAString& aStr,
                                                               nsISupports *aInterface,
                                                               Style aWhichStyle)
{
  nsresult rv;
  nscolor color;

  if (!aStr.IsVoid()) {
    nsIDocument* document = mCanvasElement ?
                            HTMLCanvasElement()->OwnerDoc() : nsnull;

    // Pass the CSS Loader object to the parser, to allow parser error
    // reports to include the outer window ID.
    nsCSSParser parser(document ? document->CSSLoader() : nsnull);
    rv = parser.ParseColorString(aStr, nsnull, 0, &color);
    if (NS_FAILED(rv)) {
      // Error reporting happens inside the CSS parser
      return NS_OK;
    }

    CurrentState().SetColorStyle(aWhichStyle, color);
    return NS_OK;
  }

  if (aInterface) {
    nsCOMPtr<nsCanvasGradientAzure> grad(do_QueryInterface(aInterface));
    if (grad) {
      CurrentState().SetGradientStyle(aWhichStyle, grad);
      return NS_OK;
    }

    nsCOMPtr<nsCanvasPatternAzure> pattern(do_QueryInterface(aInterface));
    if (pattern) {
      CurrentState().SetPatternStyle(aWhichStyle, pattern);
      return NS_OK;
    }
  }

  nsContentUtils::ReportToConsole(
    nsIScriptError::warningFlag,
    "Canvas",
    mCanvasElement ? HTMLCanvasElement()->OwnerDoc() : nsnull,
    nsContentUtils::eDOM_PROPERTIES,
    "UnexpectedCanvasVariantStyle");

  return NS_OK;
}

nsresult
nsCanvasRenderingContext2DAzure::GetStyleAsStringOrInterface(nsAString& aStr,
                                                             nsISupports **aInterface,
                                                             PRInt32 *aType,
                                                             Style aWhichStyle)
{
  const ContextState &state = CurrentState();

  if (state.patternStyles[aWhichStyle]) {
    aStr.SetIsVoid(true);
    NS_ADDREF(*aInterface = state.patternStyles[aWhichStyle]);
    *aType = CMG_STYLE_PATTERN;
  } else if (state.gradientStyles[aWhichStyle]) {
    aStr.SetIsVoid(true);
    NS_ADDREF(*aInterface = state.gradientStyles[aWhichStyle]);
    *aType = CMG_STYLE_GRADIENT;
  } else {
    StyleColorToString(state.colorStyles[aWhichStyle], aStr);
    *aInterface = nsnull;
    *aType = CMG_STYLE_STRING;
  }

  return NS_OK;
}

void
nsCanvasRenderingContext2DAzure::StyleColorToString(const nscolor& aColor, nsAString& aStr)
{
  // We can't reuse the normal CSS color stringification code,
  // because the spec calls for a different algorithm for canvas.
  if (NS_GET_A(aColor) == 255) {
    CopyUTF8toUTF16(nsPrintfCString(100, "#%02x%02x%02x",
                                    NS_GET_R(aColor),
                                    NS_GET_G(aColor),
                                    NS_GET_B(aColor)),
                    aStr);
  } else {
    CopyUTF8toUTF16(nsPrintfCString(100, "rgba(%d, %d, %d, ",
                                    NS_GET_R(aColor),
                                    NS_GET_G(aColor),
                                    NS_GET_B(aColor)),
                    aStr);
    aStr.AppendFloat(nsStyleUtil::ColorComponentToFloat(NS_GET_A(aColor)));
    aStr.Append(')');
  }
}

nsresult
nsCanvasRenderingContext2DAzure::Redraw()
{
  if (mIsEntireFrameInvalid) {
    return NS_OK;
  }

  mIsEntireFrameInvalid = true;

  if (!mCanvasElement) {
    NS_ASSERTION(mDocShell, "Redraw with no canvas element or docshell!");
    return NS_OK;
  }

  if (mThebesSurface)
      mThebesSurface->MarkDirty();

  nsSVGEffects::InvalidateDirectRenderingObservers(HTMLCanvasElement());

  HTMLCanvasElement()->InvalidateCanvasContent(nsnull);

  return NS_OK;
}

void
nsCanvasRenderingContext2DAzure::Redraw(const mgfx::Rect &r)
{
  ++mInvalidateCount;

  if (mIsEntireFrameInvalid) {
    return;
  }

  if (mPredictManyRedrawCalls ||
    mInvalidateCount > kCanvasMaxInvalidateCount) {
    Redraw();
    return;
  }

  if (!mCanvasElement) {
    NS_ASSERTION(mDocShell, "Redraw with no canvas element or docshell!");
    return;
  }

  if (mThebesSurface)
      mThebesSurface->MarkDirty();

  nsSVGEffects::InvalidateDirectRenderingObservers(HTMLCanvasElement());

  gfxRect tmpR = ThebesRect(r);
  HTMLCanvasElement()->InvalidateCanvasContent(&tmpR);

  return;
}

nsresult
nsCanvasRenderingContext2DAzure::RedrawUser(const gfxRect& r)
{
  if (mIsEntireFrameInvalid) {
    ++mInvalidateCount;
    return NS_OK;
  }

  mgfx::Rect newr =
    mTarget->GetTransform().TransformBounds(ToRect(r));
  Redraw(newr);

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetDimensions(PRInt32 width, PRInt32 height)
{
  RefPtr<DrawTarget> target;

  // Zero sized surfaces cause issues, so just go with 1x1.
  if (height == 0 || width == 0) {
    mZero = true;
    height = 1;
    width = 1;
  } else {
    mZero = false;
  }

  // Check that the dimensions are sane
  IntSize size(width, height);
  if (size.width <= 0xFFFF && size.height <= 0xFFFF &&
      size.width >= 0 && size.height >= 0) {
    SurfaceFormat format = GetSurfaceFormat();
    nsCOMPtr<nsIContent> content = do_QueryObject(mCanvasElement);
    nsIDocument* ownerDoc = nsnull;
    if (content) {
      ownerDoc = content->OwnerDoc();
    }

    nsRefPtr<LayerManager> layerManager = nsnull;

    if (ownerDoc) {
      layerManager =
        nsContentUtils::PersistentLayerManagerForDocument(ownerDoc);
    }

    if (layerManager) {
      target = layerManager->CreateDrawTarget(size, format);
    } else {
      target = gfxPlatform::GetPlatform()->CreateOffscreenDrawTarget(size, format);
    }
  }

  if (target) {
    if (gCanvasAzureMemoryReporter == nsnull) {
        gCanvasAzureMemoryReporter = new NS_MEMORY_REPORTER_NAME(CanvasAzureMemory);
      NS_RegisterMemoryReporter(gCanvasAzureMemoryReporter);
    }

    gCanvasAzureMemoryUsed += width * height * 4;
    JSContext* context = nsContentUtils::GetCurrentJSContext();
    if (context) {
      JS_updateMallocCounter(context, width * height * 4);
    }
  }

  return InitializeWithTarget(target, width, height);
}

nsresult
nsCanvasRenderingContext2DAzure::InitializeWithTarget(DrawTarget *target, PRInt32 width, PRInt32 height)
{
  Reset();

  NS_ASSERTION(mCanvasElement, "Must have a canvas element!");
  mDocShell = nsnull;

  mWidth = width;
  mHeight = height;

  mTarget = target;

  mResetLayer = true;

  /* Create dummy surfaces here - target can be null when a canvas was created
   * that is too large to support.
   */
  if (!target)
  {
    mTarget = gfxPlatform::GetPlatform()->CreateOffscreenDrawTarget(IntSize(1, 1), FORMAT_B8G8R8A8);
    if (!mTarget) {
      // SupportsAzure() is controlled by the "gfx.canvas.azure.prefer-skia"
      // pref so that may be the reason rather than an OOM.
      mValid = false;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    mValid = true;
  }

  // set up the initial canvas defaults
  mStyleStack.Clear();
  mPathBuilder = nsnull;
  mPath = nsnull;
  mDSPathBuilder = nsnull;

  ContextState *state = mStyleStack.AppendElement();
  state->globalAlpha = 1.0;

  state->colorStyles[STYLE_FILL] = NS_RGB(0,0,0);
  state->colorStyles[STYLE_STROKE] = NS_RGB(0,0,0);
  state->shadowColor = NS_RGBA(0,0,0,0);

  mTarget->ClearRect(mgfx::Rect(Point(0, 0), Size(mWidth, mHeight)));
    
  // always force a redraw, because if the surface dimensions were reset
  // then the surface became cleared, and we need to redraw everything.
  Redraw();

  return mValid ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetIsOpaque(bool isOpaque)
{
  if (isOpaque == mOpaque)
    return NS_OK;

  mOpaque = isOpaque;

  if (mValid) {
    /* If we've already been created, let SetDimensions take care of
      * recreating our surface
      */
    return SetDimensions(mWidth, mHeight);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetIsIPC(bool isIPC)
{
  if (isIPC == mIPC)
      return NS_OK;

  mIPC = isIPC;

  if (mValid) {
    /* If we've already been created, let SetDimensions take care of
      * recreating our surface
      */
    return SetDimensions(mWidth, mHeight);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::Render(gfxContext *ctx, gfxPattern::GraphicsFilter aFilter)
{
  nsresult rv = NS_OK;

  if (!mValid || !mTarget) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<gfxASurface> surface;
  
  if (NS_FAILED(GetThebesSurface(getter_AddRefs(surface)))) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<gfxPattern> pat = new gfxPattern(surface);

  pat->SetFilter(aFilter);
  pat->SetExtend(gfxPattern::EXTEND_PAD);

  gfxContext::GraphicsOperator op = ctx->CurrentOperator();
  if (mOpaque)
      ctx->SetOperator(gfxContext::OPERATOR_SOURCE);

  // XXX I don't want to use PixelSnapped here, but layout doesn't guarantee
  // pixel alignment for this stuff!
  ctx->NewPath();
  ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
  ctx->Fill();

  if (mOpaque)
      ctx->SetOperator(op);

  return rv;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetInputStream(const char *aMimeType,
                                                const PRUnichar *aEncoderOptions,
                                                nsIInputStream **aStream)
{
  if (!mValid || !mTarget) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<gfxASurface> surface;

  if (NS_FAILED(GetThebesSurface(getter_AddRefs(surface)))) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  const char encoderPrefix[] = "@mozilla.org/image/encoder;2?type=";
  nsAutoArrayPtr<char> conid(new (std::nothrow) char[strlen(encoderPrefix) + strlen(aMimeType) + 1]);

  if (!conid) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  strcpy(conid, encoderPrefix);
  strcat(conid, aMimeType);

  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(conid);
  if (!encoder) {
    return NS_ERROR_FAILURE;
  }

  nsAutoArrayPtr<PRUint8> imageBuffer(new (std::nothrow) PRUint8[mWidth * mHeight * 4]);
  if (!imageBuffer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsRefPtr<gfxImageSurface> imgsurf =
    new gfxImageSurface(imageBuffer.get(),
                        gfxIntSize(mWidth, mHeight),
                        mWidth * 4,
                        gfxASurface::ImageFormatARGB32);

  if (!imgsurf || imgsurf->CairoStatus()) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<gfxContext> ctx = new gfxContext(imgsurf);

  if (!ctx || ctx->HasError()) {
    return NS_ERROR_FAILURE;
  }

  ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx->SetSource(surface, gfxPoint(0, 0));
  ctx->Paint();

  rv = encoder->InitFromData(imageBuffer.get(),
                              mWidth * mHeight * 4, mWidth, mHeight, mWidth * 4,
                              imgIEncoder::INPUT_FORMAT_HOSTARGB,
                              nsDependentString(aEncoderOptions));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(encoder, aStream);
}

SurfaceFormat
nsCanvasRenderingContext2DAzure::GetSurfaceFormat() const
{
  return mOpaque ? FORMAT_B8G8R8X8 : FORMAT_B8G8R8A8;
}

//
// nsCanvasRenderingContext2DAzure impl
//

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetCanvasElement(nsHTMLCanvasElement* aCanvasElement)
{
  mCanvasElement = aCanvasElement;

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetCanvas(nsIDOMHTMLCanvasElement **canvas)
{
  NS_IF_ADDREF(*canvas = mCanvasElement);

  return NS_OK;
}

//
// state
//

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::Save()
{
  mStyleStack[mStyleStack.Length() - 1].transform = mTarget->GetTransform();
  mStyleStack.SetCapacity(mStyleStack.Length() + 1);
  mStyleStack.AppendElement(CurrentState());
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::Restore()
{
  if (mStyleStack.Length() - 1 == 0)
    return NS_OK;

  for (PRUint32 i = 0; i < CurrentState().clipsPushed.size(); i++) {
    mTarget->PopClip();
  }

  mStyleStack.RemoveElementAt(mStyleStack.Length() - 1);

  TransformWillUpdate();

  mTarget->SetTransform(CurrentState().transform);

  return NS_OK;
}

//
// transformations
//

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::Scale(float x, float y)
{
  if (!FloatValidate(x,y))
    return NS_OK;

  TransformWillUpdate();

  Matrix newMatrix = mTarget->GetTransform();
  mTarget->SetTransform(newMatrix.Scale(x, y));
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::Rotate(float angle)
{
  if (!FloatValidate(angle))
    return NS_OK;

  TransformWillUpdate();

  Matrix rotation = Matrix::Rotation(angle);
  mTarget->SetTransform(rotation * mTarget->GetTransform());
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::Translate(float x, float y)
{
  if (!FloatValidate(x,y)) {
    return NS_OK;
  }

  TransformWillUpdate();

  Matrix newMatrix = mTarget->GetTransform();
  mTarget->SetTransform(newMatrix.Translate(x, y));
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::Transform(float m11, float m12, float m21, float m22, float dx, float dy)
{
  if (!FloatValidate(m11,m12,m21,m22,dx,dy)) {
    return NS_OK;
  }

  TransformWillUpdate();

  Matrix matrix(m11, m12, m21, m22, dx, dy);
  mTarget->SetTransform(matrix * mTarget->GetTransform());
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetTransform(float m11, float m12, float m21, float m22, float dx, float dy)
{
  if (!FloatValidate(m11,m12,m21,m22,dx,dy)) {
    return NS_OK;
  }

  TransformWillUpdate();

  Matrix matrix(m11, m12, m21, m22, dx, dy);
  mTarget->SetTransform(matrix);

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetMozCurrentTransform(JSContext* cx,
                                                        const jsval& matrix)
{
  nsresult rv;
  Matrix newCTM;

  if (!JSValToMatrix(cx, matrix, &newCTM, &rv)) {
    return rv;
  }

  mTarget->SetTransform(newCTM);

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetMozCurrentTransform(JSContext* cx,
                                                        jsval* matrix)
{
  return MatrixToJSVal(mTarget->GetTransform(), cx, matrix);
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetMozCurrentTransformInverse(JSContext* cx,
                                                               const jsval& matrix)
{
  nsresult rv;
  Matrix newCTMInverse;

  if (!JSValToMatrix(cx, matrix, &newCTMInverse, &rv)) {
    return rv;
  }

  // XXX ERRMSG we need to report an error to developers here! (bug 329026)
  if (newCTMInverse.Invert()) {
    mTarget->SetTransform(newCTMInverse);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetMozCurrentTransformInverse(JSContext* cx,
                                                               jsval* matrix)
{
  Matrix ctm = mTarget->GetTransform();

  if (!ctm.Invert()) {
    double NaN = JSVAL_TO_DOUBLE(JS_GetNaNValue(cx));
    ctm = Matrix(NaN, NaN, NaN, NaN, NaN, NaN);
  }

  return MatrixToJSVal(ctm, cx, matrix);
}

//
// colors
//

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetGlobalAlpha(float aGlobalAlpha)
{
  if (!FloatValidate(aGlobalAlpha) || aGlobalAlpha < 0.0 || aGlobalAlpha > 1.0) {
    return NS_OK;
  }

  CurrentState().globalAlpha = aGlobalAlpha;
    
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetGlobalAlpha(float *aGlobalAlpha)
{
  *aGlobalAlpha = CurrentState().globalAlpha;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetStrokeStyle(nsIVariant *aValue)
{
  if (!aValue)
      return NS_ERROR_FAILURE;

  nsString str;

  nsresult rv;
  PRUint16 vtype;
  rv = aValue->GetDataType(&vtype);
  NS_ENSURE_SUCCESS(rv, rv);

  if (vtype == nsIDataType::VTYPE_INTERFACE ||
      vtype == nsIDataType::VTYPE_INTERFACE_IS)
  {
    nsIID *iid;
    nsCOMPtr<nsISupports> sup;
    rv = aValue->GetAsInterface(&iid, getter_AddRefs(sup));
    NS_ENSURE_SUCCESS(rv, rv);
    if (iid) {
      NS_Free(iid);
    }

    str.SetIsVoid(true);
    return SetStrokeStyle_multi(str, sup);
  }

  rv = aValue->GetAsAString(str);
  NS_ENSURE_SUCCESS(rv, rv);

  return SetStrokeStyle_multi(str, nsnull);
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetStrokeStyle(nsIVariant **aResult)
{
  nsCOMPtr<nsIWritableVariant> wv = do_CreateInstance(NS_VARIANT_CONTRACTID);

  nsCOMPtr<nsISupports> sup;
  nsString str;
  PRInt32 t;
  nsresult rv = GetStrokeStyle_multi(str, getter_AddRefs(sup), &t);
  NS_ENSURE_SUCCESS(rv, rv);

  if (t == CMG_STYLE_STRING) {
    rv = wv->SetAsAString(str);
  } else if (t == CMG_STYLE_PATTERN) {
    rv = wv->SetAsInterface(NS_GET_IID(nsIDOMCanvasPattern),
                            sup);
  } else if (t == CMG_STYLE_GRADIENT) {
    rv = wv->SetAsInterface(NS_GET_IID(nsIDOMCanvasGradient),
                            sup);
  } else {
    NS_ERROR("Unknown type from GetStroke/FillStyle_multi!");
    return NS_ERROR_FAILURE;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  NS_IF_ADDREF(*aResult = wv.get());
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetFillStyle(nsIVariant *aValue)
{
  if (!aValue) {
    return NS_ERROR_FAILURE;
  }

  nsString str;
  nsresult rv;
  PRUint16 vtype;
  rv = aValue->GetDataType(&vtype);
  NS_ENSURE_SUCCESS(rv, rv);

  if (vtype == nsIDataType::VTYPE_INTERFACE ||
      vtype == nsIDataType::VTYPE_INTERFACE_IS)
  {
    nsIID *iid;
    nsCOMPtr<nsISupports> sup;
    rv = aValue->GetAsInterface(&iid, getter_AddRefs(sup));
    NS_ENSURE_SUCCESS(rv, rv);

    str.SetIsVoid(true);
    return SetFillStyle_multi(str, sup);
  }

  rv = aValue->GetAsAString(str);
  NS_ENSURE_SUCCESS(rv, rv);

  return SetFillStyle_multi(str, nsnull);
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetFillStyle(nsIVariant **aResult)
{
  nsCOMPtr<nsIWritableVariant> wv = do_CreateInstance(NS_VARIANT_CONTRACTID);

  nsCOMPtr<nsISupports> sup;
  nsString str;
  PRInt32 t;
  nsresult rv = GetFillStyle_multi(str, getter_AddRefs(sup), &t);
  NS_ENSURE_SUCCESS(rv, rv);

  if (t == CMG_STYLE_STRING) {
    rv = wv->SetAsAString(str);
  } else if (t == CMG_STYLE_PATTERN) {
    rv = wv->SetAsInterface(NS_GET_IID(nsIDOMCanvasPattern),
                            sup);
  } else if (t == CMG_STYLE_GRADIENT) {
    rv = wv->SetAsInterface(NS_GET_IID(nsIDOMCanvasGradient),
                            sup);
  } else {
    NS_ERROR("Unknown type from GetStroke/FillStyle_multi!");
    return NS_ERROR_FAILURE;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  NS_IF_ADDREF(*aResult = wv.get());
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetMozFillRule(const nsAString& aString)
{
  FillRule rule;

  if (aString.EqualsLiteral("evenodd"))
    rule = FILL_EVEN_ODD;
  else if (aString.EqualsLiteral("nonzero"))
    rule = FILL_WINDING;
  else
    return NS_OK;

  CurrentState().fillRule = rule;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetMozFillRule(nsAString& aString)
{
    switch (CurrentState().fillRule) {
    case FILL_WINDING:
        aString.AssignLiteral("nonzero"); break;
    case FILL_EVEN_ODD:
        aString.AssignLiteral("evenodd"); break;
    default:
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetStrokeStyle_multi(const nsAString& aStr, nsISupports *aInterface)
{
    return SetStyleFromStringOrInterface(aStr, aInterface, STYLE_STROKE);
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetStrokeStyle_multi(nsAString& aStr, nsISupports **aInterface, PRInt32 *aType)
{
    return GetStyleAsStringOrInterface(aStr, aInterface, aType, STYLE_STROKE);
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetFillStyle_multi(const nsAString& aStr, nsISupports *aInterface)
{
    return SetStyleFromStringOrInterface(aStr, aInterface, STYLE_FILL);
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetFillStyle_multi(nsAString& aStr, nsISupports **aInterface, PRInt32 *aType)
{
    return GetStyleAsStringOrInterface(aStr, aInterface, aType, STYLE_FILL);
}

//
// gradients and patterns
//
NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::CreateLinearGradient(float x0, float y0, float x1, float y1,
                                                      nsIDOMCanvasGradient **_retval)
{
  if (!FloatValidate(x0,y0,x1,y1)) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  nsRefPtr<nsIDOMCanvasGradient> grad =
    new nsCanvasLinearGradientAzure(Point(x0, y0), Point(x1, y1));

  *_retval = grad.forget().get();
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::CreateRadialGradient(float x0, float y0, float r0,
                                                      float x1, float y1, float r1,
                                                      nsIDOMCanvasGradient **_retval)
{
  if (!FloatValidate(x0,y0,r0,x1,y1,r1)) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  if (r0 < 0.0 || r1 < 0.0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsRefPtr<nsIDOMCanvasGradient> grad =
    new nsCanvasRadialGradientAzure(Point(x0, y0), r0, Point(x1, y1), r1);

  *_retval = grad.forget().get();
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::CreatePattern(nsIDOMHTMLElement *image,
                                               const nsAString& repeat,
                                               nsIDOMCanvasPattern **_retval)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(image);
  if (!content) {
    return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
  }

  nsCanvasPatternAzure::RepeatMode repeatMode =
    nsCanvasPatternAzure::NOREPEAT;

  if (repeat.IsEmpty() || repeat.EqualsLiteral("repeat")) {
    repeatMode = nsCanvasPatternAzure::REPEAT;
  } else if (repeat.EqualsLiteral("repeat-x")) {
    repeatMode = nsCanvasPatternAzure::REPEATX;
  } else if (repeat.EqualsLiteral("repeat-y")) {
    repeatMode = nsCanvasPatternAzure::REPEATY;
  } else if (repeat.EqualsLiteral("no-repeat")) {
    repeatMode = nsCanvasPatternAzure::NOREPEAT;
  } else {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  nsHTMLCanvasElement* canvas = nsHTMLCanvasElement::FromContent(content);
  if (canvas) {
    nsIntSize size = canvas->GetSize();
    if (size.width == 0 || size.height == 0) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    // Special case for Canvas, which could be an Azure canvas!
    nsICanvasRenderingContextInternal *srcCanvas = canvas->GetContextAtIndex(0);
    if (srcCanvas) {
      // This might not be an Azure canvas!
      RefPtr<SourceSurface> srcSurf = srcCanvas->GetSurfaceSnapshot();

      nsRefPtr<nsCanvasPatternAzure> pat =
        new nsCanvasPatternAzure(srcSurf, repeatMode, content->NodePrincipal(), canvas->IsWriteOnly(), false);

      *_retval = pat.forget().get();
      return NS_OK;
    }
  }

  // The canvas spec says that createPattern should use the first frame
  // of animated images
  nsLayoutUtils::SurfaceFromElementResult res =
    nsLayoutUtils::SurfaceFromElement(content->AsElement(),
      nsLayoutUtils::SFE_WANT_FIRST_FRAME | nsLayoutUtils::SFE_WANT_NEW_SURFACE);

  if (!res.mSurface) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Ignore nsnull cairo surfaces! See bug 666312.
  if (!res.mSurface->CairoSurface()) {
    return NS_OK;
  }

  RefPtr<SourceSurface> srcSurf =
    gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(mTarget, res.mSurface);

  nsRefPtr<nsCanvasPatternAzure> pat =
    new nsCanvasPatternAzure(srcSurf, repeatMode, res.mPrincipal,
                             res.mIsWriteOnly, res.mCORSUsed);

  *_retval = pat.forget().get();
  return NS_OK;
}

//
// shadows
//
NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetShadowOffsetX(float x)
{
  if (!FloatValidate(x)) {
    return NS_OK;
  }

  CurrentState().shadowOffset.x = x;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetShadowOffsetX(float *x)
{
  *x = static_cast<float>(CurrentState().shadowOffset.x);
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetShadowOffsetY(float y)
{
  if (!FloatValidate(y)) {
    return NS_OK;
  }

  CurrentState().shadowOffset.y = y;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetShadowOffsetY(float *y)
{
  *y = static_cast<float>(CurrentState().shadowOffset.y);
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetShadowBlur(float blur)
{
  if (!FloatValidate(blur) || blur < 0.0) {
    return NS_OK;
  }

  CurrentState().shadowBlur = blur;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetShadowBlur(float *blur)
{
  *blur = CurrentState().shadowBlur;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetShadowColor(const nsAString& colorstr)
{
  nsIDocument* document = mCanvasElement ?
                          HTMLCanvasElement()->OwnerDoc() : nsnull;

  // Pass the CSS Loader object to the parser, to allow parser error reports
  // to include the outer window ID.
  nsCSSParser parser(document ? document->CSSLoader() : nsnull);
  nscolor color;
  nsresult rv = parser.ParseColorString(colorstr, nsnull, 0, &color);
  if (NS_FAILED(rv)) {
    // Error reporting happens inside the CSS parser
    return NS_OK;
  }
  CurrentState().shadowColor = color;

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetShadowColor(nsAString& color)
{
  StyleColorToString(CurrentState().shadowColor, color);

  return NS_OK;
}

//
// rects
//

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::ClearRect(float x, float y, float w, float h)
{
  if (!FloatValidate(x,y,w,h)) {
    return NS_OK;
  }
 
  mTarget->ClearRect(mgfx::Rect(x, y, w, h));

  return RedrawUser(gfxRect(x, y, w, h));
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::FillRect(float x, float y, float w, float h)
{
  if (!FloatValidate(x,y,w,h)) {
    return NS_OK;
  }

  const ContextState &state = CurrentState();

  if (state.patternStyles[STYLE_FILL]) {
    nsCanvasPatternAzure::RepeatMode repeat = 
      state.patternStyles[STYLE_FILL]->mRepeat;
    // In the FillRect case repeat modes are easy to deal with.
    bool limitx = repeat == nsCanvasPatternAzure::NOREPEAT || repeat == nsCanvasPatternAzure::REPEATY;
    bool limity = repeat == nsCanvasPatternAzure::NOREPEAT || repeat == nsCanvasPatternAzure::REPEATX;

    IntSize patternSize =
      state.patternStyles[STYLE_FILL]->mSurface->GetSize();

    // We always need to execute painting for non-over operators, even if
    // we end up with w/h = 0.
    if (limitx) {
      if (x < 0) {
        w += x;
        if (w < 0) {
          w = 0;
        }

        x = 0;
      }
      if (x + w > patternSize.width) {
        w = patternSize.width - x;
        if (w < 0) {
          w = 0;
        }
      }
    }
    if (limity) {
      if (y < 0) {
        h += y;
        if (h < 0) {
          h = 0;
        }

        y = 0;
      }
      if (y + h > patternSize.height) {
        h = patternSize.height - y;
        if (h < 0) {
          h = 0;
        }
      }
    }
  }

  mgfx::Rect bounds;
  
  if (NeedToDrawShadow()) {
    bounds = mgfx::Rect(x, y, w, h);
    bounds = mTarget->GetTransform().TransformBounds(bounds);
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nsnull : &bounds)->
    FillRect(mgfx::Rect(x, y, w, h),
             GeneralPattern().ForStyle(this, STYLE_FILL, mTarget),
             DrawOptions(state.globalAlpha, UsedOperation()));

  return RedrawUser(gfxRect(x, y, w, h));
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::StrokeRect(float x, float y, float w, float h)
{
  if (!FloatValidate(x,y,w,h)) {
    return NS_OK;
  }

  const ContextState &state = CurrentState();

  mgfx::Rect bounds;
  
  if (NeedToDrawShadow()) {
    bounds = mgfx::Rect(x - state.lineWidth / 2.0f, y - state.lineWidth / 2.0f,
                        w + state.lineWidth, h + state.lineWidth);
    bounds = mTarget->GetTransform().TransformBounds(bounds);
  }

  if (!w && !h) {
    return NS_OK;
  } else if (!h) {
    CapStyle cap = CAP_BUTT;
    if (state.lineJoin == JOIN_ROUND) {
      cap = CAP_ROUND;
    }
    AdjustedTarget(this, bounds.IsEmpty() ? nsnull : &bounds)->
      StrokeLine(Point(x, y), Point(x + w, y),
                  GeneralPattern().ForStyle(this, STYLE_STROKE, mTarget),
                  StrokeOptions(state.lineWidth, state.lineJoin,
                                cap, state.miterLimit,
                                state.dash.Length(),
                                state.dash.Elements(),
                                state.dashOffset),
                  DrawOptions(state.globalAlpha, UsedOperation()));
    return NS_OK;
  } else if (!w) {
    CapStyle cap = CAP_BUTT;
    if (state.lineJoin == JOIN_ROUND) {
      cap = CAP_ROUND;
    }
    AdjustedTarget(this, bounds.IsEmpty() ? nsnull : &bounds)->
      StrokeLine(Point(x, y), Point(x, y + h),
                  GeneralPattern().ForStyle(this, STYLE_STROKE, mTarget),
                  StrokeOptions(state.lineWidth, state.lineJoin,
                                cap, state.miterLimit,
                                state.dash.Length(),
                                state.dash.Elements(),
                                state.dashOffset),
                  DrawOptions(state.globalAlpha, UsedOperation()));
    return NS_OK;
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nsnull : &bounds)->
    StrokeRect(mgfx::Rect(x, y, w, h),
                GeneralPattern().ForStyle(this, STYLE_STROKE, mTarget),
                StrokeOptions(state.lineWidth, state.lineJoin,
                              state.lineCap, state.miterLimit,
                              state.dash.Length(),
                              state.dash.Elements(),
                              state.dashOffset),
                DrawOptions(state.globalAlpha, UsedOperation()));

  return Redraw();
}

//
// path bits
//

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::BeginPath()
{
  mPath = nsnull;
  mPathBuilder = nsnull;
  mDSPathBuilder = nsnull;
  mPathTransformWillUpdate = false;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::ClosePath()
{
  EnsureWritablePath();

  if (mPathBuilder) {
    mPathBuilder->Close();
  } else {
    mDSPathBuilder->Close();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::Fill()
{
  EnsureUserSpacePath();

  if (!mPath) {
    return NS_OK;
  }

  mgfx::Rect bounds;

  if (NeedToDrawShadow()) {
    bounds = mPath->GetBounds(mTarget->GetTransform());
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nsnull : &bounds)->
    Fill(mPath, GeneralPattern().ForStyle(this, STYLE_FILL, mTarget),
         DrawOptions(CurrentState().globalAlpha, UsedOperation()));

  return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::Stroke()
{
  EnsureUserSpacePath();

  if (!mPath) {
    return NS_OK;
  }

  const ContextState &state = CurrentState();

  StrokeOptions strokeOptions(state.lineWidth, state.lineJoin,
                              state.lineCap, state.miterLimit,
                              state.dash.Length(), state.dash.Elements(),
                              state.dashOffset);

  mgfx::Rect bounds;
  if (NeedToDrawShadow()) {
    bounds =
      mPath->GetStrokedBounds(strokeOptions, mTarget->GetTransform());
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nsnull : &bounds)->
    Stroke(mPath, GeneralPattern().ForStyle(this, STYLE_STROKE, mTarget),
           strokeOptions, DrawOptions(state.globalAlpha, UsedOperation()));

  return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::Clip()
{
  EnsureUserSpacePath();

  if (!mPath) {
    return NS_OK;
  }

  mTarget->PushClip(mPath);
  CurrentState().clipsPushed.push_back(mPath);

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::MoveTo(float x, float y)
{
  if (!FloatValidate(x,y))
      return NS_OK;

  EnsureWritablePath();

  if (mPathBuilder) {
    mPathBuilder->MoveTo(Point(x, y));
  } else {
    mDSPathBuilder->MoveTo(mTarget->GetTransform() * Point(x, y));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::LineTo(float x, float y)
{
  if (!FloatValidate(x,y))
      return NS_OK;

  EnsureWritablePath();
    
  return LineTo(Point(x, y));;
}
  
nsresult 
nsCanvasRenderingContext2DAzure::LineTo(const Point& aPoint)
{
  if (mPathBuilder) {
    mPathBuilder->LineTo(aPoint);
  } else {
    mDSPathBuilder->LineTo(mTarget->GetTransform() * aPoint);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::QuadraticCurveTo(float cpx, float cpy, float x, float y)
{
  if (!FloatValidate(cpx, cpy, x, y)) {
    return NS_OK;
  }

  EnsureWritablePath();

  if (mPathBuilder) {
    mPathBuilder->QuadraticBezierTo(Point(cpx, cpy), Point(x, y));
  } else {
    Matrix transform = mTarget->GetTransform();
    mDSPathBuilder->QuadraticBezierTo(transform * Point(cpx, cpy), transform * Point(x, y));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::BezierCurveTo(float cp1x, float cp1y,
                                              float cp2x, float cp2y,
                                              float x, float y)
{
  if (!FloatValidate(cp1x, cp1y, cp2x, cp2y, x, y)) {
    return NS_OK;
  }

  EnsureWritablePath();

  return BezierTo(Point(cp1x, cp1y), Point(cp2x, cp2y), Point(x, y));
}

nsresult
nsCanvasRenderingContext2DAzure::BezierTo(const Point& aCP1,
                                          const Point& aCP2,
                                          const Point& aCP3)
{
  if (mPathBuilder) {
    mPathBuilder->BezierTo(aCP1, aCP2, aCP3);
  } else {
    Matrix transform = mTarget->GetTransform();
    mDSPathBuilder->BezierTo(transform * aCP1,
                              transform * aCP2,
                              transform * aCP3);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::ArcTo(float x1, float y1, float x2, float y2, float radius)
{
  if (!FloatValidate(x1, y1, x2, y2, radius)) {
    return NS_OK;
  }

  if (radius < 0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  EnsureWritablePath();

  // Current point in user space!
  Point p0;
  if (mPathBuilder) {
    p0 = mPathBuilder->CurrentPoint();
  } else {
    Matrix invTransform = mTarget->GetTransform();
    if (!invTransform.Invert()) {
      return NS_OK;
    }

    p0 = invTransform * mDSPathBuilder->CurrentPoint();
  }

  Point p1(x1, y1);
  Point p2(x2, y2);

  // Execute these calculations in double precision to avoid cumulative
  // rounding errors.
  double dir, a2, b2, c2, cosx, sinx, d, anx, any,
          bnx, bny, x3, y3, x4, y4, cx, cy, angle0, angle1;
  bool anticlockwise;

  if (p0 == p1 || p1 == p2 || radius == 0) {
    LineTo(p1.x, p1.y);
    return NS_OK;
  }

  // Check for colinearity
  dir = (p2.x - p1.x) * (p0.y - p1.y) + (p2.y - p1.y) * (p1.x - p0.x);
  if (dir == 0) {
    LineTo(p1.x, p1.y);
    return NS_OK;
  }


  // XXX - Math for this code was already available from the non-azure code
  // and would be well tested. Perhaps converting to bezier directly might
  // be more efficient longer run.
  a2 = (p0.x-x1)*(p0.x-x1) + (p0.y-y1)*(p0.y-y1);
  b2 = (x1-x2)*(x1-x2) + (y1-y2)*(y1-y2);
  c2 = (p0.x-x2)*(p0.x-x2) + (p0.y-y2)*(p0.y-y2);
  cosx = (a2+b2-c2)/(2*sqrt(a2*b2));

  sinx = sqrt(1 - cosx*cosx);
  d = radius / ((1 - cosx) / sinx);

  anx = (x1-p0.x) / sqrt(a2);
  any = (y1-p0.y) / sqrt(a2);
  bnx = (x1-x2) / sqrt(b2);
  bny = (y1-y2) / sqrt(b2);
  x3 = x1 - anx*d;
  y3 = y1 - any*d;
  x4 = x1 - bnx*d;
  y4 = y1 - bny*d;
  anticlockwise = (dir < 0);
  cx = x3 + any*radius*(anticlockwise ? 1 : -1);
  cy = y3 - anx*radius*(anticlockwise ? 1 : -1);
  angle0 = atan2((y3-cy), (x3-cx));
  angle1 = atan2((y4-cy), (x4-cx));


  LineTo(x3, y3);

  Arc(cx, cy, radius, angle0, angle1, anticlockwise);
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::Arc(float x, float y,
                                     float r,
                                     float startAngle, float endAngle,
                                     bool ccw)
{
  if (!FloatValidate(x, y, r, startAngle, endAngle)) {
    return NS_OK;
  }

  if (r < 0.0)
      return NS_ERROR_DOM_INDEX_SIZE_ERR;

  EnsureWritablePath();

  ArcToBezier(this, Point(x, y), r, startAngle, endAngle, ccw);
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::Rect(float x, float y, float w, float h)
{
  if (!FloatValidate(x, y, w, h)) {
    return NS_OK;
  }

  EnsureWritablePath();

  if (mPathBuilder) {
    mPathBuilder->MoveTo(Point(x, y));
    mPathBuilder->LineTo(Point(x + w, y));
    mPathBuilder->LineTo(Point(x + w, y + h));
    mPathBuilder->LineTo(Point(x, y + h));
    mPathBuilder->Close();
  } else {
    mDSPathBuilder->MoveTo(mTarget->GetTransform() * Point(x, y));
    mDSPathBuilder->LineTo(mTarget->GetTransform() * Point(x + w, y));
    mDSPathBuilder->LineTo(mTarget->GetTransform() * Point(x + w, y + h));
    mDSPathBuilder->LineTo(mTarget->GetTransform() * Point(x, y + h));
    mDSPathBuilder->Close();
  }

  return NS_OK;
}

void
nsCanvasRenderingContext2DAzure::EnsureWritablePath()
{
  if (mDSPathBuilder) {
    return;
  }

  FillRule fillRule = CurrentState().fillRule;

  if (mPathBuilder) {
    if (mPathTransformWillUpdate) {
      mPath = mPathBuilder->Finish();
      mDSPathBuilder =
        mPath->TransformedCopyToBuilder(mPathToDS, fillRule);
      mPath = nsnull;
      mPathBuilder = nsnull;
    }
    return;
  }

  if (!mPath) {
    mPathBuilder = mTarget->CreatePathBuilder(fillRule);
  } else if (!mPathTransformWillUpdate) {
    mPathBuilder = mPath->CopyToBuilder(fillRule);
  } else {
    mDSPathBuilder =
      mPath->TransformedCopyToBuilder(mPathToDS, fillRule);
  }
}

void
nsCanvasRenderingContext2DAzure::EnsureUserSpacePath()
{
  FillRule fillRule = CurrentState().fillRule;

  if (!mPath && !mPathBuilder && !mDSPathBuilder) {
    mPathBuilder = mTarget->CreatePathBuilder(fillRule);
  }

  if (mPathBuilder) {
    mPath = mPathBuilder->Finish();
    mPathBuilder = nsnull;
  }

  if (mPath && mPathTransformWillUpdate) {
    mDSPathBuilder =
      mPath->TransformedCopyToBuilder(mPathToDS, fillRule);
    mPath = nsnull;
    mPathTransformWillUpdate = false;
  }

  if (mDSPathBuilder) {
    RefPtr<Path> dsPath;
    dsPath = mDSPathBuilder->Finish();
    mDSPathBuilder = nsnull;

    Matrix inverse = mTarget->GetTransform();
    if (!inverse.Invert()) {
      return;
    }

    mPathBuilder =
      dsPath->TransformedCopyToBuilder(inverse, fillRule);
    mPath = mPathBuilder->Finish();
    mPathBuilder = nsnull;
  }

  if (mPath && mPath->GetFillRule() != fillRule) {
    mPathBuilder = mPath->CopyToBuilder(fillRule);
    mPath = mPathBuilder->Finish();
  }
}

void
nsCanvasRenderingContext2DAzure::TransformWillUpdate()
{
  // Store the matrix that would transform the current path to device
  // space.
  if (mPath || mPathBuilder) {
    if (!mPathTransformWillUpdate) {
      // If the transform has already been updated, but a device space builder
      // has not been created yet mPathToDS contains the right transform to
      // transform the current mPath into device space.
      // We should leave it alone.
      mPathToDS = mTarget->GetTransform();
    }
    mPathTransformWillUpdate = true;
  }
}

//
// text
//

/**
 * Helper function for SetFont that creates a style rule for the given font.
 * @param aFont The CSS font string
 * @param aNode The canvas element
 * @param aResult Pointer in which to place the new style rule.
 * @remark Assumes all pointer arguments are non-null.
 */
static nsresult
CreateFontStyleRule(const nsAString& aFont,
                    nsINode* aNode,
                    StyleRule** aResult)
{
  nsRefPtr<StyleRule> rule;
  bool changed;

  nsIPrincipal* principal = aNode->NodePrincipal();
  nsIDocument* document = aNode->OwnerDoc();

  nsIURI* docURL = document->GetDocumentURI();
  nsIURI* baseURL = document->GetDocBaseURI();

  // Pass the CSS Loader object to the parser, to allow parser error reports
  // to include the outer window ID.
  nsCSSParser parser(document->CSSLoader());

  nsresult rv = parser.ParseStyleAttribute(EmptyString(), docURL, baseURL,
                                            principal, getter_AddRefs(rule));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = parser.ParseProperty(eCSSProperty_font, aFont, docURL, baseURL,
                            principal, rule->GetDeclaration(), &changed,
                            false);
  if (NS_FAILED(rv))
    return rv;

  rv = parser.ParseProperty(eCSSProperty_line_height,
                            NS_LITERAL_STRING("normal"), docURL, baseURL,
                            principal, rule->GetDeclaration(), &changed,
                            false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rule->RuleMatched();

  rule.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetFont(const nsAString& font)
{
  nsresult rv;

  /*
    * If font is defined with relative units (e.g. ems) and the parent
    * style context changes in between calls, setting the font to the
    * same value as previous could result in a different computed value,
    * so we cannot have the optimization where we check if the new font
    * string is equal to the old one.
    */

  nsCOMPtr<nsIContent> content = do_QueryInterface(mCanvasElement);
  if (!content && !mDocShell) {
      NS_WARNING("Canvas element must be an nsIContent and non-null or a docshell must be provided");
      return NS_ERROR_FAILURE;
  }

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }
  nsIDocument* document = presShell->GetDocument();

  nsCOMArray<nsIStyleRule> rules;

  nsRefPtr<css::StyleRule> rule;
  rv = CreateFontStyleRule(font, document, getter_AddRefs(rule));

  if (NS_FAILED(rv)) {
    return rv;
  }

  css::Declaration *declaration = rule->GetDeclaration();
  // The easiest way to see whether we got a syntax error or whether
  // we got 'inherit' or 'initial' is to look at font-size-adjust,
  // which the shorthand resets to either 'none' or
  // '-moz-system-font'.
  // We know the declaration is not !important, so we can use
  // GetNormalBlock().
  const nsCSSValue *fsaVal =
    declaration->GetNormalBlock()->ValueFor(eCSSProperty_font_size_adjust);
  if (!fsaVal || (fsaVal->GetUnit() != eCSSUnit_None &&
                  fsaVal->GetUnit() != eCSSUnit_System_Font)) {
      // We got an all-property value or a syntax error.  The spec says
      // this value must be ignored.
    return NS_OK;
  }

  rules.AppendObject(rule);

  nsStyleSet* styleSet = presShell->StyleSet();

  // have to get a parent style context for inherit-like relative
  // values (2em, bolder, etc.)
  nsRefPtr<nsStyleContext> parentContext;

  if (content && content->IsInDoc()) {
      // inherit from the canvas element
      parentContext = nsComputedDOMStyle::GetStyleContextForElement(
              content->AsElement(),
              nsnull,
              presShell);
  } else {
    // otherwise inherit from default (10px sans-serif)
    nsRefPtr<css::StyleRule> parentRule;
    rv = CreateFontStyleRule(NS_LITERAL_STRING("10px sans-serif"),
                              document,
                              getter_AddRefs(parentRule));

    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMArray<nsIStyleRule> parentRules;
    parentRules.AppendObject(parentRule);
    parentContext = styleSet->ResolveStyleForRules(nsnull, parentRules);
  }

  if (!parentContext) {
      return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsStyleContext> sc =
      styleSet->ResolveStyleForRules(parentContext, rules);
  if (!sc) {
    return NS_ERROR_FAILURE;
  }

  const nsStyleFont* fontStyle = sc->GetStyleFont();

  NS_ASSERTION(fontStyle, "Could not obtain font style");

  nsIAtom* language = sc->GetStyleVisibility()->mLanguage;
  if (!language) {
    language = presShell->GetPresContext()->GetLanguageFromCharset();
  }

  // use CSS pixels instead of dev pixels to avoid being affected by page zoom
  const PRUint32 aupcp = nsPresContext::AppUnitsPerCSSPixel();
  // un-zoom the font size to avoid being affected by text-only zoom
  const nscoord fontSize = nsStyleFont::UnZoomText(parentContext->PresContext(), fontStyle->mFont.size);

  bool printerFont = (presShell->GetPresContext()->Type() == nsPresContext::eContext_PrintPreview ||
                        presShell->GetPresContext()->Type() == nsPresContext::eContext_Print);

  gfxFontStyle style(fontStyle->mFont.style,
                      fontStyle->mFont.weight,
                      fontStyle->mFont.stretch,
                      NSAppUnitsToFloatPixels(fontSize, float(aupcp)),
                      language,
                      fontStyle->mFont.sizeAdjust,
                      fontStyle->mFont.systemFont,
                      printerFont,
                      fontStyle->mFont.featureSettings,
                      fontStyle->mFont.languageOverride);

  CurrentState().fontGroup =
      gfxPlatform::GetPlatform()->CreateFontGroup(fontStyle->mFont.name,
                                                  &style,
                                                  presShell->GetPresContext()->GetUserFontSet());
  NS_ASSERTION(CurrentState().fontGroup, "Could not get font group");

  // The font getter is required to be reserialized based on what we
  // parsed (including having line-height removed).  (Older drafts of
  // the spec required font sizes be converted to pixels, but that no
  // longer seems to be required.)
  declaration->GetValue(eCSSProperty_font, CurrentState().font);

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetFont(nsAString& font)
{
  /* will initilize the value if not set, else does nothing */
  GetCurrentFontStyle();

  font = CurrentState().font;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetTextAlign(const nsAString& ta)
{
  if (ta.EqualsLiteral("start"))
    CurrentState().textAlign = TEXT_ALIGN_START;
  else if (ta.EqualsLiteral("end"))
    CurrentState().textAlign = TEXT_ALIGN_END;
  else if (ta.EqualsLiteral("left"))
    CurrentState().textAlign = TEXT_ALIGN_LEFT;
  else if (ta.EqualsLiteral("right"))
    CurrentState().textAlign = TEXT_ALIGN_RIGHT;
  else if (ta.EqualsLiteral("center"))
    CurrentState().textAlign = TEXT_ALIGN_CENTER;

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetTextAlign(nsAString& ta)
{
  switch (CurrentState().textAlign)
  {
  case TEXT_ALIGN_START:
    ta.AssignLiteral("start");
    break;
  case TEXT_ALIGN_END:
    ta.AssignLiteral("end");
    break;
  case TEXT_ALIGN_LEFT:
    ta.AssignLiteral("left");
    break;
  case TEXT_ALIGN_RIGHT:
    ta.AssignLiteral("right");
    break;
  case TEXT_ALIGN_CENTER:
    ta.AssignLiteral("center");
    break;
  default:
    NS_ERROR("textAlign holds invalid value");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetTextBaseline(const nsAString& tb)
{
  if (tb.EqualsLiteral("top"))
    CurrentState().textBaseline = TEXT_BASELINE_TOP;
  else if (tb.EqualsLiteral("hanging"))
    CurrentState().textBaseline = TEXT_BASELINE_HANGING;
  else if (tb.EqualsLiteral("middle"))
    CurrentState().textBaseline = TEXT_BASELINE_MIDDLE;
  else if (tb.EqualsLiteral("alphabetic"))
    CurrentState().textBaseline = TEXT_BASELINE_ALPHABETIC;
  else if (tb.EqualsLiteral("ideographic"))
    CurrentState().textBaseline = TEXT_BASELINE_IDEOGRAPHIC;
  else if (tb.EqualsLiteral("bottom"))
    CurrentState().textBaseline = TEXT_BASELINE_BOTTOM;

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetTextBaseline(nsAString& tb)
{
  switch (CurrentState().textBaseline)
  {
  case TEXT_BASELINE_TOP:
    tb.AssignLiteral("top");
    break;
  case TEXT_BASELINE_HANGING:
    tb.AssignLiteral("hanging");
    break;
  case TEXT_BASELINE_MIDDLE:
    tb.AssignLiteral("middle");
    break;
  case TEXT_BASELINE_ALPHABETIC:
    tb.AssignLiteral("alphabetic");
    break;
  case TEXT_BASELINE_IDEOGRAPHIC:
    tb.AssignLiteral("ideographic");
    break;
  case TEXT_BASELINE_BOTTOM:
    tb.AssignLiteral("bottom");
    break;
  default:
    NS_ERROR("textBaseline holds invalid value");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

/*
 * Helper function that replaces the whitespace characters in a string
 * with U+0020 SPACE. The whitespace characters are defined as U+0020 SPACE,
 * U+0009 CHARACTER TABULATION (tab), U+000A LINE FEED (LF), U+000B LINE
 * TABULATION, U+000C FORM FEED (FF), and U+000D CARRIAGE RETURN (CR).
 * @param str The string whose whitespace characters to replace.
 */
static inline void
TextReplaceWhitespaceCharacters(nsAutoString& str)
{
  str.ReplaceChar("\x09\x0A\x0B\x0C\x0D", PRUnichar(' '));
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::FillText(const nsAString& text, float x, float y, float maxWidth)
{
  return DrawOrMeasureText(text, x, y, maxWidth, TEXT_DRAW_OPERATION_FILL, nsnull);
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::StrokeText(const nsAString& text, float x, float y, float maxWidth)
{
  return DrawOrMeasureText(text, x, y, maxWidth, TEXT_DRAW_OPERATION_STROKE, nsnull);
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::MeasureText(const nsAString& rawText,
                                      nsIDOMTextMetrics** _retval)
{
  float width;

  nsresult rv = DrawOrMeasureText(rawText, 0, 0, 0, TEXT_DRAW_OPERATION_MEASURE, &width);

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsRefPtr<nsIDOMTextMetrics> textMetrics = new nsTextMetricsAzure(width);
  if (!textMetrics.get()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *_retval = textMetrics.forget().get();

  return NS_OK;
}

/**
 * Used for nsBidiPresUtils::ProcessText
 */
struct NS_STACK_CLASS nsCanvasBidiProcessorAzure : public nsBidiPresUtils::BidiProcessor
{
  typedef nsCanvasRenderingContext2DAzure::ContextState ContextState;

  virtual void SetText(const PRUnichar* text, PRInt32 length, nsBidiDirection direction)
  {
    mTextRun = mFontgrp->MakeTextRun(text,
                                     length,
                                     mThebes,
                                     mAppUnitsPerDevPixel,
                                     direction==NSBIDI_RTL ? gfxTextRunFactory::TEXT_IS_RTL : 0);
  }

  virtual nscoord GetWidth()
  {
    gfxTextRun::Metrics textRunMetrics = mTextRun->MeasureText(0,
                                                               mTextRun->GetLength(),
                                                               mDoMeasureBoundingBox ?
                                                                 gfxFont::TIGHT_INK_EXTENTS :
                                                                 gfxFont::LOOSE_INK_EXTENTS,
                                                               mThebes,
                                                               nsnull);

    // this only measures the height; the total width is gotten from the
    // the return value of ProcessText.
    if (mDoMeasureBoundingBox) {
      textRunMetrics.mBoundingBox.Scale(1.0 / mAppUnitsPerDevPixel);
      mBoundingBox = mBoundingBox.Union(textRunMetrics.mBoundingBox);
    }

    return NSToCoordRound(textRunMetrics.mAdvanceWidth);
  }

  virtual void DrawText(nscoord xOffset, nscoord width)
  {
    gfxPoint point = mPt;
    point.x += xOffset;

    // offset is given in terms of left side of string
    if (mTextRun->IsRightToLeft()) {
      // Bug 581092 - don't use rounded pixel width to advance to
      // right-hand end of run, because this will cause different
      // glyph positioning for LTR vs RTL drawing of the same
      // glyph string on OS X and DWrite where textrun widths may
      // involve fractional pixels.
      gfxTextRun::Metrics textRunMetrics =
        mTextRun->MeasureText(0,
                              mTextRun->GetLength(),
                              mDoMeasureBoundingBox ?
                                  gfxFont::TIGHT_INK_EXTENTS :
                                  gfxFont::LOOSE_INK_EXTENTS,
                              mThebes,
                              nsnull);
      point.x += textRunMetrics.mAdvanceWidth;
      // old code was:
      //   point.x += width * mAppUnitsPerDevPixel;
      // TODO: restore this if/when we move to fractional coords
      // throughout the text layout process
    }

    PRUint32 numRuns;
    const gfxTextRun::GlyphRun *runs = mTextRun->GetGlyphRuns(&numRuns);
    const PRUint32 appUnitsPerDevUnit = mAppUnitsPerDevPixel;
    const double devUnitsPerAppUnit = 1.0/double(appUnitsPerDevUnit);
    Point baselineOrigin =
      Point(point.x * devUnitsPerAppUnit, point.y * devUnitsPerAppUnit);

    float advanceSum = 0;

    for (PRUint32 c = 0; c < numRuns; c++) {
      gfxFont *font = runs[c].mFont;
      PRUint32 endRun = 0;
      if (c + 1 < numRuns) {
        endRun = runs[c + 1].mCharacterOffset;
      } else {
        endRun = mTextRun->GetLength();
      }

      const gfxTextRun::CompressedGlyph *glyphs = mTextRun->GetCharacterGlyphs();

      RefPtr<ScaledFont> scaledFont =
        gfxPlatform::GetPlatform()->GetScaledFontForFont(font);

      GlyphBuffer buffer;

      std::vector<Glyph> glyphBuf;

      for (PRUint32 i = runs[c].mCharacterOffset; i < endRun; i++) {
        Glyph newGlyph;
        if (glyphs[i].IsSimpleGlyph()) {
          newGlyph.mIndex = glyphs[i].GetSimpleGlyph();
          if (mTextRun->IsRightToLeft()) {
            newGlyph.mPosition.x = baselineOrigin.x - advanceSum -
              glyphs[i].GetSimpleAdvance() * devUnitsPerAppUnit;
          } else {
            newGlyph.mPosition.x = baselineOrigin.x + advanceSum;
          }
          newGlyph.mPosition.y = baselineOrigin.y;
          advanceSum += glyphs[i].GetSimpleAdvance() * devUnitsPerAppUnit;
          glyphBuf.push_back(newGlyph);
          continue;
        }

        if (!glyphs[i].GetGlyphCount()) {
          continue;
        }

        gfxTextRun::DetailedGlyph *detailedGlyphs =
          mTextRun->GetDetailedGlyphs(i);

        for (PRUint32 c = 0; c < glyphs[i].GetGlyphCount(); c++) {
          newGlyph.mIndex = detailedGlyphs[c].mGlyphID;
          if (mTextRun->IsRightToLeft()) {
            newGlyph.mPosition.x = baselineOrigin.x + detailedGlyphs[c].mXOffset * devUnitsPerAppUnit -
              advanceSum - detailedGlyphs[c].mAdvance * devUnitsPerAppUnit;
          } else {
            newGlyph.mPosition.x = baselineOrigin.x + detailedGlyphs[c].mXOffset * devUnitsPerAppUnit + advanceSum;
          }
          newGlyph.mPosition.y = baselineOrigin.y + detailedGlyphs[c].mYOffset * devUnitsPerAppUnit;
          glyphBuf.push_back(newGlyph);
          advanceSum += detailedGlyphs[c].mAdvance * devUnitsPerAppUnit;
        }
      }

      if (!glyphBuf.size()) {
        // This may happen for glyph runs for a 0 size font.
        continue;
      }

      buffer.mGlyphs = &glyphBuf.front();
      buffer.mNumGlyphs = glyphBuf.size();

      if (mOp == nsCanvasRenderingContext2DAzure::TEXT_DRAW_OPERATION_FILL) {
        nsCanvasRenderingContext2DAzure::AdjustedTarget(mCtx)->
          FillGlyphs(scaledFont, buffer,
                      nsCanvasRenderingContext2DAzure::GeneralPattern().
                        ForStyle(mCtx, nsCanvasRenderingContext2DAzure::STYLE_FILL, mCtx->mTarget),
                      DrawOptions(mState->globalAlpha, mCtx->UsedOperation()));
      } else if (mOp == nsCanvasRenderingContext2DAzure::TEXT_DRAW_OPERATION_STROKE) {
        RefPtr<Path> path = scaledFont->GetPathForGlyphs(buffer, mCtx->mTarget);
            
        Matrix oldTransform = mCtx->mTarget->GetTransform();

        const ContextState& state = *mState;
        nsCanvasRenderingContext2DAzure::AdjustedTarget(mCtx)->
          Stroke(path, nsCanvasRenderingContext2DAzure::GeneralPattern().
                    ForStyle(mCtx, nsCanvasRenderingContext2DAzure::STYLE_STROKE, mCtx->mTarget),
                  StrokeOptions(state.lineWidth, state.lineJoin,
                                state.lineCap, state.miterLimit,
                                state.dash.Length(),
                                state.dash.Elements(),
                                state.dashOffset),
                  DrawOptions(state.globalAlpha, mCtx->UsedOperation()));

      }
    }
  }

  // current text run
  nsAutoPtr<gfxTextRun> mTextRun;

  // pointer to a screen reference context used to measure text and such
  nsRefPtr<gfxContext> mThebes;

  // Pointer to the draw target we should fill our text to
  nsCanvasRenderingContext2DAzure *mCtx;

  // position of the left side of the string, alphabetic baseline
  gfxPoint mPt;

  // current font
  gfxFontGroup* mFontgrp;

  // dev pixel conversion factor
  PRUint32 mAppUnitsPerDevPixel;

  // operation (fill or stroke)
  nsCanvasRenderingContext2DAzure::TextDrawOperation mOp;

  // context state
  ContextState *mState;

  // union of bounding boxes of all runs, needed for shadows
  gfxRect mBoundingBox;

  // true iff the bounding box should be measured
  bool mDoMeasureBoundingBox;
};

nsresult
nsCanvasRenderingContext2DAzure::DrawOrMeasureText(const nsAString& aRawText,
                                                   float aX,
                                                   float aY,
                                                   float aMaxWidth,
                                                   TextDrawOperation aOp,
                                                   float* aWidth)
{
  nsresult rv;

  if (!FloatValidate(aX, aY, aMaxWidth))
      return NS_ERROR_DOM_SYNTAX_ERR;

  // spec isn't clear on what should happen if aMaxWidth <= 0, so
  // treat it as an invalid argument
  // technically, 0 should be an invalid value as well, but 0 is the default
  // arg, and there is no way to tell if the default was used
  if (aMaxWidth < 0)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIContent> content = do_QueryInterface(mCanvasElement);
  if (!content && !mDocShell) {
      NS_WARNING("Canvas element must be an nsIContent and non-null or a docshell must be provided");
    return NS_ERROR_FAILURE;
  }

  nsIPresShell* presShell = GetPresShell();
  if (!presShell)
      return NS_ERROR_FAILURE;

  nsIDocument* document = presShell->GetDocument();

  // replace all the whitespace characters with U+0020 SPACE
  nsAutoString textToDraw(aRawText);
  TextReplaceWhitespaceCharacters(textToDraw);

  // for now, default to ltr if not in doc
  bool isRTL = false;

  if (content && content->IsInDoc()) {
    // try to find the closest context
    nsRefPtr<nsStyleContext> canvasStyle =
      nsComputedDOMStyle::GetStyleContextForElement(content->AsElement(),
                                                    nsnull,
                                                    presShell);
    if (!canvasStyle) {
      return NS_ERROR_FAILURE;
    }

    isRTL = canvasStyle->GetStyleVisibility()->mDirection ==
      NS_STYLE_DIRECTION_RTL;
  } else {
    isRTL = GET_BIDI_OPTION_DIRECTION(document->GetBidiOptions()) == IBMBIDI_TEXTDIRECTION_RTL;
  }

  const ContextState &state = CurrentState();

  // This is only needed to know if we can know the drawing bounding box easily.
  bool doDrawShadow = aOp == TEXT_DRAW_OPERATION_FILL && NeedToDrawShadow();

  nsCanvasBidiProcessorAzure processor;

  GetAppUnitsValues(&processor.mAppUnitsPerDevPixel, nsnull);
  processor.mPt = gfxPoint(aX, aY);
  processor.mThebes =
    new gfxContext(gfxPlatform::GetPlatform()->ScreenReferenceSurface());
  Matrix matrix = mTarget->GetTransform();
  processor.mThebes->SetMatrix(gfxMatrix(matrix._11, matrix._12, matrix._21, matrix._22, matrix._31, matrix._32));
  processor.mCtx = this;
  processor.mOp = aOp;
  processor.mBoundingBox = gfxRect(0, 0, 0, 0);
  processor.mDoMeasureBoundingBox = doDrawShadow || !mIsEntireFrameInvalid;
  processor.mState = &CurrentState();
    

  processor.mFontgrp = GetCurrentFontStyle();
  NS_ASSERTION(processor.mFontgrp, "font group is null");

  nscoord totalWidthCoord;

  // calls bidi algo twice since it needs the full text width and the
  // bounding boxes before rendering anything
  nsBidi bidiEngine;
  rv = nsBidiPresUtils::ProcessText(textToDraw.get(),
                                textToDraw.Length(),
                                isRTL ? NSBIDI_RTL : NSBIDI_LTR,
                                presShell->GetPresContext(),
                                processor,
                                nsBidiPresUtils::MODE_MEASURE,
                                nsnull,
                                0,
                                &totalWidthCoord,
                                &bidiEngine);
  if (NS_FAILED(rv)) {
    return rv;
  }

  float totalWidth = float(totalWidthCoord) / processor.mAppUnitsPerDevPixel;
  if (aWidth) {
    *aWidth = totalWidth;
  }

  // if only measuring, don't need to do any more work
  if (aOp==TEXT_DRAW_OPERATION_MEASURE) {
    return NS_OK;
  }

  // offset pt.x based on text align
  gfxFloat anchorX;

  if (state.textAlign == TEXT_ALIGN_CENTER) {
    anchorX = .5;
  } else if (state.textAlign == TEXT_ALIGN_LEFT ||
            (!isRTL && state.textAlign == TEXT_ALIGN_START) ||
            (isRTL && state.textAlign == TEXT_ALIGN_END)) {
    anchorX = 0;
  } else {
    anchorX = 1;
  }

  processor.mPt.x -= anchorX * totalWidth;

  // offset pt.y based on text baseline
  NS_ASSERTION(processor.mFontgrp->FontListLength()>0, "font group contains no fonts");
  const gfxFont::Metrics& fontMetrics = processor.mFontgrp->GetFontAt(0)->GetMetrics();

  gfxFloat anchorY;

  switch (state.textBaseline)
  {
  case TEXT_BASELINE_HANGING:
      // fall through; best we can do with the information available
  case TEXT_BASELINE_TOP:
    anchorY = fontMetrics.emAscent;
    break;
    break;
  case TEXT_BASELINE_MIDDLE:
    anchorY = (fontMetrics.emAscent - fontMetrics.emDescent) * .5f;
    break;
  case TEXT_BASELINE_IDEOGRAPHIC:
    // fall through; best we can do with the information available
  case TEXT_BASELINE_ALPHABETIC:
    anchorY = 0;
    break;
  case TEXT_BASELINE_BOTTOM:
    anchorY = -fontMetrics.emDescent;
    break;
  default:
    NS_ERROR("mTextBaseline holds invalid value");
    return NS_ERROR_FAILURE;
  }

  processor.mPt.y += anchorY;

  // correct bounding box to get it to be the correct size/position
  processor.mBoundingBox.width = totalWidth;
  processor.mBoundingBox.MoveBy(processor.mPt);

  processor.mPt.x *= processor.mAppUnitsPerDevPixel;
  processor.mPt.y *= processor.mAppUnitsPerDevPixel;

  Matrix oldTransform = mTarget->GetTransform();
  // if text is over aMaxWidth, then scale the text horizontally such that its
  // width is precisely aMaxWidth
  if (aMaxWidth > 0 && totalWidth > aMaxWidth) {
    Matrix newTransform = oldTransform;

    // Translate so that the anchor point is at 0,0, then scale and then
    // translate back.
    newTransform.Translate(aX, 0);
    newTransform.Scale(aMaxWidth / totalWidth, 1);
    newTransform.Translate(-aX, 0);
    /* we do this to avoid an ICE in the android compiler */
    Matrix androidCompilerBug = newTransform;
    mTarget->SetTransform(androidCompilerBug);
  }

  // save the previous bounding box
  gfxRect boundingBox = processor.mBoundingBox;

  // don't ever need to measure the bounding box twice
  processor.mDoMeasureBoundingBox = false;

  rv = nsBidiPresUtils::ProcessText(textToDraw.get(),
                                    textToDraw.Length(),
                                    isRTL ? NSBIDI_RTL : NSBIDI_LTR,
                                    presShell->GetPresContext(),
                                    processor,
                                    nsBidiPresUtils::MODE_DRAW,
                                    nsnull,
                                    0,
                                    nsnull,
                                    &bidiEngine);


  mTarget->SetTransform(oldTransform);

  if (aOp == nsCanvasRenderingContext2DAzure::TEXT_DRAW_OPERATION_FILL && !doDrawShadow)
    return RedrawUser(boundingBox);

  return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetMozTextStyle(const nsAString& textStyle)
{
    // font and mozTextStyle are the same value
    return SetFont(textStyle);
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetMozTextStyle(nsAString& textStyle)
{
    // font and mozTextStyle are the same value
    return GetFont(textStyle);
}

gfxFontGroup *nsCanvasRenderingContext2DAzure::GetCurrentFontStyle()
{
  // use lazy initilization for the font group since it's rather expensive
  if (!CurrentState().fontGroup) {
    nsresult rv = SetFont(kDefaultFontStyle);
    if (NS_FAILED(rv)) {
      gfxFontStyle style;
      style.size = kDefaultFontSize;
      CurrentState().fontGroup =
        gfxPlatform::GetPlatform()->CreateFontGroup(kDefaultFontName,
                                                    &style,
                                                    nsnull);
      if (CurrentState().fontGroup) {
        CurrentState().font = kDefaultFontStyle;
        rv = NS_OK;
      } else {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
            
    NS_ASSERTION(NS_SUCCEEDED(rv), "Default canvas font is invalid");
  }

  return CurrentState().fontGroup;
}

//
// line caps/joins
//
NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetLineWidth(float width)
{
  if (!FloatValidate(width) || width <= 0.0) {
    return NS_OK;
  }

  CurrentState().lineWidth = width;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetLineWidth(float *width)
{
  *width = CurrentState().lineWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetLineCap(const nsAString& capstyle)
{
  CapStyle cap;

  if (capstyle.EqualsLiteral("butt")) {
    cap = CAP_BUTT;
  } else if (capstyle.EqualsLiteral("round")) {
    cap = CAP_ROUND;
  } else if (capstyle.EqualsLiteral("square")) {
    cap = CAP_SQUARE;
  } else {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_OK;
  }

  CurrentState().lineCap = cap;

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetLineCap(nsAString& capstyle)
{
  switch (CurrentState().lineCap) {
  case CAP_BUTT:
    capstyle.AssignLiteral("butt");
    break;
  case CAP_ROUND:
    capstyle.AssignLiteral("round");
    break;
  case CAP_SQUARE:
    capstyle.AssignLiteral("square");
    break;
  default:
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetLineJoin(const nsAString& joinstyle)
{
  JoinStyle j;

  if (joinstyle.EqualsLiteral("round")) {
    j = JOIN_ROUND;
  } else if (joinstyle.EqualsLiteral("bevel")) {
    j = JOIN_BEVEL;
  } else if (joinstyle.EqualsLiteral("miter")) {
    j = JOIN_MITER_OR_BEVEL;
  } else {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_OK;
  }

  CurrentState().lineJoin = j;
   
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetLineJoin(nsAString& joinstyle)
{
  switch (CurrentState().lineJoin) {
  case JOIN_ROUND:
    joinstyle.AssignLiteral("round");
    break;
  case JOIN_BEVEL:
    joinstyle.AssignLiteral("bevel");
    break;
  case JOIN_MITER_OR_BEVEL:
    joinstyle.AssignLiteral("miter");
    break;
  default:
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetMiterLimit(float miter)
{
  if (!FloatValidate(miter) || miter <= 0.0)
    return NS_OK;

  CurrentState().miterLimit = miter;

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetMiterLimit(float *miter)
{
  *miter = CurrentState().miterLimit;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetMozDash(JSContext *cx, const jsval& patternArray)
{
  FallibleTArray<Float> dash;
  nsresult rv = JSValToDashArray(cx, patternArray, dash);
  if (NS_SUCCEEDED(rv)) {
    ContextState& state = CurrentState();
    state.dash = dash;
    if (state.dash.IsEmpty()) {
      state.dashOffset = 0;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetMozDash(JSContext* cx, jsval* dashArray)
{
  return DashArrayToJSVal(CurrentState().dash, cx, dashArray);
}
 
NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetMozDashOffset(float offset)
{
  if (!FloatValidate(offset)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  ContextState& state = CurrentState();
  if (!state.dash.IsEmpty()) {
    state.dashOffset = offset;
  }
  return NS_OK;
}
 
NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetMozDashOffset(float* offset)
{
  *offset = CurrentState().dashOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::IsPointInPath(float x, float y, bool *retVal)
{
  if (!FloatValidate(x,y)) {
    *retVal = false;
    return NS_OK;
  }

  EnsureUserSpacePath();

  *retVal = false;

  if (mPath) {
    *retVal = mPath->ContainsPoint(Point(x, y), mTarget->GetTransform());
  }

  return NS_OK;
}

//
// image
//

// drawImage(in HTMLImageElement image, in float dx, in float dy);
//   -- render image from 0,0 at dx,dy top-left coords
// drawImage(in HTMLImageElement image, in float dx, in float dy, in float sw, in float sh);
//   -- render image from 0,0 at dx,dy top-left coords clipping it to sw,sh
// drawImage(in HTMLImageElement image, in float sx, in float sy, in float sw, in float sh, in float dx, in float dy, in float dw, in float dh);
//   -- render the region defined by (sx,sy,sw,wh) in image-local space into the region (dx,dy,dw,dh) on the canvas

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::DrawImage(nsIDOMElement *imgElt, float a1,
                                           float a2, float a3, float a4, float a5,
                                           float a6, float a7, float a8,
                                           PRUint8 optional_argc)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(imgElt);
  if (!content) {
    return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
  }

  if (optional_argc == 0) {
    if (!FloatValidate(a1, a2)) {
      return NS_OK;
    }
  } else if (optional_argc == 2) {
    if (!FloatValidate(a1, a2, a3, a4)) {
      return NS_OK;
    }
  } else if (optional_argc == 6) {
    if (!FloatValidate(a1, a2, a3, a4, a5, a6) || !FloatValidate(a7, a8)) {
      return NS_OK;
    }
  }

  double sx,sy,sw,sh;
  double dx,dy,dw,dh;

  RefPtr<SourceSurface> srcSurf;
  gfxIntSize imgSize;

  nsHTMLCanvasElement* canvas = nsHTMLCanvasElement::FromContent(content);
  if (canvas) {
    nsIntSize size = canvas->GetSize();
    if (size.width == 0 || size.height == 0) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    // Special case for Canvas, which could be an Azure canvas!
    nsICanvasRenderingContextInternal *srcCanvas = canvas->GetContextAtIndex(0);
    if (srcCanvas == this) {
      // Self-copy.
      srcSurf = mTarget->Snapshot();
      imgSize = gfxIntSize(mWidth, mHeight);
    } else if (srcCanvas) {
      // This might not be an Azure canvas!
      srcSurf = srcCanvas->GetSurfaceSnapshot();

      if (srcSurf && mCanvasElement) {
        // Do security check here.
        CanvasUtils::DoDrawImageSecurityCheck(HTMLCanvasElement(),
                                              content->NodePrincipal(), canvas->IsWriteOnly(),
                                              false);
        imgSize = gfxIntSize(srcSurf->GetSize().width, srcSurf->GetSize().height);
      }
    }
  } else {
    gfxASurface* imgsurf =
      CanvasImageCache::Lookup(imgElt, HTMLCanvasElement(), &imgSize);
    if (imgsurf) {
      srcSurf = gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(mTarget, imgsurf);
    }
  }

  if (!srcSurf) {
    // The canvas spec says that drawImage should draw the first frame
    // of animated images
    PRUint32 sfeFlags = nsLayoutUtils::SFE_WANT_FIRST_FRAME;
    nsLayoutUtils::SurfaceFromElementResult res =
      nsLayoutUtils::SurfaceFromElement(content->AsElement(), sfeFlags);

    if (!res.mSurface) {
      // Spec says to silently do nothing if the element is still loading.
      return res.mIsStillLoading ? NS_OK : NS_ERROR_NOT_AVAILABLE;
    }

    // Ignore cairo surfaces that are bad! See bug 666312.
    if (res.mSurface->CairoStatus()) {
      return NS_OK;
    }

    imgSize = res.mSize;

    if (mCanvasElement) {
      CanvasUtils::DoDrawImageSecurityCheck(HTMLCanvasElement(),
                                            res.mPrincipal, res.mIsWriteOnly,
                                            res.mCORSUsed);
    }

    if (res.mImageRequest) {
      CanvasImageCache::NotifyDrawImage(imgElt, HTMLCanvasElement(),
                                        res.mImageRequest, res.mSurface, imgSize);
    }

    srcSurf = gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(mTarget, res.mSurface);
  }

  if (optional_argc == 0) {
    dx = a1;
    dy = a2;
    sx = sy = 0.0;
    dw = sw = (double) imgSize.width;
    dh = sh = (double) imgSize.height;
  } else if (optional_argc == 2) {
    dx = a1;
    dy = a2;
    dw = a3;
    dh = a4;
    sx = sy = 0.0;
    sw = (double) imgSize.width;
    sh = (double) imgSize.height;
  } else if (optional_argc == 6) {
    sx = a1;
    sy = a2;
    sw = a3;
    sh = a4;
    dx = a5;
    dy = a6;
    dw = a7;
    dh = a8;
  } else {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_ERROR_INVALID_ARG;
  }

  if (dw == 0.0 || dh == 0.0) {
    // not really failure, but nothing to do --
    // and noone likes a divide-by-zero
    return NS_OK;
  }

  if (sx < 0.0 || sy < 0.0 ||
      sw < 0.0 || sw > (double) imgSize.width ||
      sh < 0.0 || sh > (double) imgSize.height ||
      dw < 0.0 || dh < 0.0) {
    // XXX - Unresolved spec issues here, for now return error.
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  Filter filter;

  if (CurrentState().imageSmoothingEnabled)
    filter = mgfx::FILTER_LINEAR;
  else
    filter = mgfx::FILTER_POINT;

  mgfx::Rect bounds;
  
  if (NeedToDrawShadow()) {
    bounds = mgfx::Rect(dx, dy, dw, dh);
    bounds = mTarget->GetTransform().TransformBounds(bounds);
  }

  AdjustedTarget(this, bounds.IsEmpty() ? nsnull : &bounds)->
    DrawSurface(srcSurf,
                mgfx::Rect(dx, dy, dw, dh),
                mgfx::Rect(sx, sy, sw, sh),
                DrawSurfaceOptions(filter),
                DrawOptions(CurrentState().globalAlpha, UsedOperation()));

  return RedrawUser(gfxRect(dx, dy, dw, dh));
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetGlobalCompositeOperation(const nsAString& op)
{
  CompositionOp comp_op;

#define CANVAS_OP_TO_GFX_OP(cvsop, op2d) \
  if (op.EqualsLiteral(cvsop))   \
    comp_op = OP_##op2d;

  CANVAS_OP_TO_GFX_OP("copy", SOURCE)
  else CANVAS_OP_TO_GFX_OP("source-atop", ATOP)
  else CANVAS_OP_TO_GFX_OP("source-in", IN)
  else CANVAS_OP_TO_GFX_OP("source-out", OUT)
  else CANVAS_OP_TO_GFX_OP("source-over", OVER)
  else CANVAS_OP_TO_GFX_OP("destination-in", DEST_IN)
  else CANVAS_OP_TO_GFX_OP("destination-out", DEST_OUT)
  else CANVAS_OP_TO_GFX_OP("destination-over", DEST_OVER)
  else CANVAS_OP_TO_GFX_OP("destination-atop", DEST_ATOP)
  else CANVAS_OP_TO_GFX_OP("lighter", ADD)
  else CANVAS_OP_TO_GFX_OP("xor", XOR)
  // XXX ERRMSG we need to report an error to developers here! (bug 329026)
  else return NS_OK;

#undef CANVAS_OP_TO_GFX_OP
  CurrentState().op = comp_op;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetGlobalCompositeOperation(nsAString& op)
{
  CompositionOp comp_op = CurrentState().op;

#define CANVAS_OP_TO_GFX_OP(cvsop, op2d) \
  if (comp_op == OP_##op2d) \
    op.AssignLiteral(cvsop);

  CANVAS_OP_TO_GFX_OP("copy", SOURCE)
  else CANVAS_OP_TO_GFX_OP("destination-atop", DEST_ATOP)
  else CANVAS_OP_TO_GFX_OP("destination-in", DEST_IN)
  else CANVAS_OP_TO_GFX_OP("destination-out", DEST_OUT)
  else CANVAS_OP_TO_GFX_OP("destination-over", DEST_OVER)
  else CANVAS_OP_TO_GFX_OP("lighter", ADD)
  else CANVAS_OP_TO_GFX_OP("source-atop", ATOP)
  else CANVAS_OP_TO_GFX_OP("source-in", IN)
  else CANVAS_OP_TO_GFX_OP("source-out", OUT)
  else CANVAS_OP_TO_GFX_OP("source-over", OVER)
  else CANVAS_OP_TO_GFX_OP("xor", XOR)
  else return NS_ERROR_FAILURE;

#undef CANVAS_OP_TO_GFX_OP
    
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::DrawWindow(nsIDOMWindow* aWindow, float aX, float aY,
                                            float aW, float aH,
                                            const nsAString& aBGColor,
                                            PRUint32 flags)
{
  NS_ENSURE_ARG(aWindow != nsnull);

  // protect against too-large surfaces that will cause allocation
  // or overflow issues
  if (!gfxASurface::CheckSurfaceSize(gfxIntSize(PRInt32(aW), PRInt32(aH)),
                                      0xffff))
    return NS_ERROR_FAILURE;

  nsRefPtr<gfxASurface> drawSurf;
  GetThebesSurface(getter_AddRefs(drawSurf));

  nsRefPtr<gfxContext> thebes = new gfxContext(drawSurf);

  Matrix matrix = mTarget->GetTransform();
  thebes->SetMatrix(gfxMatrix(matrix._11, matrix._12, matrix._21,
                              matrix._22, matrix._31, matrix._32));

  // We can't allow web apps to call this until we fix at least the
  // following potential security issues:
  // -- rendering cross-domain IFRAMEs and then extracting the results
  // -- rendering the user's theme and then extracting the results
  // -- rendering native anonymous content (e.g., file input paths;
  // scrollbars should be allowed)
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    // not permitted to use DrawWindow
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Flush layout updates
  if (!(flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DO_NOT_FLUSH))
      nsContentUtils::FlushLayoutForTree(aWindow);

  nsRefPtr<nsPresContext> presContext;
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aWindow);
  if (win) {
    nsIDocShell* docshell = win->GetDocShell();
    if (docshell) {
      docshell->GetPresContext(getter_AddRefs(presContext));
    }
  }
  if (!presContext)
    return NS_ERROR_FAILURE;

  nscolor bgColor;

  nsIDocument* elementDoc = mCanvasElement ?
                            HTMLCanvasElement()->OwnerDoc() : nsnull;

  // Pass the CSS Loader object to the parser, to allow parser error reports
  // to include the outer window ID.
  nsCSSParser parser(elementDoc ? elementDoc->CSSLoader() : nsnull);
  nsresult rv = parser.ParseColorString(PromiseFlatString(aBGColor),
                                        nsnull, 0, &bgColor);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIPresShell* presShell = presContext->PresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsRect r(nsPresContext::CSSPixelsToAppUnits(aX),
           nsPresContext::CSSPixelsToAppUnits(aY),
           nsPresContext::CSSPixelsToAppUnits(aW),
           nsPresContext::CSSPixelsToAppUnits(aH));
  PRUint32 renderDocFlags = (nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                             nsIPresShell::RENDER_DOCUMENT_RELATIVE);
  if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_CARET) {
    renderDocFlags |= nsIPresShell::RENDER_CARET;
  }
  if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_VIEW) {
    renderDocFlags &= ~(nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                        nsIPresShell::RENDER_DOCUMENT_RELATIVE);
  }
  if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_USE_WIDGET_LAYERS) {
    renderDocFlags |= nsIPresShell::RENDER_USE_WIDGET_LAYERS;
  }
  if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_ASYNC_DECODE_IMAGES) {
    renderDocFlags |= nsIPresShell::RENDER_ASYNC_DECODE_IMAGES;
  }

  rv = presShell->RenderDocument(r, renderDocFlags, bgColor, thebes);

  // note that aX and aY are coordinates in the document that
  // we're drawing; aX and aY are drawn to 0,0 in current user
  // space.
  RedrawUser(gfxRect(0, 0, aW, aH));

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::AsyncDrawXULElement(nsIDOMXULElement* aElem,
                                                     float aX, float aY,
                                                     float aW, float aH,
                                                     const nsAString& aBGColor,
                                                     PRUint32 flags)
{
    NS_ENSURE_ARG(aElem != nsnull);

    // We can't allow web apps to call this until we fix at least the
    // following potential security issues:
    // -- rendering cross-domain IFRAMEs and then extracting the results
    // -- rendering the user's theme and then extracting the results
    // -- rendering native anonymous content (e.g., file input paths;
    // scrollbars should be allowed)
    if (!nsContentUtils::IsCallerTrustedForRead()) {
        // not permitted to use DrawWindow
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_DOM_SECURITY_ERR;
    }

#if 0
    nsCOMPtr<nsIFrameLoaderOwner> loaderOwner = do_QueryInterface(aElem);
    if (!loaderOwner)
        return NS_ERROR_FAILURE;

    nsRefPtr<nsFrameLoader> frameloader = loaderOwner->GetFrameLoader();
    if (!frameloader)
        return NS_ERROR_FAILURE;

    PBrowserParent *child = frameloader->GetRemoteBrowser();
    if (!child) {
        nsCOMPtr<nsIDOMWindow> window =
            do_GetInterface(frameloader->GetExistingDocShell());
        if (!window)
            return NS_ERROR_FAILURE;

        return DrawWindow(window, aX, aY, aW, aH, aBGColor, flags);
    }

    // protect against too-large surfaces that will cause allocation
    // or overflow issues
    if (!gfxASurface::CheckSurfaceSize(gfxIntSize(aW, aH), 0xffff))
        return NS_ERROR_FAILURE;

    bool flush =
        (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DO_NOT_FLUSH) == 0;

    PRUint32 renderDocFlags = nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING;
    if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_CARET) {
        renderDocFlags |= nsIPresShell::RENDER_CARET;
    }
    if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_VIEW) {
        renderDocFlags &= ~nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING;
    }

    nsRect rect(nsPresContext::CSSPixelsToAppUnits(aX),
                nsPresContext::CSSPixelsToAppUnits(aY),
                nsPresContext::CSSPixelsToAppUnits(aW),
                nsPresContext::CSSPixelsToAppUnits(aH));
    if (mIPC) {
        PDocumentRendererParent *pdocrender =
            child->SendPDocumentRendererConstructor(rect,
                                                    mThebes->CurrentMatrix(),
                                                    nsString(aBGColor),
                                                    renderDocFlags, flush,
                                                    nsIntSize(mWidth, mHeight));
        if (!pdocrender)
            return NS_ERROR_FAILURE;

        DocumentRendererParent *docrender =
            static_cast<DocumentRendererParent *>(pdocrender);

        docrender->SetCanvasContext(this, mThebes);
    }
#endif
    return NS_OK;
}

//
// device pixel getting/setting
//

void
nsCanvasRenderingContext2DAzure::EnsureUnpremultiplyTable() {
  if (sUnpremultiplyTable)
    return;

  // Infallably alloc the unpremultiply table.
  sUnpremultiplyTable = new PRUint8[256][256];

  // It's important that the array be indexed first by alpha and then by rgb
  // value.  When we unpremultiply a pixel, we're guaranteed to do three
  // lookups with the same alpha; indexing by alpha first makes it likely that
  // those three lookups will be close to one another in memory, thus
  // increasing the chance of a cache hit.

  // a == 0 case
  for (PRUint32 c = 0; c <= 255; c++) {
    sUnpremultiplyTable[0][c] = c;
  }

  for (int a = 1; a <= 255; a++) {
    for (int c = 0; c <= 255; c++) {
      sUnpremultiplyTable[a][c] = (PRUint8)((c * 255) / a);
    }
  }
}


NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetImageData()
{
  /* Should never be called -- GetImageData_explicit is the QS entry point */
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetImageData_explicit(PRInt32 x, PRInt32 y, PRUint32 w, PRUint32 h,
                                                       PRUint8 *aData, PRUint32 aDataLen)
{
  if (!mValid)
    return NS_ERROR_FAILURE;

  if (!mCanvasElement && !mDocShell) {
    NS_ERROR("No canvas element and no docshell in GetImageData!!!");
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Check only if we have a canvas element; if we were created with a docshell,
  // then it's special internal use.
  if (mCanvasElement &&
      HTMLCanvasElement()->IsWriteOnly() &&
      !nsContentUtils::IsCallerTrustedForRead())
  {
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (w == 0 || h == 0 || aDataLen != w * h * 4)
    return NS_ERROR_DOM_SYNTAX_ERR;

  CheckedInt32 rightMost = CheckedInt32(x) + w;
  CheckedInt32 bottomMost = CheckedInt32(y) + h;

  if (!rightMost.valid() || !bottomMost.valid())
    return NS_ERROR_DOM_SYNTAX_ERR;

  if (mZero) {
    return NS_OK;
  }

  IntRect srcRect(0, 0, mWidth, mHeight);
  IntRect destRect(x, y, w, h);

  if (!srcRect.Contains(destRect)) {
    // Some data is outside the canvas surface, clear the destination.
    memset(aData, 0, aDataLen);
  }

  IntRect srcReadRect = srcRect.Intersect(destRect);
  IntRect dstWriteRect = srcReadRect;
  dstWriteRect.MoveBy(-x, -y);

  PRUint8 *src = aData;
  PRUint32 srcStride = w * 4;
  
  RefPtr<DataSourceSurface> readback;
  if (!srcReadRect.IsEmpty()) {
    RefPtr<SourceSurface> snapshot = mTarget->Snapshot();

    readback = snapshot->GetDataSurface();

    srcStride = readback->Stride();
    src = readback->GetData() + srcReadRect.y * srcStride + srcReadRect.x * 4;
  }

  // make sure sUnpremultiplyTable has been created
  EnsureUnpremultiplyTable();

  // NOTE! dst is the same as src, and this relies on reading
  // from src and advancing that ptr before writing to dst.
  PRUint8 *dst = aData + dstWriteRect.y * (w * 4) + dstWriteRect.x * 4;

  for (int j = 0; j < dstWriteRect.height; j++) {
    for (int i = 0; i < dstWriteRect.width; i++) {
      // XXX Is there some useful swizzle MMX we can use here?
#ifdef IS_LITTLE_ENDIAN
      PRUint8 b = *src++;
      PRUint8 g = *src++;
      PRUint8 r = *src++;
      PRUint8 a = *src++;
#else
      PRUint8 a = *src++;
      PRUint8 r = *src++;
      PRUint8 g = *src++;
      PRUint8 b = *src++;
#endif
      // Convert to non-premultiplied color
      *dst++ = sUnpremultiplyTable[a][r];
      *dst++ = sUnpremultiplyTable[a][g];
      *dst++ = sUnpremultiplyTable[a][b];
      *dst++ = a;
    }
    src += srcStride - (dstWriteRect.width * 4);
    dst += (w * 4) - (dstWriteRect.width * 4);
  }
  return NS_OK;
}

void
nsCanvasRenderingContext2DAzure::EnsurePremultiplyTable() {
  if (sPremultiplyTable)
    return;

  // Infallably alloc the premultiply table.
  sPremultiplyTable = new PRUint8[256][256];

  // Like the unpremultiply table, it's important that we index the premultiply
  // table with the alpha value as the first index to ensure good cache
  // performance.

  for (int a = 0; a <= 255; a++) {
    for (int c = 0; c <= 255; c++) {
      sPremultiplyTable[a][c] = (a * c + 254) / 255;
    }
  }
}

void
nsCanvasRenderingContext2DAzure::FillRuleChanged()
{
  if (mPath) {
    mPathBuilder = mPath->CopyToBuilder(CurrentState().fillRule);
    mPath = nsnull;
  }
}

// void putImageData (in ImageData d, in float x, in float y);
NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::PutImageData()
{
  /* Should never be called -- PutImageData_explicit is the QS entry point */
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::PutImageData_explicit(PRInt32 x, PRInt32 y, PRUint32 w, PRUint32 h,
                                                       unsigned char *aData, PRUint32 aDataLen,
                                                       bool hasDirtyRect, PRInt32 dirtyX, PRInt32 dirtyY,
                                                       PRInt32 dirtyWidth, PRInt32 dirtyHeight)
{
  if (!mValid) {
    return NS_ERROR_FAILURE;
  }

  if (w == 0 || h == 0) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  IntRect dirtyRect;
  IntRect imageDataRect(0, 0, w, h);

  if (hasDirtyRect) {
    // fix up negative dimensions
    if (dirtyWidth < 0) {
      NS_ENSURE_TRUE(dirtyWidth != INT_MIN, NS_ERROR_DOM_INDEX_SIZE_ERR);

      CheckedInt32 checkedDirtyX = CheckedInt32(dirtyX) + dirtyWidth;

      if (!checkedDirtyX.valid())
          return NS_ERROR_DOM_INDEX_SIZE_ERR;

      dirtyX = checkedDirtyX.value();
      dirtyWidth = -(int32)dirtyWidth;
    }

    if (dirtyHeight < 0) {
      NS_ENSURE_TRUE(dirtyHeight != INT_MIN, NS_ERROR_DOM_INDEX_SIZE_ERR);

      CheckedInt32 checkedDirtyY = CheckedInt32(dirtyY) + dirtyHeight;

      if (!checkedDirtyY.valid())
          return NS_ERROR_DOM_INDEX_SIZE_ERR;

      dirtyY = checkedDirtyY.value();
      dirtyHeight = -(int32)dirtyHeight;
    }

    // bound the dirty rect within the imageData rectangle
    dirtyRect = imageDataRect.Intersect(IntRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight));

    if (dirtyRect.Width() <= 0 || dirtyRect.Height() <= 0)
        return NS_OK;
  } else {
    dirtyRect = imageDataRect;
  }

  dirtyRect.MoveBy(IntPoint(x, y));
  dirtyRect = IntRect(0, 0, mWidth, mHeight).Intersect(dirtyRect);

  if (dirtyRect.Width() <= 0 || dirtyRect.Height() <= 0) {
    return NS_OK;
  }

  PRUint32 len = w * h * 4;
  if (aDataLen != len) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  nsRefPtr<gfxImageSurface> imgsurf = new gfxImageSurface(gfxIntSize(w, h),
                                                          gfxASurface::ImageFormatARGB32);
  if (!imgsurf || imgsurf->CairoStatus()) {
    return NS_ERROR_FAILURE;
  }

  // ensure premultiply table has been created
  EnsurePremultiplyTable();

  PRUint8 *src = aData;
  PRUint8 *dst = imgsurf->Data();

  for (PRUint32 j = 0; j < h; j++) {
    for (PRUint32 i = 0; i < w; i++) {
      PRUint8 r = *src++;
      PRUint8 g = *src++;
      PRUint8 b = *src++;
      PRUint8 a = *src++;

      // Convert to premultiplied color (losslessly if the input came from getImageData)
#ifdef IS_LITTLE_ENDIAN
      *dst++ = sPremultiplyTable[a][b];
      *dst++ = sPremultiplyTable[a][g];
      *dst++ = sPremultiplyTable[a][r];
      *dst++ = a;
#else
      *dst++ = a;
      *dst++ = sPremultiplyTable[a][r];
      *dst++ = sPremultiplyTable[a][g];
      *dst++ = sPremultiplyTable[a][b];
#endif
    }
  }

  RefPtr<SourceSurface> sourceSurface =
    mTarget->CreateSourceSurfaceFromData(imgsurf->Data(), IntSize(w, h), imgsurf->Stride(), FORMAT_B8G8R8A8);


  mTarget->CopySurface(sourceSurface,
                       IntRect(dirtyRect.x - x, dirtyRect.y - y,
                               dirtyRect.width, dirtyRect.height),
                       IntPoint(dirtyRect.x, dirtyRect.y));

  Redraw(mgfx::Rect(dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height));

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetThebesSurface(gfxASurface **surface)
{
  if (!mTarget) {
    nsRefPtr<gfxASurface> tmpSurf =
      gfxPlatform::GetPlatform()->CreateOffscreenSurface(gfxIntSize(1, 1), gfxASurface::CONTENT_COLOR_ALPHA);
    *surface = tmpSurf.forget().get();
    return NS_OK;
  }

  if (!mThebesSurface) {
    mThebesSurface =
      gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(mTarget);    

    if (!mThebesSurface) {
      return NS_ERROR_FAILURE;
    }
  } else {
    // Normally GetThebesSurfaceForDrawTarget will handle the flush, when
    // we're returning a cached ThebesSurface we need to flush here.
    mTarget->Flush();
  }

  *surface = mThebesSurface;
  NS_ADDREF(*surface);

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::CreateImageData()
{
  /* Should never be called; handled entirely in the quickstub */
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::GetMozImageSmoothingEnabled(bool *retVal)
{
  *retVal = CurrentState().imageSmoothingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2DAzure::SetMozImageSmoothingEnabled(bool val)
{
  if (val != CurrentState().imageSmoothingEnabled) {
      CurrentState().imageSmoothingEnabled = val;
  }

  return NS_OK;
}

static PRUint8 g2DContextLayerUserData;

class CanvasRenderingContext2DUserData : public LayerUserData {
public:
  CanvasRenderingContext2DUserData(nsHTMLCanvasElement *aContent)
    : mContent(aContent) {}
  static void DidTransactionCallback(void* aData)
  {
    static_cast<CanvasRenderingContext2DUserData*>(aData)->mContent->MarkContextClean();
  }

private:
  nsRefPtr<nsHTMLCanvasElement> mContent;
};

already_AddRefed<CanvasLayer>
nsCanvasRenderingContext2DAzure::GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                           CanvasLayer *aOldLayer,
                                           LayerManager *aManager)
{
  if (!mValid) {
    return nsnull;
  }

  if (mTarget) {
    mTarget->Flush();
  }

  if (!mResetLayer && aOldLayer &&
      aOldLayer->HasUserData(&g2DContextLayerUserData)) {
      NS_ADDREF(aOldLayer);
      return aOldLayer;
  }

  nsRefPtr<CanvasLayer> canvasLayer = aManager->CreateCanvasLayer();
  if (!canvasLayer) {
      NS_WARNING("CreateCanvasLayer returned null!");
      return nsnull;
  }
  CanvasRenderingContext2DUserData *userData = nsnull;
  if (aBuilder->IsPaintingToWindow()) {
    // Make the layer tell us whenever a transaction finishes (including
    // the current transaction), so we can clear our invalidation state and
    // start invalidating again. We need to do this for the layer that is
    // being painted to a window (there shouldn't be more than one at a time,
    // and if there is, flushing the invalidation state more often than
    // necessary is harmless).

    // The layer will be destroyed when we tear down the presentation
    // (at the latest), at which time this userData will be destroyed,
    // releasing the reference to the element.
    // The userData will receive DidTransactionCallbacks, which flush the
    // the invalidation state to indicate that the canvas is up to date.
    userData = new CanvasRenderingContext2DUserData(HTMLCanvasElement());
    canvasLayer->SetDidTransactionCallback(
            CanvasRenderingContext2DUserData::DidTransactionCallback, userData);
  }
  canvasLayer->SetUserData(&g2DContextLayerUserData, userData);

  CanvasLayer::Data data;

  data.mDrawTarget = mTarget;
  data.mSize = nsIntSize(mWidth, mHeight);

  canvasLayer->Initialize(data);
  PRUint32 flags = mOpaque ? Layer::CONTENT_OPAQUE : 0;
  canvasLayer->SetContentFlags(flags);
  canvasLayer->Updated();

  mResetLayer = false;

  return canvasLayer.forget();
}

void
nsCanvasRenderingContext2DAzure::MarkContextClean()
{
  if (mInvalidateCount > 0) {
    mPredictManyRedrawCalls = mInvalidateCount > kCanvasMaxInvalidateCount;
  }
  mIsEntireFrameInvalid = false;
  mInvalidateCount = 0;
}

