/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object returned from element.getComputedStyle() */

#ifndef nsComputedDOMStyle_h__
#define nsComputedDOMStyle_h__

#include "nsDOMCSSDeclaration.h"

#include "nsROCSSPrimitiveValue.h"
#include "nsDOMCSSRGBColor.h"
#include "nsDOMCSSValueList.h"
#include "nsCSSProps.h"

#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsAutoPtr.h"
#include "nsStyleStruct.h"

class nsIPresShell;

class nsComputedDOMStyle : public nsDOMCSSDeclaration,
                           public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_CLASS_AMBIGUOUS(nsComputedDOMStyle,
                                                     nsICSSDeclaration)

  NS_IMETHOD Init(nsIDOMElement *aElement,
                  const nsAString& aPseudoElt,
                  nsIPresShell *aPresShell);

  NS_DECL_NSICSSDECLARATION

  NS_DECL_NSIDOMCSSSTYLEDECLARATION

  nsComputedDOMStyle();
  virtual ~nsComputedDOMStyle();

  static void Shutdown();

  virtual nsINode *GetParentObject()
  {
    return mContent;
  }

  static already_AddRefed<nsStyleContext>
  GetStyleContextForElement(mozilla::dom::Element* aElement, nsIAtom* aPseudo,
                            nsIPresShell* aPresShell);

  static already_AddRefed<nsStyleContext>
  GetStyleContextForElementNoFlush(mozilla::dom::Element* aElement,
                                   nsIAtom* aPseudo,
                                   nsIPresShell* aPresShell);

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
  virtual mozilla::css::Declaration* GetCSSDeclaration(bool);
  virtual nsresult SetCSSDeclaration(mozilla::css::Declaration*);
  virtual nsIDocument* DocToUpdate();
  virtual void GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv);

private:
  void AssertFlushedPendingReflows() {
    NS_ASSERTION(mFlushedPendingReflows,
                 "property getter should have been marked layout-dependent");
  }

