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
#include "nsDOMCSSRGBColor.h"

#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"

class nsComputedDOMStyle : public nsIComputedDOMStyle
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIDOMElement *aElement,
                  const nsAString& aPseudoElt,
                  nsIPresShell *aPresShell);

  NS_DECL_NSIDOMCSSSTYLEDECLARATION

  nsComputedDOMStyle();
  virtual ~nsComputedDOMStyle();

private:
  nsIFrame* GetContainingBlock(nsIFrame *aFrame);

  nsresult GetStyleData(nsStyleStructID aID,
                        const nsStyleStruct*& aStyleStruct,
                        nsIFrame* aFrame=0);

  nsresult GetOffsetWidthFor(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue);

  nsresult GetAbsoluteOffset(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue);

  nsresult GetRelativeOffset(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue);

  nsresult GetStaticOffset(PRUint8 aSide,
                           nsIFrame *aFrame,
                           nsIDOMCSSPrimitiveValue*& aValue);

  nsresult GetPaddingWidthFor(PRUint8 aSide,
                              nsIFrame *aFrame,
                              nsIDOMCSSPrimitiveValue*& aValue);

  nscoord GetPaddingWidthCoordFor(PRUint8 aSide,
                                  nsIFrame *aFrame);

  nsresult GetBorderStyleFor(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue);

  nsresult GetBorderWidthFor(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue);

  nscoord GetBorderWidthCoordFor(PRUint8 aSide,
                                 nsIFrame *aFrame);

  nsresult GetBorderColorFor(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue);

  nsresult GetMarginWidthFor(PRUint8 aSide,
                             nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue);

  nscoord GetMarginWidthCoordFor(PRUint8 aSide,
                                 nsIFrame *aFrame);

  nsresult GetLineHeightCoord(nsIFrame *aFrame,
                              const nsStyleText *aText,
                              nscoord& aCoord);

  /* Properties Queryable as CSSValues */
  nsresult GetWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetHeight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetMaxHeight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetMaxWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetMinHeight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetMinWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetLeft(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetTop(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetRight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBottom(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* Font properties */
  nsresult GetColor(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetFontFamily(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetFontStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetFontSize(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetFontSizeAdjust(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetFontWeight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetFontVariant(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* Background properties */
  nsresult GetBackgroundAttachment(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBackgroundColor(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBackgroundImage(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBackgroundRepeat(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* Padding properties */
  nsresult GetPadding(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetPaddingTop(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetPaddingBottom(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetPaddingLeft(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetPaddingRight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* Table Properties */
  nsresult GetBorderCollapse(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderSpacing(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetCaptionSide(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetEmptyCells(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetTableLayout(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetVerticalAlign(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* Border Properties */
  nsresult GetBorderStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderTopStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderBottomStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderLeftStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderRightStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderTopWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderBottomWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderLeftWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderRightWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderTopColor(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderBottomColor(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderLeftColor(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderRightColor(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* Margin Properties */
  nsresult GetMarginWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetMarginTopWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetMarginBottomWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetMarginLeftWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetMarginRightWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* Outline Properties */
  nsresult GetOutline(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetOutlineWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetOutlineStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetOutlineColor(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /*Marker Properties */
  nsresult GetMarkerOffset(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* z-index */
  nsresult GetZIndex(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* List properties */
  nsresult GetListStyleImage(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetListStylePosition(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetListStyleType(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* Text Properties */
  nsresult GetLineHeight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetTextAlign(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetTextDecoration(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetTextIndent(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetTextTransform(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetLetterSpacing(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetWordSpacing(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetWhiteSpace(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* Visibility properties */
  nsresult GetOpacity(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetVisibility(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* Direction properties */
  nsresult GetDirection(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetUnicodeBidi(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* Display properties */
  nsresult GetBinding(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetClear(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetCssFloat(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetDisplay(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetPosition(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetClip(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetOverflow(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  /* User interface properties */
  nsresult GetCursor(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  nsROCSSPrimitiveValue* GetROCSSPrimitiveValue();
  nsDOMCSSRGBColor* GetDOMCSSRGBColor(nscolor aColor);

  nsWeakPtr mPresShellWeak;
  nsCOMPtr<nsIContent> mContent;

  /*
   * When a frame is unavailable, strong reference to the
   * style context while we're accessing the data from in.
   */
  nsCOMPtr<nsIStyleContext> mStyleContextHolder;
  nsCOMPtr<nsIAtom> mPseudo;

  float mT2P; /* For unit conversions */
};

#endif /* nsComputedDOMStyle_h__ */

