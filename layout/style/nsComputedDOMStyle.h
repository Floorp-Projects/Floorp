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
  void SetExposeVisitedStyle(PRBool aExpose) {
    NS_ASSERTION(aExpose != mExposeVisitedStyle, "should always be changing");
    mExposeVisitedStyle = aExpose;
  }

  // nsDOMCSSDeclaration abstract methods which should never be called
  // on a nsComputedDOMStyle object, but must be defined to avoid
  // compile errors.
  virtual mozilla::css::Declaration* GetCSSDeclaration(PRBool);
  virtual nsresult SetCSSDeclaration(mozilla::css::Declaration*);
  virtual nsIDocument* DocToUpdate();
  virtual nsresult GetCSSParsingEnvironment(nsIURI**, nsIURI**, nsIPrincipal**,
                                            mozilla::css::Loader**);

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

  nsresult GetEllipseRadii(const nsStyleCorners& aRadius,
                           PRUint8 aFullCorner,
                           PRBool aIsBorder, // else outline
                           nsIDOMCSSValue** aValue);

  nsresult GetOffsetWidthFor(mozilla::css::Side aSide, nsIDOMCSSValue** aValue);

  nsresult GetAbsoluteOffset(mozilla::css::Side aSide, nsIDOMCSSValue** aValue);

  nsresult GetRelativeOffset(mozilla::css::Side aSide, nsIDOMCSSValue** aValue);

  nsresult GetStaticOffset(mozilla::css::Side aSide, nsIDOMCSSValue** aValue);

  nsresult GetPaddingWidthFor(mozilla::css::Side aSide, nsIDOMCSSValue** aValue);

  nsresult GetBorderColorsFor(mozilla::css::Side aSide, nsIDOMCSSValue** aValue);

  nsresult GetBorderStyleFor(mozilla::css::Side aSide, nsIDOMCSSValue** aValue);

  nsresult GetBorderWidthFor(mozilla::css::Side aSide, nsIDOMCSSValue** aValue);

  nsresult GetBorderColorFor(mozilla::css::Side aSide, nsIDOMCSSValue** aValue);

  nsresult GetMarginWidthFor(mozilla::css::Side aSide, nsIDOMCSSValue** aValue);

  nsresult GetSVGPaintFor(PRBool aFill, nsIDOMCSSValue** aValue);

  PRBool GetLineHeightCoord(nscoord& aCoord);

  nsresult GetCSSShadowArray(nsCSSShadowArray* aArray,
                             const nscolor& aDefaultColor,
                             PRBool aIsBoxShadow,
                             nsIDOMCSSValue** aValue);

  nsresult GetBackgroundList(PRUint8 nsStyleBackground::Layer::* aMember,
                             PRUint32 nsStyleBackground::* aCount,
                             const PRInt32 aTable[],
                             nsIDOMCSSValue** aResult);

  nsresult GetCSSGradientString(const nsStyleGradient* aGradient,
                                nsAString& aString);
  nsresult GetImageRectString(nsIURI* aURI,
                              const nsStyleSides& aCropRect,
                              nsString& aString);

  /* Properties queryable as CSSValues.
   * To avoid a name conflict with nsIDOM*CSS2Properties, these are all
   * DoGetXXX instead of GetXXX.
   */

  nsresult DoGetAppearance(nsIDOMCSSValue** aValue);

  /* Box properties */
  nsresult DoGetBoxAlign(nsIDOMCSSValue** aValue);
  nsresult DoGetBoxDirection(nsIDOMCSSValue** aValue);
  nsresult DoGetBoxFlex(nsIDOMCSSValue** aValue);
  nsresult DoGetBoxOrdinalGroup(nsIDOMCSSValue** aValue);
  nsresult DoGetBoxOrient(nsIDOMCSSValue** aValue);
  nsresult DoGetBoxPack(nsIDOMCSSValue** aValue);
  nsresult DoGetBoxSizing(nsIDOMCSSValue** aValue);

  nsresult DoGetWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetHeight(nsIDOMCSSValue** aValue);
  nsresult DoGetMaxHeight(nsIDOMCSSValue** aValue);
  nsresult DoGetMaxWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetMinHeight(nsIDOMCSSValue** aValue);
  nsresult DoGetMinWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetLeft(nsIDOMCSSValue** aValue);
  nsresult DoGetTop(nsIDOMCSSValue** aValue);
  nsresult DoGetRight(nsIDOMCSSValue** aValue);
  nsresult DoGetBottom(nsIDOMCSSValue** aValue);
  nsresult DoGetStackSizing(nsIDOMCSSValue** aValue);

  /* Font properties */
  nsresult DoGetColor(nsIDOMCSSValue** aValue);
  nsresult DoGetFontFamily(nsIDOMCSSValue** aValue);
  nsresult DoGetMozFontFeatureSettings(nsIDOMCSSValue** aValue);
  nsresult DoGetMozFontLanguageOverride(nsIDOMCSSValue** aValue);
  nsresult DoGetFontSize(nsIDOMCSSValue** aValue);
  nsresult DoGetFontSizeAdjust(nsIDOMCSSValue** aValue);
  nsresult DoGetFontStretch(nsIDOMCSSValue** aValue);
  nsresult DoGetFontStyle(nsIDOMCSSValue** aValue);
  nsresult DoGetFontWeight(nsIDOMCSSValue** aValue);
  nsresult DoGetFontVariant(nsIDOMCSSValue** aValue);

  /* Background properties */
  nsresult DoGetBackgroundAttachment(nsIDOMCSSValue** aValue);
  nsresult DoGetBackgroundColor(nsIDOMCSSValue** aValue);
  nsresult DoGetBackgroundImage(nsIDOMCSSValue** aValue);
  nsresult DoGetBackgroundPosition(nsIDOMCSSValue** aValue);
  nsresult DoGetBackgroundRepeat(nsIDOMCSSValue** aValue);
  nsresult DoGetBackgroundClip(nsIDOMCSSValue** aValue);
  nsresult DoGetBackgroundInlinePolicy(nsIDOMCSSValue** aValue);
  nsresult DoGetBackgroundOrigin(nsIDOMCSSValue** aValue);
  nsresult DoGetMozBackgroundSize(nsIDOMCSSValue** aValue);

  /* Padding properties */
  nsresult DoGetPadding(nsIDOMCSSValue** aValue);
  nsresult DoGetPaddingTop(nsIDOMCSSValue** aValue);
  nsresult DoGetPaddingBottom(nsIDOMCSSValue** aValue);
  nsresult DoGetPaddingLeft(nsIDOMCSSValue** aValue);
  nsresult DoGetPaddingRight(nsIDOMCSSValue** aValue);

  /* Table Properties */
  nsresult DoGetBorderCollapse(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderSpacing(nsIDOMCSSValue** aValue);
  nsresult DoGetCaptionSide(nsIDOMCSSValue** aValue);
  nsresult DoGetEmptyCells(nsIDOMCSSValue** aValue);
  nsresult DoGetTableLayout(nsIDOMCSSValue** aValue);
  nsresult DoGetVerticalAlign(nsIDOMCSSValue** aValue);

  /* Border Properties */
  nsresult DoGetBorderStyle(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderTopStyle(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderBottomStyle(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderLeftStyle(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderRightStyle(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderTopWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderBottomWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderLeftWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderRightWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderTopColor(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderBottomColor(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderLeftColor(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderRightColor(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderBottomColors(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderLeftColors(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderRightColors(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderTopColors(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderBottomLeftRadius(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderBottomRightRadius(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderTopLeftRadius(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderTopRightRadius(nsIDOMCSSValue** aValue);
  nsresult DoGetFloatEdge(nsIDOMCSSValue** aValue);
  nsresult DoGetBorderImage(nsIDOMCSSValue** aValue);

  /* Box Shadow */
  nsresult DoGetBoxShadow(nsIDOMCSSValue** aValue);

  /* Window Shadow */
  nsresult DoGetWindowShadow(nsIDOMCSSValue** aValue);

  /* Margin Properties */
  nsresult DoGetMarginWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetMarginTopWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetMarginBottomWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetMarginLeftWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetMarginRightWidth(nsIDOMCSSValue** aValue);

  /* Outline Properties */
  nsresult DoGetOutline(nsIDOMCSSValue** aValue);
  nsresult DoGetOutlineWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetOutlineStyle(nsIDOMCSSValue** aValue);
  nsresult DoGetOutlineColor(nsIDOMCSSValue** aValue);
  nsresult DoGetOutlineOffset(nsIDOMCSSValue** aValue);
  nsresult DoGetOutlineRadiusBottomLeft(nsIDOMCSSValue** aValue);
  nsresult DoGetOutlineRadiusBottomRight(nsIDOMCSSValue** aValue);
  nsresult DoGetOutlineRadiusTopLeft(nsIDOMCSSValue** aValue);
  nsresult DoGetOutlineRadiusTopRight(nsIDOMCSSValue** aValue);

  /* Content Properties */
  nsresult DoGetContent(nsIDOMCSSValue** aValue);
  nsresult DoGetCounterIncrement(nsIDOMCSSValue** aValue);
  nsresult DoGetCounterReset(nsIDOMCSSValue** aValue);
  nsresult DoGetMarkerOffset(nsIDOMCSSValue** aValue);

  /* Quotes Properties */
  nsresult DoGetQuotes(nsIDOMCSSValue** aValue);

  /* z-index */
  nsresult DoGetZIndex(nsIDOMCSSValue** aValue);

  /* List properties */
  nsresult DoGetListStyleImage(nsIDOMCSSValue** aValue);
  nsresult DoGetListStylePosition(nsIDOMCSSValue** aValue);
  nsresult DoGetListStyleType(nsIDOMCSSValue** aValue);
  nsresult DoGetImageRegion(nsIDOMCSSValue** aValue);

  /* Text Properties */
  nsresult DoGetLineHeight(nsIDOMCSSValue** aValue);
  nsresult DoGetTextAlign(nsIDOMCSSValue** aValue);
  nsresult DoGetTextDecoration(nsIDOMCSSValue** aValue);
  nsresult DoGetTextIndent(nsIDOMCSSValue** aValue);
  nsresult DoGetTextTransform(nsIDOMCSSValue** aValue);
  nsresult DoGetTextShadow(nsIDOMCSSValue** aValue);
  nsresult DoGetLetterSpacing(nsIDOMCSSValue** aValue);
  nsresult DoGetWordSpacing(nsIDOMCSSValue** aValue);
  nsresult DoGetWhiteSpace(nsIDOMCSSValue** aValue);
  nsresult DoGetWordWrap(nsIDOMCSSValue** aValue);
  nsresult DoGetMozTabSize(nsIDOMCSSValue** aValue);

  /* Visibility properties */
  nsresult DoGetOpacity(nsIDOMCSSValue** aValue);
  nsresult DoGetPointerEvents(nsIDOMCSSValue** aValue);
  nsresult DoGetVisibility(nsIDOMCSSValue** aValue);

  /* Direction properties */
  nsresult DoGetDirection(nsIDOMCSSValue** aValue);
  nsresult DoGetUnicodeBidi(nsIDOMCSSValue** aValue);

  /* Display properties */
  nsresult DoGetBinding(nsIDOMCSSValue** aValue);
  nsresult DoGetClear(nsIDOMCSSValue** aValue);
  nsresult DoGetCssFloat(nsIDOMCSSValue** aValue);
  nsresult DoGetDisplay(nsIDOMCSSValue** aValue);
  nsresult DoGetPosition(nsIDOMCSSValue** aValue);
  nsresult DoGetClip(nsIDOMCSSValue** aValue);
  nsresult DoGetOverflow(nsIDOMCSSValue** aValue);
  nsresult DoGetOverflowX(nsIDOMCSSValue** aValue);
  nsresult DoGetOverflowY(nsIDOMCSSValue** aValue);
  nsresult DoGetResize(nsIDOMCSSValue** aValue);
  nsresult DoGetPageBreakAfter(nsIDOMCSSValue** aValue);
  nsresult DoGetPageBreakBefore(nsIDOMCSSValue** aValue);
  nsresult DoGetMozTransform(nsIDOMCSSValue** aValue);
  nsresult DoGetMozTransformOrigin(nsIDOMCSSValue **aValue);

  /* User interface properties */
  nsresult DoGetCursor(nsIDOMCSSValue** aValue);
  nsresult DoGetForceBrokenImageIcon(nsIDOMCSSValue** aValue);
  nsresult DoGetIMEMode(nsIDOMCSSValue** aValue);
  nsresult DoGetUserFocus(nsIDOMCSSValue** aValue);
  nsresult DoGetUserInput(nsIDOMCSSValue** aValue);
  nsresult DoGetUserModify(nsIDOMCSSValue** aValue);
  nsresult DoGetUserSelect(nsIDOMCSSValue** aValue);

  /* Column properties */
  nsresult DoGetColumnCount(nsIDOMCSSValue** aValue);
  nsresult DoGetColumnWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetColumnGap(nsIDOMCSSValue** aValue);
  nsresult DoGetColumnRuleWidth(nsIDOMCSSValue** aValue);
  nsresult DoGetColumnRuleStyle(nsIDOMCSSValue** aValue);
  nsresult DoGetColumnRuleColor(nsIDOMCSSValue** aValue);

  /* CSS Transitions */
  nsresult DoGetTransitionProperty(nsIDOMCSSValue** aValue);
  nsresult DoGetTransitionDuration(nsIDOMCSSValue** aValue);
  nsresult DoGetTransitionDelay(nsIDOMCSSValue** aValue);
  nsresult DoGetTransitionTimingFunction(nsIDOMCSSValue** aValue);

  /* SVG properties */
  nsresult DoGetFill(nsIDOMCSSValue** aValue);
  nsresult DoGetStroke(nsIDOMCSSValue** aValue);
  nsresult DoGetMarkerEnd(nsIDOMCSSValue** aValue);
  nsresult DoGetMarkerMid(nsIDOMCSSValue** aValue);
  nsresult DoGetMarkerStart(nsIDOMCSSValue** aValue);
  nsresult DoGetStrokeDasharray(nsIDOMCSSValue** aValue);

  nsresult DoGetStrokeDashoffset(nsIDOMCSSValue** aValue);
  nsresult DoGetStrokeWidth(nsIDOMCSSValue** aValue);

  nsresult DoGetFillOpacity(nsIDOMCSSValue** aValue);
  nsresult DoGetFloodOpacity(nsIDOMCSSValue** aValue);
  nsresult DoGetStopOpacity(nsIDOMCSSValue** aValue);
  nsresult DoGetStrokeMiterlimit(nsIDOMCSSValue** aValue);
  nsresult DoGetStrokeOpacity(nsIDOMCSSValue** aValue);

  nsresult DoGetClipRule(nsIDOMCSSValue** aValue);
  nsresult DoGetFillRule(nsIDOMCSSValue** aValue);
  nsresult DoGetStrokeLinecap(nsIDOMCSSValue** aValue);
  nsresult DoGetStrokeLinejoin(nsIDOMCSSValue** aValue);
  nsresult DoGetTextAnchor(nsIDOMCSSValue** aValue);

  nsresult DoGetColorInterpolation(nsIDOMCSSValue** aValue);
  nsresult DoGetColorInterpolationFilters(nsIDOMCSSValue** aValue);
  nsresult DoGetDominantBaseline(nsIDOMCSSValue** aValue);
  nsresult DoGetImageRendering(nsIDOMCSSValue** aValue);
  nsresult DoGetShapeRendering(nsIDOMCSSValue** aValue);
  nsresult DoGetTextRendering(nsIDOMCSSValue** aValue);

  nsresult DoGetFloodColor(nsIDOMCSSValue** aValue);
  nsresult DoGetLightingColor(nsIDOMCSSValue** aValue);
  nsresult DoGetStopColor(nsIDOMCSSValue** aValue);

  nsresult DoGetClipPath(nsIDOMCSSValue** aValue);
  nsresult DoGetFilter(nsIDOMCSSValue** aValue);
  nsresult DoGetMask(nsIDOMCSSValue** aValue);

  nsROCSSPrimitiveValue* GetROCSSPrimitiveValue();
  nsDOMCSSValueList* GetROCSSValueList(PRBool aCommaDelimited);
  nsresult SetToRGBAColor(nsROCSSPrimitiveValue* aValue, nscolor aColor);
  nsresult SetValueToStyleImage(const nsStyleImage& aStyleImage,
                                nsROCSSPrimitiveValue* aValue);

  /**
   * A method to get a percentage base for a percentage value.  Returns PR_TRUE
   * if a percentage base value was determined, PR_FALSE otherwise.
   */
  typedef PRBool (nsComputedDOMStyle::*PercentageBaseGetter)(nscoord&);

  /**
   * Method to set aValue to aCoord.  If aCoord is a percentage value and
   * aPercentageBaseGetter is not null, aPercentageBaseGetter is called.  If it
   * returns PR_TRUE, the percentage base it outputs in its out param is used
   * to compute an nscoord value.  If the getter is null or returns PR_FALSE,
   * the percent value of aCoord is set as a percent value on aValue.  aTable,
   * if not null, is the keyword table to handle eStyleUnit_Enumerated.  When
   * calling SetAppUnits on aValue (for coord or percent values), the value
   * passed in will be NS_MAX of the value in aMinAppUnits and the NS_MIN of
   * the actual value in aCoord and the value in aMaxAppUnits.
   *
   * XXXbz should caller pass in some sort of bitfield indicating which units
   * can be expected or something?
   */
  void SetValueToCoord(nsROCSSPrimitiveValue* aValue,
                       const nsStyleCoord& aCoord,
                       PRBool aClampNegativeCalc,
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
                              PRBool aClampNegativeCalc);

  PRBool GetCBContentWidth(nscoord& aWidth);
  PRBool GetCBContentHeight(nscoord& aWidth);
  PRBool GetFrameBoundsWidthForTransform(nscoord &aWidth);
  PRBool GetFrameBoundsHeightForTransform(nscoord &aHeight);
  PRBool GetFrameBorderRectWidth(nscoord& aWidth);
  PRBool GetFrameBorderRectHeight(nscoord& aHeight);

  struct ComputedStyleMapEntry
  {
    // Create a pointer-to-member-function type.
    typedef nsresult (nsComputedDOMStyle::*ComputeMethod)(nsIDOMCSSValue**);

    nsCSSProperty mProperty;
    ComputeMethod mGetter;
    PRBool mNeedsLayoutFlush;
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

  PRPackedBool mExposeVisitedStyle;

#ifdef DEBUG
  PRBool mFlushedPendingReflows;
#endif
};

nsresult
NS_NewComputedDOMStyle(nsIDOMElement *aElement, const nsAString &aPseudoElt,
                       nsIPresShell *aPresShell,
                       nsComputedDOMStyle **aComputedStyle);

#endif /* nsComputedDOMStyle_h__ */

