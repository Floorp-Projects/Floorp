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
 * The Initial Developer of the Original Code is:
 *   Christopher A. Aillon <christopher@aillon.com>
 *
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
  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIDOMElement *aElement,
                  const nsAString& aPseudoElt,
                  nsIPresShell *aPresShell);

  NS_DECL_NSICSSDECLARATION

  NS_DECL_NSIDOMCSSSTYLEDECLARATION

  nsComputedDOMStyle();
  virtual ~nsComputedDOMStyle();

  static void Shutdown();

private:
  void FlushPendingReflows();
  
  nsIFrame* GetContainingBlock(nsIFrame *aFrame);

  nsresult GetStyleData(nsStyleStructID aID,
                        const nsStyleStruct*& aStyleStruct,
                        nsIFrame* aFrame=0);

  nsresult GetOffsetWidthFor(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue);

  nsresult GetAbsoluteOffset(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue);

  nsresult GetRelativeOffset(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue);

  nsresult GetStaticOffset(PRUint8 aSide,
                           nsIFrame *aFrame,
                           nsIDOMCSSValue** aValue);

  nsresult GetPaddingWidthFor(PRUint8 aSide,
                              nsIFrame *aFrame,
                              nsIDOMCSSValue** aValue);

  nscoord GetPaddingWidthCoordFor(PRUint8 aSide,
                                  nsIFrame *aFrame);

  nsresult GetBorderColorsFor(PRUint8 aSide,
                              nsIFrame *aFrame,
                              nsIDOMCSSValue** aValue);

  nsresult GetBorderStyleFor(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue);

  nsresult GetBorderRadiusFor(PRUint8 aSide,
                              nsIFrame *aFrame,
                              nsIDOMCSSValue** aValue);

  nsresult GetBorderWidthFor(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue);

  nscoord GetBorderWidthCoordFor(PRUint8 aSide,
                                 nsIFrame *aFrame);

  nsresult GetBorderColorFor(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue);

  nsresult GetMarginWidthFor(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue);

  nscoord GetMarginWidthCoordFor(PRUint8 aSide,
                                 nsIFrame *aFrame);

  nsresult GetLineHeightCoord(nsIFrame *aFrame,
                              const nsStyleText *aText,
                              nscoord& aCoord);

  /* Properties Queryable as CSSValues */

  nsresult GetAppearance(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Box properties */
  nsresult GetBoxAlign(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBoxDirection(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBoxFlex(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBoxOrdinalGroup(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBoxOrient(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBoxPack(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBoxSizing(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  nsresult GetWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetHeight(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetMaxHeight(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetMaxWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetMinHeight(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetMinWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetLeft(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetTop(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetRight(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBottom(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Font properties */
  nsresult GetColor(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetFontFamily(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetFontStyle(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetFontSize(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetFontSizeAdjust(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetFontWeight(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetFontVariant(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Background properties */
  nsresult GetBackgroundAttachment(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBackgroundColor(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBackgroundImage(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBackgroundRepeat(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBackgroundClip(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBackgroundInlinePolicy(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBackgroundOrigin(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Padding properties */
  nsresult GetPadding(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetPaddingTop(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetPaddingBottom(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetPaddingLeft(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetPaddingRight(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Table Properties */
  nsresult GetBorderCollapse(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderSpacing(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetCaptionSide(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetEmptyCells(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetTableLayout(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetVerticalAlign(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Border Properties */
  nsresult GetBorderStyle(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderTopStyle(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderBottomStyle(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderLeftStyle(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderRightStyle(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderTopWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderBottomWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderLeftWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderRightWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderTopColor(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderBottomColor(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderLeftColor(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderRightColor(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderBottomColors(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderLeftColors(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderRightColors(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderTopColors(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderRadiusBottomLeft(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderRadiusBottomRight(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderRadiusTopLeft(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetBorderRadiusTopRight(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetFloatEdge(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Margin Properties */
  nsresult GetMarginWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetMarginTopWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetMarginBottomWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetMarginLeftWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetMarginRightWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Outline Properties */
  nsresult GetOutline(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetOutlineWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetOutlineStyle(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetOutlineColor(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /*Marker Properties */
  nsresult GetMarkerOffset(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* z-index */
  nsresult GetZIndex(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* List properties */
  nsresult GetListStyleImage(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetListStylePosition(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetListStyleType(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetImageRegion(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Text Properties */
  nsresult GetLineHeight(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetTextAlign(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetTextDecoration(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetTextIndent(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetTextTransform(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetLetterSpacing(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetWordSpacing(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetWhiteSpace(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Visibility properties */
  nsresult GetOpacity(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetVisibility(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Direction properties */
  nsresult GetDirection(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetUnicodeBidi(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Display properties */
  nsresult GetBinding(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetClear(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetCssFloat(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetDisplay(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetPosition(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetClip(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetOverflow(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* User interface properties */
  nsresult GetCursor(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetUserFocus(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetUserInput(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetUserModify(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetUserSelect(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  /* Column properties */
  nsresult GetColumnCount(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetColumnWidth(nsIFrame *aFrame, nsIDOMCSSValue** aValue);
  nsresult GetColumnGap(nsIFrame *aFrame, nsIDOMCSSValue** aValue);

  nsROCSSPrimitiveValue* GetROCSSPrimitiveValue();
  nsDOMCSSValueList* GetROCSSValueList(PRBool aCommaDelimited);
  nsDOMCSSRGBColor* GetDOMCSSRGBColor(nscolor aColor);

  struct ComputedStyleMapEntry
  {
    // Create a pointer-to-member-function type.
    typedef nsresult (nsComputedDOMStyle::*ComputeMethod)(nsIFrame*, nsIDOMCSSValue**);

    nsCSSProperty mProperty;
    ComputeMethod mGetter;
  };

  static const ComputedStyleMapEntry* GetQueryablePropertyMap(PRUint32* aLength);

  CSS2PropertiesTearoff mInner;

  nsWeakPtr mPresShellWeak;
  nsCOMPtr<nsIContent> mContent;

  /*
   * When a frame is unavailable, strong reference to the
   * style context while we're accessing the data from in.
   */
  nsRefPtr<nsStyleContext> mStyleContextHolder;
  nsCOMPtr<nsIAtom> mPseudo;

  float mT2P; /* For unit conversions */
};

#endif /* nsComputedDOMStyle_h__ */

