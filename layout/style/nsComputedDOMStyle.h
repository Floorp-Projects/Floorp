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
public:
  typedef nsCSSProps::KTableEntry KTableEntry;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsComputedDOMStyle,
                                                                   nsICSSDeclaration)

  NS_DECL_NSICSSDECLARATION

  NS_DECL_NSIDOMCSSSTYLEDECLARATION_HELPER
  virtual already_AddRefed<mozilla::dom::CSSValue>
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
  mozilla::dom::CSSValue* CreateTextAlignValue(uint8_t aAlign,
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

  mozilla::dom::CSSValue* GetEllipseRadii(const nsStyleCorners& aRadius,
                                          uint8_t aFullCorner,
                                          bool aIsBorder); // else outline

  mozilla::dom::CSSValue* GetOffsetWidthFor(mozilla::css::Side aSide);

  mozilla::dom::CSSValue* GetAbsoluteOffset(mozilla::css::Side aSide);

  mozilla::dom::CSSValue* GetRelativeOffset(mozilla::css::Side aSide);

  mozilla::dom::CSSValue* GetStickyOffset(mozilla::css::Side aSide);

  mozilla::dom::CSSValue* GetStaticOffset(mozilla::css::Side aSide);

  mozilla::dom::CSSValue* GetPaddingWidthFor(mozilla::css::Side aSide);

  mozilla::dom::CSSValue* GetBorderColorsFor(mozilla::css::Side aSide);

  mozilla::dom::CSSValue* GetBorderStyleFor(mozilla::css::Side aSide);

  mozilla::dom::CSSValue* GetBorderWidthFor(mozilla::css::Side aSide);

  mozilla::dom::CSSValue* GetBorderColorFor(mozilla::css::Side aSide);

  mozilla::dom::CSSValue* GetMarginWidthFor(mozilla::css::Side aSide);

  mozilla::dom::CSSValue* GetSVGPaintFor(bool aFill);

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
  mozilla::dom::CSSValue* GetGridTrackSize(const nsStyleCoord& aMinSize,
                                           const nsStyleCoord& aMaxSize);
  mozilla::dom::CSSValue* GetGridTemplateColumnsRows(const nsStyleGridTemplate& aTrackList,
                                                     const nsTArray<nscoord>* aTrackSizes);
  mozilla::dom::CSSValue* GetGridLine(const nsStyleGridLine& aGridLine);

  bool GetLineHeightCoord(nscoord& aCoord);

  mozilla::dom::CSSValue* GetCSSShadowArray(nsCSSShadowArray* aArray,
                                            const nscolor& aDefaultColor,
                                            bool aIsBoxShadow);

  mozilla::dom::CSSValue* GetBackgroundList(uint8_t nsStyleBackground::Layer::* aMember,
                                            uint32_t nsStyleBackground::* aCount,
                                            const KTableEntry aTable[]);

  void GetCSSGradientString(const nsStyleGradient* aGradient,
                            nsAString& aString);
  void GetImageRectString(nsIURI* aURI,
                          const nsStyleSides& aCropRect,
                          nsString& aString);
  mozilla::dom::CSSValue* GetScrollSnapPoints(const nsStyleCoord& aCoord);
  void AppendTimingFunction(nsDOMCSSValueList *aValueList,
                            const nsTimingFunction& aTimingFunction);

  /* Properties queryable as CSSValues.
   * To avoid a name conflict with nsIDOM*CSS2Properties, these are all
   * DoGetXXX instead of GetXXX.
   */

  mozilla::dom::CSSValue* DoGetAppearance();

  /* Box properties */
  mozilla::dom::CSSValue* DoGetBoxAlign();
  mozilla::dom::CSSValue* DoGetBoxDecorationBreak();
  mozilla::dom::CSSValue* DoGetBoxDirection();
  mozilla::dom::CSSValue* DoGetBoxFlex();
  mozilla::dom::CSSValue* DoGetBoxOrdinalGroup();
  mozilla::dom::CSSValue* DoGetBoxOrient();
  mozilla::dom::CSSValue* DoGetBoxPack();
  mozilla::dom::CSSValue* DoGetBoxSizing();

  mozilla::dom::CSSValue* DoGetWidth();
  mozilla::dom::CSSValue* DoGetHeight();
  mozilla::dom::CSSValue* DoGetMaxHeight();
  mozilla::dom::CSSValue* DoGetMaxWidth();
  mozilla::dom::CSSValue* DoGetMinHeight();
  mozilla::dom::CSSValue* DoGetMinWidth();
  mozilla::dom::CSSValue* DoGetMixBlendMode();
  mozilla::dom::CSSValue* DoGetIsolation();
  mozilla::dom::CSSValue* DoGetObjectFit();
  mozilla::dom::CSSValue* DoGetObjectPosition();
  mozilla::dom::CSSValue* DoGetLeft();
  mozilla::dom::CSSValue* DoGetTop();
  mozilla::dom::CSSValue* DoGetRight();
  mozilla::dom::CSSValue* DoGetBottom();
  mozilla::dom::CSSValue* DoGetStackSizing();

  /* Font properties */
  mozilla::dom::CSSValue* DoGetColor();
  mozilla::dom::CSSValue* DoGetFontFamily();
  mozilla::dom::CSSValue* DoGetFontFeatureSettings();
  mozilla::dom::CSSValue* DoGetFontKerning();
  mozilla::dom::CSSValue* DoGetFontLanguageOverride();
  mozilla::dom::CSSValue* DoGetFontSize();
  mozilla::dom::CSSValue* DoGetFontSizeAdjust();
  mozilla::dom::CSSValue* DoGetOsxFontSmoothing();
  mozilla::dom::CSSValue* DoGetFontStretch();
  mozilla::dom::CSSValue* DoGetFontStyle();
  mozilla::dom::CSSValue* DoGetFontSynthesis();
  mozilla::dom::CSSValue* DoGetFontVariant();
  mozilla::dom::CSSValue* DoGetFontVariantAlternates();
  mozilla::dom::CSSValue* DoGetFontVariantCaps();
  mozilla::dom::CSSValue* DoGetFontVariantEastAsian();
  mozilla::dom::CSSValue* DoGetFontVariantLigatures();
  mozilla::dom::CSSValue* DoGetFontVariantNumeric();
  mozilla::dom::CSSValue* DoGetFontVariantPosition();
  mozilla::dom::CSSValue* DoGetFontWeight();

  /* Grid properties */
  mozilla::dom::CSSValue* DoGetGridAutoFlow();
  mozilla::dom::CSSValue* DoGetGridAutoColumns();
  mozilla::dom::CSSValue* DoGetGridAutoRows();
  mozilla::dom::CSSValue* DoGetGridTemplateAreas();
  mozilla::dom::CSSValue* DoGetGridTemplateColumns();
  mozilla::dom::CSSValue* DoGetGridTemplateRows();
  mozilla::dom::CSSValue* DoGetGridColumnStart();
  mozilla::dom::CSSValue* DoGetGridColumnEnd();
  mozilla::dom::CSSValue* DoGetGridRowStart();
  mozilla::dom::CSSValue* DoGetGridRowEnd();
  mozilla::dom::CSSValue* DoGetGridColumnGap();
  mozilla::dom::CSSValue* DoGetGridRowGap();

  /* Background properties */
  mozilla::dom::CSSValue* DoGetBackgroundAttachment();
  mozilla::dom::CSSValue* DoGetBackgroundColor();
  mozilla::dom::CSSValue* DoGetBackgroundImage();
  mozilla::dom::CSSValue* DoGetBackgroundPosition();
  mozilla::dom::CSSValue* DoGetBackgroundRepeat();
  mozilla::dom::CSSValue* DoGetBackgroundClip();
  mozilla::dom::CSSValue* DoGetBackgroundBlendMode();
  mozilla::dom::CSSValue* DoGetBackgroundOrigin();
  mozilla::dom::CSSValue* DoGetBackgroundSize();

  /* Padding properties */
  mozilla::dom::CSSValue* DoGetPaddingTop();
  mozilla::dom::CSSValue* DoGetPaddingBottom();
  mozilla::dom::CSSValue* DoGetPaddingLeft();
  mozilla::dom::CSSValue* DoGetPaddingRight();

  /* Table Properties */
  mozilla::dom::CSSValue* DoGetBorderCollapse();
  mozilla::dom::CSSValue* DoGetBorderSpacing();
  mozilla::dom::CSSValue* DoGetCaptionSide();
  mozilla::dom::CSSValue* DoGetEmptyCells();
  mozilla::dom::CSSValue* DoGetTableLayout();
  mozilla::dom::CSSValue* DoGetVerticalAlign();

  /* Border Properties */
  mozilla::dom::CSSValue* DoGetBorderTopStyle();
  mozilla::dom::CSSValue* DoGetBorderBottomStyle();
  mozilla::dom::CSSValue* DoGetBorderLeftStyle();
  mozilla::dom::CSSValue* DoGetBorderRightStyle();
  mozilla::dom::CSSValue* DoGetBorderTopWidth();
  mozilla::dom::CSSValue* DoGetBorderBottomWidth();
  mozilla::dom::CSSValue* DoGetBorderLeftWidth();
  mozilla::dom::CSSValue* DoGetBorderRightWidth();
  mozilla::dom::CSSValue* DoGetBorderTopColor();
  mozilla::dom::CSSValue* DoGetBorderBottomColor();
  mozilla::dom::CSSValue* DoGetBorderLeftColor();
  mozilla::dom::CSSValue* DoGetBorderRightColor();
  mozilla::dom::CSSValue* DoGetBorderBottomColors();
  mozilla::dom::CSSValue* DoGetBorderLeftColors();
  mozilla::dom::CSSValue* DoGetBorderRightColors();
  mozilla::dom::CSSValue* DoGetBorderTopColors();
  mozilla::dom::CSSValue* DoGetBorderBottomLeftRadius();
  mozilla::dom::CSSValue* DoGetBorderBottomRightRadius();
  mozilla::dom::CSSValue* DoGetBorderTopLeftRadius();
  mozilla::dom::CSSValue* DoGetBorderTopRightRadius();
  mozilla::dom::CSSValue* DoGetFloatEdge();

  /* Border Image */
  mozilla::dom::CSSValue* DoGetBorderImageSource();
  mozilla::dom::CSSValue* DoGetBorderImageSlice();
  mozilla::dom::CSSValue* DoGetBorderImageWidth();
  mozilla::dom::CSSValue* DoGetBorderImageOutset();
  mozilla::dom::CSSValue* DoGetBorderImageRepeat();

  /* Box Shadow */
  mozilla::dom::CSSValue* DoGetBoxShadow();

  /* Window Shadow */
  mozilla::dom::CSSValue* DoGetWindowShadow();

  /* Margin Properties */
  mozilla::dom::CSSValue* DoGetMarginTopWidth();
  mozilla::dom::CSSValue* DoGetMarginBottomWidth();
  mozilla::dom::CSSValue* DoGetMarginLeftWidth();
  mozilla::dom::CSSValue* DoGetMarginRightWidth();

  /* Outline Properties */
  mozilla::dom::CSSValue* DoGetOutlineWidth();
  mozilla::dom::CSSValue* DoGetOutlineStyle();
  mozilla::dom::CSSValue* DoGetOutlineColor();
  mozilla::dom::CSSValue* DoGetOutlineOffset();
  mozilla::dom::CSSValue* DoGetOutlineRadiusBottomLeft();
  mozilla::dom::CSSValue* DoGetOutlineRadiusBottomRight();
  mozilla::dom::CSSValue* DoGetOutlineRadiusTopLeft();
  mozilla::dom::CSSValue* DoGetOutlineRadiusTopRight();

  /* Content Properties */
  mozilla::dom::CSSValue* DoGetContent();
  mozilla::dom::CSSValue* DoGetCounterIncrement();
  mozilla::dom::CSSValue* DoGetCounterReset();
  mozilla::dom::CSSValue* DoGetMarkerOffset();

  /* Quotes Properties */
  mozilla::dom::CSSValue* DoGetQuotes();

  /* z-index */
  mozilla::dom::CSSValue* DoGetZIndex();

  /* List properties */
  mozilla::dom::CSSValue* DoGetListStyleImage();
  mozilla::dom::CSSValue* DoGetListStylePosition();
  mozilla::dom::CSSValue* DoGetListStyleType();
  mozilla::dom::CSSValue* DoGetImageRegion();

  /* Text Properties */
  mozilla::dom::CSSValue* DoGetLineHeight();
  mozilla::dom::CSSValue* DoGetRubyAlign();
  mozilla::dom::CSSValue* DoGetRubyPosition();
  mozilla::dom::CSSValue* DoGetTextAlign();
  mozilla::dom::CSSValue* DoGetTextAlignLast();
  mozilla::dom::CSSValue* DoGetTextCombineUpright();
  mozilla::dom::CSSValue* DoGetTextDecoration();
  mozilla::dom::CSSValue* DoGetTextDecorationColor();
  mozilla::dom::CSSValue* DoGetTextDecorationLine();
  mozilla::dom::CSSValue* DoGetTextDecorationStyle();
  mozilla::dom::CSSValue* DoGetTextEmphasisColor();
  mozilla::dom::CSSValue* DoGetTextEmphasisPosition();
  mozilla::dom::CSSValue* DoGetTextEmphasisStyle();
  mozilla::dom::CSSValue* DoGetTextIndent();
  mozilla::dom::CSSValue* DoGetTextOrientation();
  mozilla::dom::CSSValue* DoGetTextOverflow();
  mozilla::dom::CSSValue* DoGetTextTransform();
  mozilla::dom::CSSValue* DoGetTextShadow();
  mozilla::dom::CSSValue* DoGetLetterSpacing();
  mozilla::dom::CSSValue* DoGetWordSpacing();
  mozilla::dom::CSSValue* DoGetWhiteSpace();
  mozilla::dom::CSSValue* DoGetWordBreak();
  mozilla::dom::CSSValue* DoGetWordWrap();
  mozilla::dom::CSSValue* DoGetHyphens();
  mozilla::dom::CSSValue* DoGetTabSize();
  mozilla::dom::CSSValue* DoGetTextSizeAdjust();

  /* Visibility properties */
  mozilla::dom::CSSValue* DoGetOpacity();
  mozilla::dom::CSSValue* DoGetPointerEvents();
  mozilla::dom::CSSValue* DoGetVisibility();
  mozilla::dom::CSSValue* DoGetWritingMode();

  /* Direction properties */
  mozilla::dom::CSSValue* DoGetDirection();
  mozilla::dom::CSSValue* DoGetUnicodeBidi();

  /* Display properties */
  mozilla::dom::CSSValue* DoGetBinding();
  mozilla::dom::CSSValue* DoGetClear();
  mozilla::dom::CSSValue* DoGetFloat();
  mozilla::dom::CSSValue* DoGetDisplay();
  mozilla::dom::CSSValue* DoGetContain();
  mozilla::dom::CSSValue* DoGetPosition();
  mozilla::dom::CSSValue* DoGetClip();
  mozilla::dom::CSSValue* DoGetImageOrientation();
  mozilla::dom::CSSValue* DoGetWillChange();
  mozilla::dom::CSSValue* DoGetOverflow();
  mozilla::dom::CSSValue* DoGetOverflowX();
  mozilla::dom::CSSValue* DoGetOverflowY();
  mozilla::dom::CSSValue* DoGetOverflowClipBox();
  mozilla::dom::CSSValue* DoGetResize();
  mozilla::dom::CSSValue* DoGetPageBreakAfter();
  mozilla::dom::CSSValue* DoGetPageBreakBefore();
  mozilla::dom::CSSValue* DoGetPageBreakInside();
  mozilla::dom::CSSValue* DoGetTouchAction();
  mozilla::dom::CSSValue* DoGetTransform();
  mozilla::dom::CSSValue* DoGetTransformBox();
  mozilla::dom::CSSValue* DoGetTransformOrigin();
  mozilla::dom::CSSValue* DoGetPerspective();
  mozilla::dom::CSSValue* DoGetBackfaceVisibility();
  mozilla::dom::CSSValue* DoGetPerspectiveOrigin();
  mozilla::dom::CSSValue* DoGetTransformStyle();
  mozilla::dom::CSSValue* DoGetOrient();
  mozilla::dom::CSSValue* DoGetScrollBehavior();
  mozilla::dom::CSSValue* DoGetScrollSnapType();
  mozilla::dom::CSSValue* DoGetScrollSnapTypeX();
  mozilla::dom::CSSValue* DoGetScrollSnapTypeY();
  mozilla::dom::CSSValue* DoGetScrollSnapPointsX();
  mozilla::dom::CSSValue* DoGetScrollSnapPointsY();
  mozilla::dom::CSSValue* DoGetScrollSnapDestination();
  mozilla::dom::CSSValue* DoGetScrollSnapCoordinate();

  /* User interface properties */
  mozilla::dom::CSSValue* DoGetCursor();
  mozilla::dom::CSSValue* DoGetForceBrokenImageIcon();
  mozilla::dom::CSSValue* DoGetIMEMode();
  mozilla::dom::CSSValue* DoGetUserFocus();
  mozilla::dom::CSSValue* DoGetUserInput();
  mozilla::dom::CSSValue* DoGetUserModify();
  mozilla::dom::CSSValue* DoGetUserSelect();
  mozilla::dom::CSSValue* DoGetWindowDragging();

  /* Column properties */
  mozilla::dom::CSSValue* DoGetColumnCount();
  mozilla::dom::CSSValue* DoGetColumnFill();
  mozilla::dom::CSSValue* DoGetColumnWidth();
  mozilla::dom::CSSValue* DoGetColumnGap();
  mozilla::dom::CSSValue* DoGetColumnRuleWidth();
  mozilla::dom::CSSValue* DoGetColumnRuleStyle();
  mozilla::dom::CSSValue* DoGetColumnRuleColor();

  /* CSS Transitions */
  mozilla::dom::CSSValue* DoGetTransitionProperty();
  mozilla::dom::CSSValue* DoGetTransitionDuration();
  mozilla::dom::CSSValue* DoGetTransitionDelay();
  mozilla::dom::CSSValue* DoGetTransitionTimingFunction();

  /* CSS Animations */
  mozilla::dom::CSSValue* DoGetAnimationName();
  mozilla::dom::CSSValue* DoGetAnimationDuration();
  mozilla::dom::CSSValue* DoGetAnimationDelay();
  mozilla::dom::CSSValue* DoGetAnimationTimingFunction();
  mozilla::dom::CSSValue* DoGetAnimationDirection();
  mozilla::dom::CSSValue* DoGetAnimationFillMode();
  mozilla::dom::CSSValue* DoGetAnimationIterationCount();
  mozilla::dom::CSSValue* DoGetAnimationPlayState();

  /* CSS Flexbox properties */
  mozilla::dom::CSSValue* DoGetFlexBasis();
  mozilla::dom::CSSValue* DoGetFlexDirection();
  mozilla::dom::CSSValue* DoGetFlexGrow();
  mozilla::dom::CSSValue* DoGetFlexShrink();
  mozilla::dom::CSSValue* DoGetFlexWrap();

  /* CSS Flexbox/Grid properties */
  mozilla::dom::CSSValue* DoGetOrder();

  /* CSS Box Alignment properties */
  mozilla::dom::CSSValue* DoGetAlignContent();
  mozilla::dom::CSSValue* DoGetAlignItems();
  mozilla::dom::CSSValue* DoGetAlignSelf();
  mozilla::dom::CSSValue* DoGetJustifyContent();
  mozilla::dom::CSSValue* DoGetJustifyItems();
  mozilla::dom::CSSValue* DoGetJustifySelf();

  /* SVG properties */
  mozilla::dom::CSSValue* DoGetFill();
  mozilla::dom::CSSValue* DoGetStroke();
  mozilla::dom::CSSValue* DoGetMarkerEnd();
  mozilla::dom::CSSValue* DoGetMarkerMid();
  mozilla::dom::CSSValue* DoGetMarkerStart();
  mozilla::dom::CSSValue* DoGetStrokeDasharray();

  mozilla::dom::CSSValue* DoGetStrokeDashoffset();
  mozilla::dom::CSSValue* DoGetStrokeWidth();
  mozilla::dom::CSSValue* DoGetVectorEffect();

  mozilla::dom::CSSValue* DoGetFillOpacity();
  mozilla::dom::CSSValue* DoGetFloodOpacity();
  mozilla::dom::CSSValue* DoGetStopOpacity();
  mozilla::dom::CSSValue* DoGetStrokeMiterlimit();
  mozilla::dom::CSSValue* DoGetStrokeOpacity();

  mozilla::dom::CSSValue* DoGetClipRule();
  mozilla::dom::CSSValue* DoGetFillRule();
  mozilla::dom::CSSValue* DoGetStrokeLinecap();
  mozilla::dom::CSSValue* DoGetStrokeLinejoin();
  mozilla::dom::CSSValue* DoGetTextAnchor();

  mozilla::dom::CSSValue* DoGetColorInterpolation();
  mozilla::dom::CSSValue* DoGetColorInterpolationFilters();
  mozilla::dom::CSSValue* DoGetDominantBaseline();
  mozilla::dom::CSSValue* DoGetImageRendering();
  mozilla::dom::CSSValue* DoGetShapeRendering();
  mozilla::dom::CSSValue* DoGetTextRendering();

  mozilla::dom::CSSValue* DoGetFloodColor();
  mozilla::dom::CSSValue* DoGetLightingColor();
  mozilla::dom::CSSValue* DoGetStopColor();

  mozilla::dom::CSSValue* DoGetClipPath();
  mozilla::dom::CSSValue* DoGetFilter();
  mozilla::dom::CSSValue* DoGetMask();
  mozilla::dom::CSSValue* DoGetMaskType();
  mozilla::dom::CSSValue* DoGetPaintOrder();

  /* Custom properties */
  mozilla::dom::CSSValue* DoGetCustomProperty(const nsAString& aPropertyName);

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
  already_AddRefed<mozilla::dom::CSSValue> CreatePrimitiveValueForStyleFilter(
    const nsStyleFilter& aStyleFilter);

  // Helper function for computing basic shape styles.
  mozilla::dom::CSSValue* CreatePrimitiveValueForClipPath(
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

