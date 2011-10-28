/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher A. Aillon <christopher@aillon.com>
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher A. Aillon <christopher@aillon.com>
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
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsComputedDOMStyle,
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
  nsIDOMCSSValue* DoGetMozFontFeatureSettings();
  nsIDOMCSSValue* DoGetMozFontLanguageOverride();
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
  nsIDOMCSSValue* DoGetMozBackgroundSize();

  /* Padding properties */
  nsIDOMCSSValue* DoGetPadding();
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
  nsIDOMCSSValue* DoGetBorderStyle();
  nsIDOMCSSValue* DoGetBorderWidth();
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
  nsIDOMCSSValue* DoGetBorderImage();

  /* Box Shadow */
  nsIDOMCSSValue* DoGetBoxShadow();

  /* Window Shadow */
  nsIDOMCSSValue* DoGetWindowShadow();

  /* Margin Properties */
  nsIDOMCSSValue* DoGetMarginWidth();
  nsIDOMCSSValue* DoGetMarginTopWidth();
  nsIDOMCSSValue* DoGetMarginBottomWidth();
  nsIDOMCSSValue* DoGetMarginLeftWidth();
  nsIDOMCSSValue* DoGetMarginRightWidth();

  /* Outline Properties */
  nsIDOMCSSValue* DoGetOutline();
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
  nsIDOMCSSValue* DoGetMozTextBlink();
  nsIDOMCSSValue* DoGetTextDecoration();
  nsIDOMCSSValue* DoGetMozTextDecorationColor();
  nsIDOMCSSValue* DoGetMozTextDecorationLine();
  nsIDOMCSSValue* DoGetMozTextDecorationStyle();
  nsIDOMCSSValue* DoGetTextIndent();
  nsIDOMCSSValue* DoGetTextOverflow();
  nsIDOMCSSValue* DoGetTextTransform();
  nsIDOMCSSValue* DoGetTextShadow();
  nsIDOMCSSValue* DoGetLetterSpacing();
  nsIDOMCSSValue* DoGetWordSpacing();
  nsIDOMCSSValue* DoGetWhiteSpace();
  nsIDOMCSSValue* DoGetWordWrap();
  nsIDOMCSSValue* DoGetHyphens();
  nsIDOMCSSValue* DoGetMozTabSize();

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
  nsIDOMCSSValue* DoGetMozTransform();
  nsIDOMCSSValue* DoGetMozTransformOrigin();
  nsIDOMCSSValue* DoGetMozPerspective();
  nsIDOMCSSValue* DoGetMozBackfaceVisibility();
  nsIDOMCSSValue* DoGetMozPerspectiveOrigin();
  nsIDOMCSSValue* DoGetMozTransformStyle();
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