#define STYLE_STRUCT(name_, checkdata_cb_, ctor_args_)                  \
  const nsStyle##name_ * GetStyle##name_() {                            \
    return mStyleContextHolder->GetStyle##name_();                      \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  // All of the property getters below return a pointer to a refcounted object
  // that has just been created, but the refcount is still 0. Caller must take
  // ownership.

  nsIDOMCSSValue* GetEllipseRadii(const nsStyleCorners& aRadius,
                                  PRUint8 aFullCorner,
                                  bool aIsBorder); // else outline

  nsIDOMCSSValue* GetOffsetWidthFor(mozilla::css::Side aSide);

  nsIDOMCSSValue* GetAbsoluteOffset(mozilla::css::Side aSide);

  nsIDOMCSSValue* GetRelativeOffset(mozilla::css::Side aSide);

  nsIDOMCSSValue* GetStaticOffset(mozilla::css::Side aSide);

  nsIDOMCSSValue* GetPaddingWidthFor(mozilla::css::Side aSide);

  nsIDOMCSSValue* GetBorderColorsFor(mozilla::css::Side aSide);

  nsIDOMCSSValue* GetBorderStyleFor(mozilla::css::Side aSide);

  nsIDOMCSSValue* GetBorderWidthFor(mozilla::css::Side aSide);

  nsIDOMCSSValue* GetBorderColorFor(mozilla::css::Side aSide);

  nsIDOMCSSValue* GetMarginWidthFor(mozilla::css::Side aSide);

  nsIDOMCSSValue* GetSVGPaintFor(bool aFill);

  bool GetLineHeightCoord(nscoord& aCoord);

  nsIDOMCSSValue* GetCSSShadowArray(nsCSSShadowArray* aArray,
                                    const nscolor& aDefaultColor,
                                    bool aIsBoxShadow);

  nsIDOMCSSValue* GetBackgroundList(PRUint8 nsStyleBackground::Layer::* aMember,
                                    PRUint32 nsStyleBackground::* aCount,
                                    const PRInt32 aTable[]);

  void GetCSSGradientString(const nsStyleGradient* aGradient,
                            nsAString& aString);
  void GetImageRectString(nsIURI* aURI,
                          const nsStyleSides& aCropRect,
                          nsString& aString);
  void AppendTimingFunction(nsDOMCSSValueList *aValueList,
                            const nsTimingFunction& aTimingFunction);

  /* Properties queryable as CSSValues.
   * To avoid a name conflict with nsIDOM*CSS2Properties, these are all
   * DoGetXXX instead of GetXXX.
   */

  nsIDOMCSSValue* DoGetAppearance();

  /* Box properties */
  nsIDOMCSSValue* DoGetBoxAlign();
  nsIDOMCSSValue* DoGetBoxDirection();
  nsIDOMCSSValue* DoGetBoxFlex();
  nsIDOMCSSValue* DoGetBoxOrdinalGroup();
  nsIDOMCSSValue* DoGetBoxOrient();
  nsIDOMCSSValue* DoGetBoxPack();
  nsIDOMCSSValue* DoGetBoxSizing();

  nsIDOMCSSValue* DoGetWidth();
  nsIDOMCSSValue* DoGetHeight();
  nsIDOMCSSValue* DoGetMaxHeight();
  nsIDOMCSSValue* DoGetMaxWidth();
  nsIDOMCSSValue* DoGetMinHeight();
  nsIDOMCSSValue* DoGetMinWidth();
  nsIDOMCSSValue* DoGetLeft();
  nsIDOMCSSValue* DoGetTop();
  nsIDOMCSSValue* DoGetRight();
  nsIDOMCSSValue* DoGetBottom();
  nsIDOMCSSValue* DoGetStackSizing();

  /* Font properties */
  nsIDOMCSSValue* DoGetColor();
  nsIDOMCSSValue* DoGetFontFamily();
  nsIDOMCSSValue* DoGetFontFeatureSettings();
  nsIDOMCSSValue* DoGetFontLanguageOverride();
  nsIDOMCSSValue* DoGetFontSize();
  nsIDOMCSSValue* DoGetFontSizeAdjust();
  nsIDOMCSSValue* DoGetFontStretch();
  nsIDOMCSSValue* DoGetFontStyle();
  nsIDOMCSSValue* DoGetFontWeight();
  nsIDOMCSSValue* DoGetFontVariant();

  /* Background properties */
  nsIDOMCSSValue* DoGetBackgroundAttachment();
  nsIDOMCSSValue* DoGetBackgroundColor();
  nsIDOMCSSValue* DoGetBackgroundImage();
  nsIDOMCSSValue* DoGetBackgroundPosition();
  nsIDOMCSSValue* DoGetBackgroundRepeat();
  nsIDOMCSSValue* DoGetBackgroundClip();
  nsIDOMCSSValue* DoGetBackgroundInlinePolicy();
  nsIDOMCSSValue* DoGetBackgroundOrigin();
  nsIDOMCSSValue* DoGetBackgroundSize();

  /* Padding properties */
  nsIDOMCSSValue* DoGetPaddingTop();
  nsIDOMCSSValue* DoGetPaddingBottom();
  nsIDOMCSSValue* DoGetPaddingLeft();
  nsIDOMCSSValue* DoGetPaddingRight();

  /* Table Properties */
  nsIDOMCSSValue* DoGetBorderCollapse();
  nsIDOMCSSValue* DoGetBorderSpacing();
  nsIDOMCSSValue* DoGetCaptionSide();
  nsIDOMCSSValue* DoGetEmptyCells();
  nsIDOMCSSValue* DoGetTableLayout();
  nsIDOMCSSValue* DoGetVerticalAlign();

  /* Border Properties */
  nsIDOMCSSValue* DoGetBorderTopStyle();
  nsIDOMCSSValue* DoGetBorderBottomStyle();
  nsIDOMCSSValue* DoGetBorderLeftStyle();
  nsIDOMCSSValue* DoGetBorderRightStyle();
  nsIDOMCSSValue* DoGetBorderTopWidth();
  nsIDOMCSSValue* DoGetBorderBottomWidth();
  nsIDOMCSSValue* DoGetBorderLeftWidth();
  nsIDOMCSSValue* DoGetBorderRightWidth();
  nsIDOMCSSValue* DoGetBorderTopColor();
  nsIDOMCSSValue* DoGetBorderBottomColor();
  nsIDOMCSSValue* DoGetBorderLeftColor();
  nsIDOMCSSValue* DoGetBorderRightColor();
  nsIDOMCSSValue* DoGetBorderBottomColors();
  nsIDOMCSSValue* DoGetBorderLeftColors();
  nsIDOMCSSValue* DoGetBorderRightColors();
  nsIDOMCSSValue* DoGetBorderTopColors();
  nsIDOMCSSValue* DoGetBorderBottomLeftRadius();
  nsIDOMCSSValue* DoGetBorderBottomRightRadius();
  nsIDOMCSSValue* DoGetBorderTopLeftRadius();
  nsIDOMCSSValue* DoGetBorderTopRightRadius();
  nsIDOMCSSValue* DoGetFloatEdge();

  /* Border Image */
  nsIDOMCSSValue* DoGetBorderImageSource();
  nsIDOMCSSValue* DoGetBorderImageSlice();
  nsIDOMCSSValue* DoGetBorderImageWidth();
  nsIDOMCSSValue* DoGetBorderImageOutset();
  nsIDOMCSSValue* DoGetBorderImageRepeat();

  /* Box Shadow */
  nsIDOMCSSValue* DoGetBoxShadow();

  /* Window Shadow */
  nsIDOMCSSValue* DoGetWindowShadow();

  /* Margin Properties */
  nsIDOMCSSValue* DoGetMarginTopWidth();
  nsIDOMCSSValue* DoGetMarginBottomWidth();
  nsIDOMCSSValue* DoGetMarginLeftWidth();
  nsIDOMCSSValue* DoGetMarginRightWidth();

  /* Outline Properties */
  nsIDOMCSSValue* DoGetOutlineWidth();
  nsIDOMCSSValue* DoGetOutlineStyle();
  nsIDOMCSSValue* DoGetOutlineColor();
  nsIDOMCSSValue* DoGetOutlineOffset();
  nsIDOMCSSValue* DoGetOutlineRadiusBottomLeft();
  nsIDOMCSSValue* DoGetOutlineRadiusBottomRight();
  nsIDOMCSSValue* DoGetOutlineRadiusTopLeft();
  nsIDOMCSSValue* DoGetOutlineRadiusTopRight();

  /* Content Properties */
  nsIDOMCSSValue* DoGetContent();
  nsIDOMCSSValue* DoGetCounterIncrement();
  nsIDOMCSSValue* DoGetCounterReset();
  nsIDOMCSSValue* DoGetMarkerOffset();

  /* Quotes Properties */
  nsIDOMCSSValue* DoGetQuotes();

  /* z-index */
  nsIDOMCSSValue* DoGetZIndex();

  /* List properties */
  nsIDOMCSSValue* DoGetListStyleImage();
  nsIDOMCSSValue* DoGetListStylePosition();
  nsIDOMCSSValue* DoGetListStyleType();
  nsIDOMCSSValue* DoGetImageRegion();

  /* Text Properties */
  nsIDOMCSSValue* DoGetLineHeight();
  nsIDOMCSSValue* DoGetTextAlign();
  nsIDOMCSSValue* DoGetTextAlignLast();
  nsIDOMCSSValue* DoGetMozTextBlink();
  nsIDOMCSSValue* DoGetTextDecoration();
  nsIDOMCSSValue* DoGetTextDecorationColor();
  nsIDOMCSSValue* DoGetTextDecorationLine();
  nsIDOMCSSValue* DoGetTextDecorationStyle();
  nsIDOMCSSValue* DoGetTextIndent();
  nsIDOMCSSValue* DoGetTextOverflow();
  nsIDOMCSSValue* DoGetTextTransform();
  nsIDOMCSSValue* DoGetTextShadow();
  nsIDOMCSSValue* DoGetLetterSpacing();
  nsIDOMCSSValue* DoGetWordSpacing();
  nsIDOMCSSValue* DoGetWhiteSpace();
  nsIDOMCSSValue* DoGetWordBreak();
  nsIDOMCSSValue* DoGetWordWrap();
  nsIDOMCSSValue* DoGetHyphens();
  nsIDOMCSSValue* DoGetTabSize();
  nsIDOMCSSValue* DoGetTextSizeAdjust();

  /* Visibility properties */
  nsIDOMCSSValue* DoGetOpacity();
  nsIDOMCSSValue* DoGetPointerEvents();
  nsIDOMCSSValue* DoGetVisibility();

  /* Direction properties */
  nsIDOMCSSValue* DoGetDirection();
  nsIDOMCSSValue* DoGetUnicodeBidi();

  /* Display properties */
  nsIDOMCSSValue* DoGetBinding();
  nsIDOMCSSValue* DoGetClear();
  nsIDOMCSSValue* DoGetCssFloat();
  nsIDOMCSSValue* DoGetDisplay();
  nsIDOMCSSValue* DoGetPosition();
  nsIDOMCSSValue* DoGetClip();
  nsIDOMCSSValue* DoGetOverflow();
  nsIDOMCSSValue* DoGetOverflowX();
  nsIDOMCSSValue* DoGetOverflowY();
  nsIDOMCSSValue* DoGetResize();
  nsIDOMCSSValue* DoGetPageBreakAfter();
  nsIDOMCSSValue* DoGetPageBreakBefore();
  nsIDOMCSSValue* DoGetTransform();
  nsIDOMCSSValue* DoGetTransformOrigin();
  nsIDOMCSSValue* DoGetPerspective();
  nsIDOMCSSValue* DoGetBackfaceVisibility();
  nsIDOMCSSValue* DoGetPerspectiveOrigin();
  nsIDOMCSSValue* DoGetTransformStyle();
  nsIDOMCSSValue* DoGetOrient();

  /* User interface properties */
  nsIDOMCSSValue* DoGetCursor();
  nsIDOMCSSValue* DoGetForceBrokenImageIcon();
  nsIDOMCSSValue* DoGetIMEMode();
  nsIDOMCSSValue* DoGetUserFocus();
  nsIDOMCSSValue* DoGetUserInput();
  nsIDOMCSSValue* DoGetUserModify();
  nsIDOMCSSValue* DoGetUserSelect();

  /* Column properties */
  nsIDOMCSSValue* DoGetColumnCount();
  nsIDOMCSSValue* DoGetColumnFill();
  nsIDOMCSSValue* DoGetColumnWidth();
  nsIDOMCSSValue* DoGetColumnGap();
  nsIDOMCSSValue* DoGetColumnRuleWidth();
  nsIDOMCSSValue* DoGetColumnRuleStyle();
  nsIDOMCSSValue* DoGetColumnRuleColor();

  /* CSS Transitions */
  nsIDOMCSSValue* DoGetTransitionProperty();
  nsIDOMCSSValue* DoGetTransitionDuration();
  nsIDOMCSSValue* DoGetTransitionDelay();
  nsIDOMCSSValue* DoGetTransitionTimingFunction();

  /* CSS Animations */
  nsIDOMCSSValue* DoGetAnimationName();
  nsIDOMCSSValue* DoGetAnimationDuration();
  nsIDOMCSSValue* DoGetAnimationDelay();
  nsIDOMCSSValue* DoGetAnimationTimingFunction();
  nsIDOMCSSValue* DoGetAnimationDirection();
  nsIDOMCSSValue* DoGetAnimationFillMode();
  nsIDOMCSSValue* DoGetAnimationIterationCount();
  nsIDOMCSSValue* DoGetAnimationPlayState();

  /* SVG properties */
  nsIDOMCSSValue* DoGetFill();
  nsIDOMCSSValue* DoGetStroke();
  nsIDOMCSSValue* DoGetMarkerEnd();
  nsIDOMCSSValue* DoGetMarkerMid();
  nsIDOMCSSValue* DoGetMarkerStart();
  nsIDOMCSSValue* DoGetStrokeDasharray();

  nsIDOMCSSValue* DoGetStrokeDashoffset();
  nsIDOMCSSValue* DoGetStrokeWidth();
  nsIDOMCSSValue* DoGetVectorEffect();

  nsIDOMCSSValue* DoGetFillOpacity();
  nsIDOMCSSValue* DoGetFloodOpacity();
  nsIDOMCSSValue* DoGetStopOpacity();
  nsIDOMCSSValue* DoGetStrokeMiterlimit();
  nsIDOMCSSValue* DoGetStrokeOpacity();

  nsIDOMCSSValue* DoGetClipRule();
  nsIDOMCSSValue* DoGetFillRule();
  nsIDOMCSSValue* DoGetStrokeLinecap();
  nsIDOMCSSValue* DoGetStrokeLinejoin();
  nsIDOMCSSValue* DoGetTextAnchor();

  nsIDOMCSSValue* DoGetColorInterpolation();
  nsIDOMCSSValue* DoGetColorInterpolationFilters();
  nsIDOMCSSValue* DoGetDominantBaseline();
  nsIDOMCSSValue* DoGetImageRendering();
  nsIDOMCSSValue* DoGetShapeRendering();
  nsIDOMCSSValue* DoGetTextRendering();

  nsIDOMCSSValue* DoGetFloodColor();
  nsIDOMCSSValue* DoGetLightingColor();
  nsIDOMCSSValue* DoGetStopColor();

  nsIDOMCSSValue* DoGetClipPath();
  nsIDOMCSSValue* DoGetFilter();
  nsIDOMCSSValue* DoGetMask();

  nsROCSSPrimitiveValue* GetROCSSPrimitiveValue();
  nsDOMCSSValueList* GetROCSSValueList(bool aCommaDelimited);
  void SetToRGBAColor(nsROCSSPrimitiveValue* aValue, nscolor aColor);
  void SetValueToStyleImage(const nsStyleImage& aStyleImage,
                            nsROCSSPrimitiveValue* aValue);

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
                       PercentageBaseGetter aPercentageBaseGetter = nsnull,
                       const PRInt32 aTable[] = nsnull,
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
  bool GetFrameBoundsWidthForTransform(nscoord &aWidth);
  bool GetFrameBoundsHeightForTransform(nscoord &aHeight);
  bool GetFrameBorderRectWidth(nscoord& aWidth);
  bool GetFrameBorderRectHeight(nscoord& aHeight);

  struct ComputedStyleMapEntry
  {
    // Create a pointer-to-member-function type.
    typedef nsIDOMCSSValue* (nsComputedDOMStyle::*ComputeMethod)();

    nsCSSProperty mProperty;
    ComputeMethod mGetter;
    bool mNeedsLayoutFlush;
  };

  static const ComputedStyleMapEntry* GetQueryablePropertyMap(PRUint32* aLength);

  // We don't really have a good immutable representation of "presentation".
  // Given the way GetComputedStyle is currently used, we should just grab the
  // 0th presshell, if any, from the document.
  nsWeakPtr mDocumentWeak;
  nsCOMPtr<nsIContent> mContent;

  /*
   * Strong reference to the style context while we're accessing the data from
   * it.  This can be either a style context we resolved ourselves or a style
   * context we got from our frame.
   */
  nsRefPtr<nsStyleContext> mStyleContextHolder;
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

  bool mExposeVisitedStyle;

#ifdef DEBUG
  bool mFlushedPendingReflows;
#endif
};

nsresult
NS_NewComputedDOMStyle(nsIDOMElement *aElement, const nsAString &aPseudoElt,
                       nsIPresShell *aPresShell,
                       nsComputedDOMStyle **aComputedStyle);

#endif /* nsComputedDOMStyle_h__ */

