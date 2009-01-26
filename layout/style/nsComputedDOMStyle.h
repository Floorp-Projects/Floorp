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

#include "nsIComputedDOMStyle.h"

#include "nsROCSSPrimitiveValue.h"
#include "nsDOMCSSDeclaration.h"
#include "nsDOMCSSRGBColor.h"
#include "nsDOMCSSValueList.h"
#include "nsCSSProps.h"

#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsAutoPtr.h"

class nsComputedDOMStyle : public nsIComputedDOMStyle
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsComputedDOMStyle)

  NS_IMETHOD Init(nsIDOMElement *aElement,
                  const nsAString& aPseudoElt,
                  nsIPresShell *aPresShell);

  NS_DECL_NSICSSDECLARATION

  NS_DECL_NSIDOMCSSSTYLEDECLARATION

  nsComputedDOMStyle();
  virtual ~nsComputedDOMStyle();

  static void Shutdown();

private:
  nsresult GetPropertyCSSValueInternal(const nsAString& aPropertyName,
                                       nsIDOMCSSValue** aReturn);
  void FlushPendingReflows();
  
#define STYLE_STRUCT(name_, checkdata_cb_, ctor_args_)                  \
  const nsStyle##name_ * GetStyle##name_() {                            \
    return mStyleContextHolder->GetStyle##name_();                      \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  nsresult GetEllipseRadii(const nsStyleCorners& aRadius,
                           PRUint8 aFullCorner,
                           nsIDOMCSSValue** aValue);

  nsresult GetOffsetWidthFor(PRUint8 aSide, nsIDOMCSSValue** aValue);

  nsresult GetAbsoluteOffset(PRUint8 aSide, nsIDOMCSSValue** aValue);

  nsresult GetRelativeOffset(PRUint8 aSide, nsIDOMCSSValue** aValue);

  nsresult GetStaticOffset(PRUint8 aSide, nsIDOMCSSValue** aValue);

  nsresult GetPaddingWidthFor(PRUint8 aSide, nsIDOMCSSValue** aValue);

  nsresult GetBorderColorsFor(PRUint8 aSide, nsIDOMCSSValue** aValue);

  nsresult GetBorderStyleFor(PRUint8 aSide, nsIDOMCSSValue** aValue);

  nsresult GetBorderWidthFor(PRUint8 aSide, nsIDOMCSSValue** aValue);

  nsresult GetBorderColorFor(PRUint8 aSide, nsIDOMCSSValue** aValue);

  nsresult GetMarginWidthFor(PRUint8 aSide, nsIDOMCSSValue** aValue);

  PRBool GetLineHeightCoord(nscoord& aCoord);

  nsresult GetCSSShadowArray(nsCSSShadowArray* aArray,
                             const nscolor& aDefaultColor,
                             PRBool aUsesSpread,
                             nsIDOMCSSValue** aValue);

  /* Properties Queryable as CSSValues */

  nsresult GetAppearance(nsIDOMCSSValue** aValue);

  /* Box properties */
  nsresult GetBoxAlign(nsIDOMCSSValue** aValue);
  nsresult GetBoxDirection(nsIDOMCSSValue** aValue);
  nsresult GetBoxFlex(nsIDOMCSSValue** aValue);
  nsresult GetBoxOrdinalGroup(nsIDOMCSSValue** aValue);
  nsresult GetBoxOrient(nsIDOMCSSValue** aValue);
  nsresult GetBoxPack(nsIDOMCSSValue** aValue);
  nsresult GetBoxSizing(nsIDOMCSSValue** aValue);

  nsresult GetWidth(nsIDOMCSSValue** aValue);
  nsresult GetHeight(nsIDOMCSSValue** aValue);
  nsresult GetMaxHeight(nsIDOMCSSValue** aValue);
  nsresult GetMaxWidth(nsIDOMCSSValue** aValue);
  nsresult GetMinHeight(nsIDOMCSSValue** aValue);
  nsresult GetMinWidth(nsIDOMCSSValue** aValue);
  nsresult GetLeft(nsIDOMCSSValue** aValue);
  nsresult GetTop(nsIDOMCSSValue** aValue);
  nsresult GetRight(nsIDOMCSSValue** aValue);
  nsresult GetBottom(nsIDOMCSSValue** aValue);
  nsresult GetStackSizing(nsIDOMCSSValue** aValue);

  /* Font properties */
  nsresult GetColor(nsIDOMCSSValue** aValue);
  nsresult GetFontFamily(nsIDOMCSSValue** aValue);
  nsresult GetFontStyle(nsIDOMCSSValue** aValue);
  nsresult GetFontSize(nsIDOMCSSValue** aValue);
  nsresult GetFontSizeAdjust(nsIDOMCSSValue** aValue);
  nsresult GetFontWeight(nsIDOMCSSValue** aValue);
  nsresult GetFontVariant(nsIDOMCSSValue** aValue);

  /* Background properties */
  nsresult GetBackgroundAttachment(nsIDOMCSSValue** aValue);
  nsresult GetBackgroundColor(nsIDOMCSSValue** aValue);
  nsresult GetBackgroundImage(nsIDOMCSSValue** aValue);
  nsresult GetBackgroundPosition(nsIDOMCSSValue** aValue);
  nsresult GetBackgroundRepeat(nsIDOMCSSValue** aValue);
  nsresult GetBackgroundClip(nsIDOMCSSValue** aValue);
  nsresult GetBackgroundInlinePolicy(nsIDOMCSSValue** aValue);
  nsresult GetBackgroundOrigin(nsIDOMCSSValue** aValue);

  /* Padding properties */
  nsresult GetPadding(nsIDOMCSSValue** aValue);
  nsresult GetPaddingTop(nsIDOMCSSValue** aValue);
  nsresult GetPaddingBottom(nsIDOMCSSValue** aValue);
  nsresult GetPaddingLeft(nsIDOMCSSValue** aValue);
  nsresult GetPaddingRight(nsIDOMCSSValue** aValue);

  /* Table Properties */
  nsresult GetBorderCollapse(nsIDOMCSSValue** aValue);
  nsresult GetBorderSpacing(nsIDOMCSSValue** aValue);
  nsresult GetCaptionSide(nsIDOMCSSValue** aValue);
  nsresult GetEmptyCells(nsIDOMCSSValue** aValue);
  nsresult GetTableLayout(nsIDOMCSSValue** aValue);
  nsresult GetVerticalAlign(nsIDOMCSSValue** aValue);

  /* Border Properties */
  nsresult GetBorderStyle(nsIDOMCSSValue** aValue);
  nsresult GetBorderWidth(nsIDOMCSSValue** aValue);
  nsresult GetBorderTopStyle(nsIDOMCSSValue** aValue);
  nsresult GetBorderBottomStyle(nsIDOMCSSValue** aValue);
  nsresult GetBorderLeftStyle(nsIDOMCSSValue** aValue);
  nsresult GetBorderRightStyle(nsIDOMCSSValue** aValue);
  nsresult GetBorderTopWidth(nsIDOMCSSValue** aValue);
  nsresult GetBorderBottomWidth(nsIDOMCSSValue** aValue);
  nsresult GetBorderLeftWidth(nsIDOMCSSValue** aValue);
  nsresult GetBorderRightWidth(nsIDOMCSSValue** aValue);
  nsresult GetBorderTopColor(nsIDOMCSSValue** aValue);
  nsresult GetBorderBottomColor(nsIDOMCSSValue** aValue);
  nsresult GetBorderLeftColor(nsIDOMCSSValue** aValue);
  nsresult GetBorderRightColor(nsIDOMCSSValue** aValue);
  nsresult GetBorderBottomColors(nsIDOMCSSValue** aValue);
  nsresult GetBorderLeftColors(nsIDOMCSSValue** aValue);
  nsresult GetBorderRightColors(nsIDOMCSSValue** aValue);
  nsresult GetBorderTopColors(nsIDOMCSSValue** aValue);
  nsresult GetBorderRadiusBottomLeft(nsIDOMCSSValue** aValue);
  nsresult GetBorderRadiusBottomRight(nsIDOMCSSValue** aValue);
  nsresult GetBorderRadiusTopLeft(nsIDOMCSSValue** aValue);
  nsresult GetBorderRadiusTopRight(nsIDOMCSSValue** aValue);
  nsresult GetFloatEdge(nsIDOMCSSValue** aValue);
  nsresult GetBorderImage(nsIDOMCSSValue** aValue);

  /* Box Shadow */
  nsresult GetBoxShadow(nsIDOMCSSValue** aValue);

  /* Window Shadow */
  nsresult GetWindowShadow(nsIDOMCSSValue** aValue);

  /* Margin Properties */
  nsresult GetMarginWidth(nsIDOMCSSValue** aValue);
  nsresult GetMarginTopWidth(nsIDOMCSSValue** aValue);
  nsresult GetMarginBottomWidth(nsIDOMCSSValue** aValue);
  nsresult GetMarginLeftWidth(nsIDOMCSSValue** aValue);
  nsresult GetMarginRightWidth(nsIDOMCSSValue** aValue);

  /* Outline Properties */
  nsresult GetOutline(nsIDOMCSSValue** aValue);
  nsresult GetOutlineWidth(nsIDOMCSSValue** aValue);
  nsresult GetOutlineStyle(nsIDOMCSSValue** aValue);
  nsresult GetOutlineColor(nsIDOMCSSValue** aValue);
  nsresult GetOutlineOffset(nsIDOMCSSValue** aValue);
  nsresult GetOutlineRadiusBottomLeft(nsIDOMCSSValue** aValue);
  nsresult GetOutlineRadiusBottomRight(nsIDOMCSSValue** aValue);
  nsresult GetOutlineRadiusTopLeft(nsIDOMCSSValue** aValue);
  nsresult GetOutlineRadiusTopRight(nsIDOMCSSValue** aValue);

  /* Content Properties */
  nsresult GetContent(nsIDOMCSSValue** aValue);
  nsresult GetCounterIncrement(nsIDOMCSSValue** aValue);
  nsresult GetCounterReset(nsIDOMCSSValue** aValue);
  nsresult GetMarkerOffset(nsIDOMCSSValue** aValue);

  /* Quotes Properties */
  nsresult GetQuotes(nsIDOMCSSValue** aValue);

  /* z-index */
  nsresult GetZIndex(nsIDOMCSSValue** aValue);

  /* List properties */
  nsresult GetListStyleImage(nsIDOMCSSValue** aValue);
  nsresult GetListStylePosition(nsIDOMCSSValue** aValue);
  nsresult GetListStyleType(nsIDOMCSSValue** aValue);
  nsresult GetImageRegion(nsIDOMCSSValue** aValue);

  /* Text Properties */
  nsresult GetLineHeight(nsIDOMCSSValue** aValue);
  nsresult GetTextAlign(nsIDOMCSSValue** aValue);
  nsresult GetTextDecoration(nsIDOMCSSValue** aValue);
  nsresult GetTextIndent(nsIDOMCSSValue** aValue);
  nsresult GetTextTransform(nsIDOMCSSValue** aValue);
  nsresult GetTextShadow(nsIDOMCSSValue** aValue);
  nsresult GetLetterSpacing(nsIDOMCSSValue** aValue);
  nsresult GetWordSpacing(nsIDOMCSSValue** aValue);
  nsresult GetWhiteSpace(nsIDOMCSSValue** aValue);
  nsresult GetWordWrap(nsIDOMCSSValue** aValue);

  /* Visibility properties */
  nsresult GetOpacity(nsIDOMCSSValue** aValue);
  nsresult GetVisibility(nsIDOMCSSValue** aValue);

  /* Direction properties */
  nsresult GetDirection(nsIDOMCSSValue** aValue);
  nsresult GetUnicodeBidi(nsIDOMCSSValue** aValue);

  /* Display properties */
  nsresult GetBinding(nsIDOMCSSValue** aValue);
  nsresult GetClear(nsIDOMCSSValue** aValue);
  nsresult GetCssFloat(nsIDOMCSSValue** aValue);
  nsresult GetDisplay(nsIDOMCSSValue** aValue);
  nsresult GetPosition(nsIDOMCSSValue** aValue);
  nsresult GetClip(nsIDOMCSSValue** aValue);
  nsresult GetOverflow(nsIDOMCSSValue** aValue);
  nsresult GetOverflowX(nsIDOMCSSValue** aValue);
  nsresult GetOverflowY(nsIDOMCSSValue** aValue);
  nsresult GetPageBreakAfter(nsIDOMCSSValue** aValue);
  nsresult GetPageBreakBefore(nsIDOMCSSValue** aValue);
  nsresult GetMozTransform(nsIDOMCSSValue** aValue);
  nsresult GetMozTransformOrigin(nsIDOMCSSValue **aValue);

  /* User interface properties */
  nsresult GetCursor(nsIDOMCSSValue** aValue);
  nsresult GetForceBrokenImageIcon(nsIDOMCSSValue** aValue);
  nsresult GetIMEMode(nsIDOMCSSValue** aValue);
  nsresult GetUserFocus(nsIDOMCSSValue** aValue);
  nsresult GetUserInput(nsIDOMCSSValue** aValue);
  nsresult GetUserModify(nsIDOMCSSValue** aValue);
  nsresult GetUserSelect(nsIDOMCSSValue** aValue);

  /* Column properties */
  nsresult GetColumnCount(nsIDOMCSSValue** aValue);
  nsresult GetColumnWidth(nsIDOMCSSValue** aValue);
  nsresult GetColumnGap(nsIDOMCSSValue** aValue);
  nsresult GetColumnRuleWidth(nsIDOMCSSValue** aValue);
  nsresult GetColumnRuleStyle(nsIDOMCSSValue** aValue);
  nsresult GetColumnRuleColor(nsIDOMCSSValue** aValue);

