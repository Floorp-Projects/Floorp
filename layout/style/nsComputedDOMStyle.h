/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object returned from element.getComputedStyle() */

#ifndef nsComputedDOMStyle_h__
#define nsComputedDOMStyle_h__

#include "nsAutoPtr.h"
#include "mozilla/ArenaRefPtr.h"
#include "mozilla/ArenaRefPtrInlines.h"
#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsCSSProps.h"
#include "nsDOMCSSDeclaration.h"
#include "nsStyleContext.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/gfx/Types.h"
#include "nsCoord.h"
#include "nsColor.h"
#include "nsIContent.h"

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

struct nsComputedStyleMap;
class nsIFrame;
class nsIPresShell;
class nsDOMCSSValueList;
struct nsMargin;
class nsROCSSPrimitiveValue;
struct nsStyleBackground;
class nsStyleCoord;
class nsStyleCorners;
struct nsStyleFilter;
class nsStyleGradient;
struct nsStyleImage;
class nsStyleSides;
struct nsTimingFunction;

class nsComputedDOMStyle final : public nsDOMCSSDeclaration
                               , public nsStubMutationObserver
{
private:
  // Convenience typedefs:
  typedef nsCSSProps::KTableEntry KTableEntry;
  typedef mozilla::dom::CSSValue CSSValue;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsComputedDOMStyle,
                                                                   nsICSSDeclaration)

  NS_DECL_NSICSSDECLARATION

  NS_DECL_NSIDOMCSSSTYLEDECLARATION_HELPER
  virtual already_AddRefed<CSSValue>
  GetPropertyCSSValue(const nsAString& aProp, mozilla::ErrorResult& aRv)
    override;
  using nsICSSDeclaration::GetPropertyCSSValue;
  virtual void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName) override;

  enum StyleType {
    eDefaultOnly, // Only includes UA and user sheets
    eAll // Includes all stylesheets
  };

  nsComputedDOMStyle(mozilla::dom::Element* aElement,
                     const nsAString& aPseudoElt,
                     nsIPresShell* aPresShell,
                     StyleType aStyleType);

  virtual nsINode *GetParentObject() override
  {
    return mContent;
  }

  static already_AddRefed<nsStyleContext>
  GetStyleContextForElement(mozilla::dom::Element* aElement, nsIAtom* aPseudo,
                            nsIPresShell* aPresShell,
                            StyleType aStyleType = eAll);

  static already_AddRefed<nsStyleContext>
  GetStyleContextForElementNoFlush(mozilla::dom::Element* aElement,
                                   nsIAtom* aPseudo,
                                   nsIPresShell* aPresShell,
                                   StyleType aStyleType = eAll);

  static nsIPresShell*
  GetPresShellForContent(nsIContent* aContent);

  // Helper for nsDOMWindowUtils::GetVisitedDependentComputedStyle
  void SetExposeVisitedStyle(bool aExpose) {
    NS_ASSERTION(aExpose != mExposeVisitedStyle, "should always be changing");
    mExposeVisitedStyle = aExpose;
  }

  // nsDOMCSSDeclaration abstract methods which should never be called
  // on a nsComputedDOMStyle object, but must be defined to avoid
  // compile errors.
  virtual mozilla::css::Declaration* GetCSSDeclaration(Operation) override;
  virtual nsresult SetCSSDeclaration(mozilla::css::Declaration*) override;
  virtual nsIDocument* DocToUpdate() override;
  virtual void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv) override;

  static nsROCSSPrimitiveValue* MatrixToCSSValue(const mozilla::gfx::Matrix4x4& aMatrix);

  static void RegisterPrefChangeCallbacks();
  static void UnregisterPrefChangeCallbacks();

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED

private:
  virtual ~nsComputedDOMStyle();

  void AssertFlushedPendingReflows() {
    NS_ASSERTION(mFlushedPendingReflows,
                 "property getter should have been marked layout-dependent");
  }

  nsMargin GetAdjustedValuesForBoxSizing();

  // Helper method for DoGetTextAlign[Last].
  CSSValue* CreateTextAlignValue(uint8_t aAlign,
                                               bool aAlignTrue,
                                               const KTableEntry aTable[]);
  // This indicates error by leaving mStyleContext null.
  void UpdateCurrentStyleSources(bool aNeedsLayoutFlush);
  void ClearCurrentStyleSources();

  // Helper functions called by UpdateCurrentStyleSources.
  void ClearStyleContext();
  void SetResolvedStyleContext(RefPtr<nsStyleContext>&& aContext);
  void SetFrameStyleContext(nsStyleContext* aContext);

#define STYLE_STRUCT(name_, checkdata_cb_)                              \
  const nsStyle##name_ * Style##name_() {                               \
    return mStyleContext->Style##name_();                               \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  // All of the property getters below return a pointer to a refcounted object
  // that has just been created, but the refcount is still 0. Caller must take
  // ownership.

  CSSValue* GetEllipseRadii(const nsStyleCorners& aRadius,
                                          uint8_t aFullCorner,
                                          bool aIsBorder); // else outline

  CSSValue* GetOffsetWidthFor(mozilla::css::Side aSide);

  CSSValue* GetAbsoluteOffset(mozilla::css::Side aSide);

  CSSValue* GetRelativeOffset(mozilla::css::Side aSide);

  CSSValue* GetStickyOffset(mozilla::css::Side aSide);

  CSSValue* GetStaticOffset(mozilla::css::Side aSide);

  CSSValue* GetPaddingWidthFor(mozilla::css::Side aSide);

  CSSValue* GetBorderColorsFor(mozilla::css::Side aSide);

  CSSValue* GetBorderStyleFor(mozilla::css::Side aSide);

  CSSValue* GetBorderWidthFor(mozilla::css::Side aSide);

  CSSValue* GetBorderColorFor(mozilla::css::Side aSide);

  CSSValue* GetMarginWidthFor(mozilla::css::Side aSide);

  CSSValue* GetSVGPaintFor(bool aFill);

  // Appends all aLineNames (must be non-empty) space-separated to aResult.
  void AppendGridLineNames(nsString& aResult,
                           const nsTArray<nsString>& aLineNames);
  // Appends aLineNames (if non-empty) as a CSSValue* to aValueList.
  void AppendGridLineNames(nsDOMCSSValueList* aValueList,
                           const nsTArray<nsString>& aLineNames);
  // Appends aLineNames1/2 (if non-empty) as a CSSValue* to aValueList.
  void AppendGridLineNames(nsDOMCSSValueList* aValueList,
                           const nsTArray<nsString>& aLineNames1,
                           const nsTArray<nsString>& aLineNames2);
  CSSValue* GetGridTrackSize(const nsStyleCoord& aMinSize,
                                           const nsStyleCoord& aMaxSize);
  CSSValue* GetGridTemplateColumnsRows(const nsStyleGridTemplate& aTrackList,
                                                     const nsTArray<nscoord>* aTrackSizes);
  CSSValue* GetGridLine(const nsStyleGridLine& aGridLine);

  bool GetLineHeightCoord(nscoord& aCoord);

  CSSValue* GetCSSShadowArray(nsCSSShadowArray* aArray,
                                            const nscolor& aDefaultColor,
                                            bool aIsBoxShadow);

  CSSValue* GetBackgroundList(uint8_t nsStyleBackground::Layer::* aMember,
                                            uint32_t nsStyleBackground::* aCount,
                                            const KTableEntry aTable[]);

  void GetCSSGradientString(const nsStyleGradient* aGradient,
                            nsAString& aString);
  void GetImageRectString(nsIURI* aURI,
                          const nsStyleSides& aCropRect,
                          nsString& aString);
  CSSValue* GetScrollSnapPoints(const nsStyleCoord& aCoord);
  void AppendTimingFunction(nsDOMCSSValueList *aValueList,
                            const nsTimingFunction& aTimingFunction);

  /* Properties queryable as CSSValues.
   * To avoid a name conflict with nsIDOM*CSS2Properties, these are all
   * DoGetXXX instead of GetXXX.
   */

  CSSValue* DoGetAppearance();

  /* Box properties */
  CSSValue* DoGetBoxAlign();
  CSSValue* DoGetBoxDecorationBreak();
  CSSValue* DoGetBoxDirection();
  CSSValue* DoGetBoxFlex();
  CSSValue* DoGetBoxOrdinalGroup();
  CSSValue* DoGetBoxOrient();
  CSSValue* DoGetBoxPack();
  CSSValue* DoGetBoxSizing();

  CSSValue* DoGetWidth();
  CSSValue* DoGetHeight();
  CSSValue* DoGetMaxHeight();
  CSSValue* DoGetMaxWidth();
  CSSValue* DoGetMinHeight();
  CSSValue* DoGetMinWidth();
  CSSValue* DoGetMixBlendMode();
  CSSValue* DoGetIsolation();
  CSSValue* DoGetObjectFit();
  CSSValue* DoGetObjectPosition();
  CSSValue* DoGetLeft();
  CSSValue* DoGetTop();
  CSSValue* DoGetRight();
  CSSValue* DoGetBottom();
  CSSValue* DoGetStackSizing();

  /* Font properties */
  CSSValue* DoGetColor();
  CSSValue* DoGetFontFamily();
  CSSValue* DoGetFontFeatureSettings();
  CSSValue* DoGetFontKerning();
  CSSValue* DoGetFontLanguageOverride();
  CSSValue* DoGetFontSize();
  CSSValue* DoGetFontSizeAdjust();
  CSSValue* DoGetOsxFontSmoothing();
  CSSValue* DoGetFontStretch();
  CSSValue* DoGetFontStyle();
  CSSValue* DoGetFontSynthesis();
  CSSValue* DoGetFontVariant();
  CSSValue* DoGetFontVariantAlternates();
  CSSValue* DoGetFontVariantCaps();
  CSSValue* DoGetFontVariantEastAsian();
  CSSValue* DoGetFontVariantLigatures();
  CSSValue* DoGetFontVariantNumeric();
  CSSValue* DoGetFontVariantPosition();
  CSSValue* DoGetFontWeight();

  /* Grid properties */
  CSSValue* DoGetGridAutoFlow();
  CSSValue* DoGetGridAutoColumns();
  CSSValue* DoGetGridAutoRows();
  CSSValue* DoGetGridTemplateAreas();
  CSSValue* DoGetGridTemplateColumns();
  CSSValue* DoGetGridTemplateRows();
  CSSValue* DoGetGridColumnStart();
  CSSValue* DoGetGridColumnEnd();
  CSSValue* DoGetGridRowStart();
  CSSValue* DoGetGridRowEnd();
  CSSValue* DoGetGridColumnGap();
  CSSValue* DoGetGridRowGap();

  /* Background properties */
  CSSValue* DoGetBackgroundAttachment();
  CSSValue* DoGetBackgroundColor();
  CSSValue* DoGetBackgroundImage();
  CSSValue* DoGetBackgroundPosition();
  CSSValue* DoGetBackgroundRepeat();
  CSSValue* DoGetBackgroundClip();
  CSSValue* DoGetBackgroundBlendMode();
  CSSValue* DoGetBackgroundOrigin();
  CSSValue* DoGetBackgroundSize();

  /* Padding properties */
  CSSValue* DoGetPaddingTop();
  CSSValue* DoGetPaddingBottom();
  CSSValue* DoGetPaddingLeft();
  CSSValue* DoGetPaddingRight();

  /* Table Properties */
  CSSValue* DoGetBorderCollapse();
  CSSValue* DoGetBorderSpacing();
  CSSValue* DoGetCaptionSide();
  CSSValue* DoGetEmptyCells();
  CSSValue* DoGetTableLayout();
  CSSValue* DoGetVerticalAlign();

  /* Border Properties */
  CSSValue* DoGetBorderTopStyle();
  CSSValue* DoGetBorderBottomStyle();
  CSSValue* DoGetBorderLeftStyle();
  CSSValue* DoGetBorderRightStyle();
  CSSValue* DoGetBorderTopWidth();
  CSSValue* DoGetBorderBottomWidth();
  CSSValue* DoGetBorderLeftWidth();
  CSSValue* DoGetBorderRightWidth();
  CSSValue* DoGetBorderTopColor();
  CSSValue* DoGetBorderBottomColor();
  CSSValue* DoGetBorderLeftColor();
  CSSValue* DoGetBorderRightColor();
  CSSValue* DoGetBorderBottomColors();
  CSSValue* DoGetBorderLeftColors();
  CSSValue* DoGetBorderRightColors();
  CSSValue* DoGetBorderTopColors();
  CSSValue* DoGetBorderBottomLeftRadius();
  CSSValue* DoGetBorderBottomRightRadius();
  CSSValue* DoGetBorderTopLeftRadius();
  CSSValue* DoGetBorderTopRightRadius();
  CSSValue* DoGetFloatEdge();

  /* Border Image */
  CSSValue* DoGetBorderImageSource();
  CSSValue* DoGetBorderImageSlice();
  CSSValue* DoGetBorderImageWidth();
  CSSValue* DoGetBorderImageOutset();
  CSSValue* DoGetBorderImageRepeat();

  /* Box Shadow */
  CSSValue* DoGetBoxShadow();

  /* Window Shadow */
  CSSValue* DoGetWindowShadow();

  /* Margin Properties */
  CSSValue* DoGetMarginTopWidth();
  CSSValue* DoGetMarginBottomWidth();
  CSSValue* DoGetMarginLeftWidth();
  CSSValue* DoGetMarginRightWidth();

  /* Outline Properties */
  CSSValue* DoGetOutlineWidth();
  CSSValue* DoGetOutlineStyle();
  CSSValue* DoGetOutlineColor();
  CSSValue* DoGetOutlineOffset();
  CSSValue* DoGetOutlineRadiusBottomLeft();
  CSSValue* DoGetOutlineRadiusBottomRight();
  CSSValue* DoGetOutlineRadiusTopLeft();
  CSSValue* DoGetOutlineRadiusTopRight();

  /* Content Properties */
  CSSValue* DoGetContent();
  CSSValue* DoGetCounterIncrement();
  CSSValue* DoGetCounterReset();
  CSSValue* DoGetMarkerOffset();

  /* Quotes Properties */
  CSSValue* DoGetQuotes();

  /* z-index */
  CSSValue* DoGetZIndex();

  /* List properties */
  CSSValue* DoGetListStyleImage();
  CSSValue* DoGetListStylePosition();
  CSSValue* DoGetListStyleType();
  CSSValue* DoGetImageRegion();

  /* Text Properties */
  CSSValue* DoGetLineHeight();
  CSSValue* DoGetRubyAlign();
  CSSValue* DoGetRubyPosition();
  CSSValue* DoGetTextAlign();
  CSSValue* DoGetTextAlignLast();
  CSSValue* DoGetTextCombineUpright();
  CSSValue* DoGetTextDecoration();
  CSSValue* DoGetTextDecorationColor();
  CSSValue* DoGetTextDecorationLine();
  CSSValue* DoGetTextDecorationStyle();
  CSSValue* DoGetTextEmphasisColor();
  CSSValue* DoGetTextEmphasisPosition();
  CSSValue* DoGetTextEmphasisStyle();
  CSSValue* DoGetTextIndent();
  CSSValue* DoGetTextOrientation();
  CSSValue* DoGetTextOverflow();
  CSSValue* DoGetTextTransform();
  CSSValue* DoGetTextShadow();
  CSSValue* DoGetLetterSpacing();
  CSSValue* DoGetWordSpacing();
  CSSValue* DoGetWhiteSpace();
  CSSValue* DoGetWordBreak();
  CSSValue* DoGetWordWrap();
  CSSValue* DoGetHyphens();
  CSSValue* DoGetTabSize();
  CSSValue* DoGetTextSizeAdjust();

  /* Visibility properties */
  CSSValue* DoGetOpacity();
  CSSValue* DoGetPointerEvents();
  CSSValue* DoGetVisibility();
  CSSValue* DoGetWritingMode();

  /* Direction properties */
  CSSValue* DoGetDirection();
  CSSValue* DoGetUnicodeBidi();

  /* Display properties */
  CSSValue* DoGetBinding();
  CSSValue* DoGetClear();
  CSSValue* DoGetFloat();
  CSSValue* DoGetDisplay();
  CSSValue* DoGetContain();
  CSSValue* DoGetPosition();
  CSSValue* DoGetClip();
  CSSValue* DoGetImageOrientation();
  CSSValue* DoGetWillChange();
  CSSValue* DoGetOverflow();
  CSSValue* DoGetOverflowX();
  CSSValue* DoGetOverflowY();
  CSSValue* DoGetOverflowClipBox();
  CSSValue* DoGetResize();
  CSSValue* DoGetPageBreakAfter();
  CSSValue* DoGetPageBreakBefore();
  CSSValue* DoGetPageBreakInside();
  CSSValue* DoGetTouchAction();
  CSSValue* DoGetTransform();
  CSSValue* DoGetTransformBox();
  CSSValue* DoGetTransformOrigin();
  CSSValue* DoGetPerspective();
  CSSValue* DoGetBackfaceVisibility();
  CSSValue* DoGetPerspectiveOrigin();
  CSSValue* DoGetTransformStyle();
  CSSValue* DoGetOrient();
  CSSValue* DoGetScrollBehavior();
  CSSValue* DoGetScrollSnapType();
  CSSValue* DoGetScrollSnapTypeX();
  CSSValue* DoGetScrollSnapTypeY();
  CSSValue* DoGetScrollSnapPointsX();
  CSSValue* DoGetScrollSnapPointsY();
  CSSValue* DoGetScrollSnapDestination();
  CSSValue* DoGetScrollSnapCoordinate();

  /* User interface properties */
  CSSValue* DoGetCursor();
  CSSValue* DoGetForceBrokenImageIcon();
  CSSValue* DoGetIMEMode();
  CSSValue* DoGetUserFocus();
  CSSValue* DoGetUserInput();
  CSSValue* DoGetUserModify();
  CSSValue* DoGetUserSelect();
  CSSValue* DoGetWindowDragging();

  /* Column properties */
  CSSValue* DoGetColumnCount();
  CSSValue* DoGetColumnFill();
  CSSValue* DoGetColumnWidth();
  CSSValue* DoGetColumnGap();
  CSSValue* DoGetColumnRuleWidth();
  CSSValue* DoGetColumnRuleStyle();
  CSSValue* DoGetColumnRuleColor();

  /* CSS Transitions */
  CSSValue* DoGetTransitionProperty();
  CSSValue* DoGetTransitionDuration();
  CSSValue* DoGetTransitionDelay();
  CSSValue* DoGetTransitionTimingFunction();

  /* CSS Animations */
  CSSValue* DoGetAnimationName();
  CSSValue* DoGetAnimationDuration();
  CSSValue* DoGetAnimationDelay();
  CSSValue* DoGetAnimationTimingFunction();
  CSSValue* DoGetAnimationDirection();
  CSSValue* DoGetAnimationFillMode();
  CSSValue* DoGetAnimationIterationCount();
  CSSValue* DoGetAnimationPlayState();

  /* CSS Flexbox properties */
  CSSValue* DoGetFlexBasis();
  CSSValue* DoGetFlexDirection();
  CSSValue* DoGetFlexGrow();
  CSSValue* DoGetFlexShrink();
  CSSValue* DoGetFlexWrap();

  /* CSS Flexbox/Grid properties */
  CSSValue* DoGetOrder();

  /* CSS Box Alignment properties */
  CSSValue* DoGetAlignContent();
  CSSValue* DoGetAlignItems();
  CSSValue* DoGetAlignSelf();
  CSSValue* DoGetJustifyContent();
  CSSValue* DoGetJustifyItems();
  CSSValue* DoGetJustifySelf();

  /* SVG properties */
  CSSValue* DoGetFill();
  CSSValue* DoGetStroke();
  CSSValue* DoGetMarkerEnd();
  CSSValue* DoGetMarkerMid();
  CSSValue* DoGetMarkerStart();
  CSSValue* DoGetStrokeDasharray();

  CSSValue* DoGetStrokeDashoffset();
  CSSValue* DoGetStrokeWidth();
  CSSValue* DoGetVectorEffect();

  CSSValue* DoGetFillOpacity();
  CSSValue* DoGetFloodOpacity();
  CSSValue* DoGetStopOpacity();
  CSSValue* DoGetStrokeMiterlimit();
  CSSValue* DoGetStrokeOpacity();

  CSSValue* DoGetClipRule();
  CSSValue* DoGetFillRule();
  CSSValue* DoGetStrokeLinecap();
  CSSValue* DoGetStrokeLinejoin();
  CSSValue* DoGetTextAnchor();

  CSSValue* DoGetColorInterpolation();
  CSSValue* DoGetColorInterpolationFilters();
  CSSValue* DoGetDominantBaseline();
  CSSValue* DoGetImageRendering();
  CSSValue* DoGetShapeRendering();
  CSSValue* DoGetTextRendering();

  CSSValue* DoGetFloodColor();
  CSSValue* DoGetLightingColor();
  CSSValue* DoGetStopColor();

  CSSValue* DoGetClipPath();
  CSSValue* DoGetFilter();
  CSSValue* DoGetMask();
  CSSValue* DoGetMaskType();
  CSSValue* DoGetPaintOrder();

  /* Custom properties */
  CSSValue* DoGetCustomProperty(const nsAString& aPropertyName);

  nsDOMCSSValueList* GetROCSSValueList(bool aCommaDelimited);

  /* Helper functions */
  void SetToRGBAColor(nsROCSSPrimitiveValue* aValue, nscolor aColor);
  void SetValueToStyleImage(const nsStyleImage& aStyleImage,
                            nsROCSSPrimitiveValue* aValue);
  void SetValueToPositionCoord(
    const nsStyleBackground::Position::PositionCoord& aCoord,
    nsROCSSPrimitiveValue* aValue);
  void SetValueToPosition(const nsStyleBackground::Position& aPosition,
                          nsDOMCSSValueList* aValueList);

  /**
   * A method to get a percentage base for a percentage value.  Returns true
   * if a percentage base value was determined, false otherwise.
   */
  typedef bool (nsComputedDOMStyle::*PercentageBaseGetter)(nscoord&);

  /**
   * Method to set aValue to aCoord.  If aCoord is a percentage value and
   * aPercentageBaseGetter is not null, aPercentageBaseGetter is called.  If it
   * returns true, the percentage base it outputs in its out param is used
   * to compute an nscoord value.  If the getter is null or returns false,
   * the percent value of aCoord is set as a percent value on aValue.  aTable,
   * if not null, is the keyword table to handle eStyleUnit_Enumerated.  When
   * calling SetAppUnits on aValue (for coord or percent values), the value
   * passed in will be clamped to be no less than aMinAppUnits and no more than
   * aMaxAppUnits.
   *
   * XXXbz should caller pass in some sort of bitfield indicating which units
   * can be expected or something?
   */
  void SetValueToCoord(nsROCSSPrimitiveValue* aValue,
                       const nsStyleCoord& aCoord,
                       bool aClampNegativeCalc,
                       PercentageBaseGetter aPercentageBaseGetter = nullptr,
                       const KTableEntry aTable[] = nullptr,
                       nscoord aMinAppUnits = nscoord_MIN,
                       nscoord aMaxAppUnits = nscoord_MAX);

  /**
   * If aCoord is a eStyleUnit_Coord returns the nscoord.  If it's
   * eStyleUnit_Percent, attempts to resolve the percentage base and returns
   * the resulting nscoord.  If it's some other unit or a percentge base can't
   * be determined, returns aDefaultValue.
   */
  nscoord StyleCoordToNSCoord(const nsStyleCoord& aCoord,
                              PercentageBaseGetter aPercentageBaseGetter,
                              nscoord aDefaultValue,
                              bool aClampNegativeCalc);

  bool GetCBContentWidth(nscoord& aWidth);
  bool GetCBContentHeight(nscoord& aWidth);
  bool GetScrollFrameContentWidth(nscoord& aWidth);
  bool GetScrollFrameContentHeight(nscoord& aHeight);
  bool GetFrameBoundsWidthForTransform(nscoord &aWidth);
  bool GetFrameBoundsHeightForTransform(nscoord &aHeight);
  bool GetFrameBorderRectWidth(nscoord& aWidth);
  bool GetFrameBorderRectHeight(nscoord& aHeight);

  /* Helper functions for computing the filter property style. */
  void SetCssTextToCoord(nsAString& aCssText, const nsStyleCoord& aCoord);
  already_AddRefed<CSSValue> CreatePrimitiveValueForStyleFilter(
    const nsStyleFilter& aStyleFilter);

  // Helper function for computing basic shape styles.
  CSSValue* CreatePrimitiveValueForClipPath(
    const nsStyleBasicShape* aStyleBasicShape, uint8_t aSizingBox);
  void BoxValuesToString(nsAString& aString,
                         const nsTArray<nsStyleCoord>& aBoxValues);
  void BasicShapeRadiiToString(nsAString& aCssText,
                               const nsStyleCorners& aCorners);


  static nsComputedStyleMap* GetComputedStyleMap();

  // We don't really have a good immutable representation of "presentation".
  // Given the way GetComputedStyle is currently used, we should just grab the
  // 0th presshell, if any, from the document.
  nsWeakPtr mDocumentWeak;
  nsCOMPtr<nsIContent> mContent;

  /**
   * Strong reference to the style context we access data from.  This can be
   * either a style context we resolved ourselves or a style context we got
   * from our frame.
   *
   * If we got the style context from the frame, we clear out mStyleContext
   * in ClearCurrentStyleSources.  If we resolved one ourselves, then
   * ClearCurrentStyleSources leaves it in mStyleContext for use the next
   * time this nsComputedDOMStyle object is queried.  UpdateCurrentStyleSources
   * in this case will check that the style context is still valid to be used,
   * by checking whether flush styles results in any restyles having been
   * processed.
   *
   * Since an ArenaRefPtr is used to hold the style context, it will be cleared
   * if the pres arena from which it was allocated goes away.
   */
  mozilla::ArenaRefPtr<nsStyleContext> mStyleContext;
  nsCOMPtr<nsIAtom> mPseudo;

  /*
   * While computing style data, the primary frame for mContent --- named "outer"
   * because we should use it to compute positioning data.  Null
   * otherwise.
   */
  nsIFrame* mOuterFrame;
  /*
   * While computing style data, the "inner frame" for mContent --- the frame
   * which we should use to compute margin, border, padding and content data.  Null
   * otherwise.
   */
  nsIFrame* mInnerFrame;
  /*
   * While computing style data, the presshell we're working with.  Null
   * otherwise.
   */
  nsIPresShell* mPresShell;

  /*
   * The kind of styles we should be returning.
   */
  StyleType mStyleType;

  /**
   * The nsComputedDOMStyle generation at the time we last resolved a style
   * context and stored it in mStyleContext.
   */
  uint64_t mStyleContextGeneration;

  bool mExposeVisitedStyle;

  /**
   * Whether we resolved a style context last time we called
   * UpdateCurrentStyleSources.  Initially false.
   */
  bool mResolvedStyleContext;

#ifdef DEBUG
  bool mFlushedPendingReflows;
#endif
};

already_AddRefed<nsComputedDOMStyle>
NS_NewComputedDOMStyle(mozilla::dom::Element* aElement,
                       const nsAString& aPseudoElt,
                       nsIPresShell* aPresShell,
                       nsComputedDOMStyle::StyleType aStyleType =
                         nsComputedDOMStyle::eAll);

#endif /* nsComputedDOMStyle_h__ */