#ifdef MOZ_SVG
  /* SVG properties */
  nsresult GetSVGPaintFor(PRBool aFill, nsIDOMCSSValue** aValue);

  nsresult GetFill(nsIDOMCSSValue** aValue);
  nsresult GetStroke(nsIDOMCSSValue** aValue);
  nsresult GetMarkerEnd(nsIDOMCSSValue** aValue);
  nsresult GetMarkerMid(nsIDOMCSSValue** aValue);
  nsresult GetMarkerStart(nsIDOMCSSValue** aValue);
  nsresult GetStrokeDasharray(nsIDOMCSSValue** aValue);

  nsresult GetStrokeDashoffset(nsIDOMCSSValue** aValue);
  nsresult GetStrokeWidth(nsIDOMCSSValue** aValue);

  nsresult GetFillOpacity(nsIDOMCSSValue** aValue);
  nsresult GetFloodOpacity(nsIDOMCSSValue** aValue);
  nsresult GetStopOpacity(nsIDOMCSSValue** aValue);
  nsresult GetStrokeMiterlimit(nsIDOMCSSValue** aValue);
  nsresult GetStrokeOpacity(nsIDOMCSSValue** aValue);

  nsresult GetClipRule(nsIDOMCSSValue** aValue);
  nsresult GetFillRule(nsIDOMCSSValue** aValue);
  nsresult GetStrokeLinecap(nsIDOMCSSValue** aValue);
  nsresult GetStrokeLinejoin(nsIDOMCSSValue** aValue);
  nsresult GetTextAnchor(nsIDOMCSSValue** aValue);

  nsresult GetColorInterpolation(nsIDOMCSSValue** aValue);
  nsresult GetColorInterpolationFilters(nsIDOMCSSValue** aValue);
  nsresult GetDominantBaseline(nsIDOMCSSValue** aValue);
  nsresult GetPointerEvents(nsIDOMCSSValue** aValue);
  nsresult GetShapeRendering(nsIDOMCSSValue** aValue);
  nsresult GetTextRendering(nsIDOMCSSValue** aValue);

  nsresult GetFloodColor(nsIDOMCSSValue** aValue);
  nsresult GetLightingColor(nsIDOMCSSValue** aValue);
  nsresult GetStopColor(nsIDOMCSSValue** aValue);

  nsresult GetClipPath(nsIDOMCSSValue** aValue);
  nsresult GetFilter(nsIDOMCSSValue** aValue);
  nsresult GetMask(nsIDOMCSSValue** aValue);
#endif // MOZ_SVG

  nsROCSSPrimitiveValue* GetROCSSPrimitiveValue();
  nsDOMCSSValueList* GetROCSSValueList(PRBool aCommaDelimited);
  nsresult SetToRGBAColor(nsROCSSPrimitiveValue* aValue, nscolor aColor);
  
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
   * passed in will be PR_MAX of the value in aMinAppUnits and the PR_MIN of
   * the actual value in aCoord and the value in aMaxAppUnits.
   *
   * XXXbz should caller pass in some sort of bitfield indicating which units
   * can be expected or something?
   */
  void SetValueToCoord(nsROCSSPrimitiveValue* aValue,
                       const nsStyleCoord& aCoord,
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
                              nscoord aDefaultValue);

  PRBool GetCBContentWidth(nscoord& aWidth);
  PRBool GetCBContentHeight(nscoord& aWidth);
  PRBool GetFrameBoundsWidthForTransform(nscoord &aWidth);
  PRBool GetFrameBoundsHeightForTransform(nscoord &aHeight);
  PRBool GetFrameBorderRectWidth(nscoord& aWidth);

  struct ComputedStyleMapEntry
  {
    // Create a pointer-to-member-function type.
    typedef nsresult (nsComputedDOMStyle::*ComputeMethod)(nsIDOMCSSValue**);

    nsCSSProperty mProperty;
    ComputeMethod mGetter;
  };

  static const ComputedStyleMapEntry* GetQueryablePropertyMap(PRUint32* aLength);

  CSS2PropertiesTearoff mInner;

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

  PRInt32 mAppUnitsPerInch; /* For unit conversions */
};

#endif /* nsComputedDOMStyle_h__ */

