/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Corp.
 * Portions created by Netscape Corp.are Copyright (C) 2003 Netscape
 * Corp. All Rights Reserved.
 *
 * Original Author: Aaron Leventhal
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// NOTE: alphabetically ordered
#include "nsTextAccessibleWrap.h"
#include "ISimpleDOMText_i.c"
#include "nsContentCID.h"
#include "nsIAccessibleDocument.h"
#include "nsIDOMRange.h"
#include "nsIFontMetrics.h"
#include "nsIFrame.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIRenderingContext.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"

static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

// --------------------------------------------------------
// nsTextAccessibleWrap Accessible
// --------------------------------------------------------

nsTextAccessibleWrap::nsTextAccessibleWrap(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsTextAccessible(aDOMNode, aShell)
{ 
}

STDMETHODIMP_(ULONG) nsTextAccessibleWrap::AddRef()
{
  return nsAccessNode::AddRef();
}

STDMETHODIMP_(ULONG) nsTextAccessibleWrap::Release()
{
  return nsAccessNode::Release();
}

STDMETHODIMP nsTextAccessibleWrap::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = nsnull;

  if (IID_IUnknown == iid || IID_ISimpleDOMText == iid)
    *ppv = NS_STATIC_CAST(ISimpleDOMText*, this);

  if (nsnull == *ppv)
    return nsAccessibleWrap::QueryInterface(iid, ppv);
   
  (NS_REINTERPRET_CAST(IUnknown*, *ppv))->AddRef(); 
  return S_OK;
}

STDMETHODIMP nsTextAccessibleWrap::get_domText( 
    /* [retval][out] */ BSTR __RPC_FAR *aDomText)
{
  if (!mDOMNode) {
    return E_FAIL; // Node already shut down
  }
  nsAutoString nodeValue;

  mDOMNode->GetNodeValue(nodeValue);
  *aDomText = ::SysAllocString(nodeValue.get());

  return S_OK;
}

STDMETHODIMP nsTextAccessibleWrap::get_clippedSubstringBounds( 
    /* [in] */ unsigned int aStartIndex,
    /* [in] */ unsigned int aEndIndex,
    /* [out] */ int __RPC_FAR *aX,
    /* [out] */ int __RPC_FAR *aY,
    /* [out] */ int __RPC_FAR *aWidth,
    /* [out] */ int __RPC_FAR *aHeight)
{
  nscoord x, y, width, height, docX, docY, docWidth, docHeight;
  HRESULT rv = get_unclippedSubstringBounds(aStartIndex, aEndIndex, &x, &y, &width, &height);
  if (FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIAccessibleDocument> docAccessible(GetDocAccessible());
  nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(docAccessible));
  NS_ASSERTION(accessible, "There must always be a doc accessible, but there isn't");

  accessible->GetBounds(&docX, &docY, &docWidth, &docHeight);

  nsRect unclippedRect(x, y, width, height);
  nsRect docRect(docX, docY, docWidth, docHeight);
  nsRect clippedRect;

  clippedRect.IntersectRect(unclippedRect, docRect);

  *aX = clippedRect.x;
  *aY = clippedRect.y;
  *aWidth = clippedRect.width;
  *aHeight = clippedRect.height;

  return S_OK;
}

STDMETHODIMP nsTextAccessibleWrap::get_unclippedSubstringBounds( 
    /* [in] */ unsigned int aStartIndex,
    /* [in] */ unsigned int aEndIndex,
    /* [out] */ int __RPC_FAR *aX,
    /* [out] */ int __RPC_FAR *aY,
    /* [out] */ int __RPC_FAR *aWidth,
    /* [out] */ int __RPC_FAR *aHeight)
{
  if (!mDOMNode) {
    return E_FAIL; // Node already shut down
  }
   if (NS_FAILED(GetCharacterExtents(aStartIndex, aEndIndex, 
                                    aX, aY, aWidth, aHeight))) {
    return NS_ERROR_FAILURE;
  }

  // Add offsets for entire accessible
  PRInt32 nodeX, nodeY, nodeWidth, nodeHeight;
  GetBounds(&nodeX, &nodeY, &nodeWidth, &nodeHeight);
  *aX += nodeX;
  *aY += nodeY;

  return S_OK;
}


STDMETHODIMP nsTextAccessibleWrap::scrollToSubstring( 
    /* [in] */ unsigned int aStartIndex,
    /* [in] */ unsigned int aEndIndex)
{
  nsCOMPtr<nsIPresShell> presShell(GetPresShell());
  nsIFrame *frame = GetFrame();

  if (!frame || !presShell) {
    return E_FAIL;  // This accessible has been shut down
  }

  nsPresContext *presContext = presShell->GetPresContext();
  nsCOMPtr<nsIDOMRange> scrollToRange = do_CreateInstance(kRangeCID);
  nsCOMPtr<nsISelectionController> selCon;
  frame->GetSelectionController(presContext, getter_AddRefs(selCon));
  if (!presContext || !scrollToRange || !selCon) {
    return E_FAIL;
  }

  scrollToRange->SetStart(mDOMNode, aStartIndex);
  scrollToRange->SetEnd(mDOMNode, aEndIndex);
  nsCOMPtr<nsISelection> domSel;
  selCon->GetSelection(nsISelectionController::SELECTION_ACCESSIBILITY, 
                       getter_AddRefs(domSel));
  if (domSel) {
    domSel->RemoveAllRanges();
    domSel->AddRange(scrollToRange);

    selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_ACCESSIBILITY, 
      nsISelectionController::SELECTION_ANCHOR_REGION, PR_TRUE);

    domSel->CollapseToStart();
  }

  return S_OK;
}

nsIFrame* nsTextAccessibleWrap::GetPointFromOffset(nsIFrame *aContainingFrame, 
                                                nsPresContext *aPresContext,
                                                nsIRenderingContext *aRendContext,
                                                PRInt32 aOffset, 
                                                nsPoint& aOutPoint)
{
  nsIFrame *textFrame = nsnull;
  PRInt32 outOffset;
  aContainingFrame->GetChildFrameContainingOffset(aOffset, PR_FALSE, &outOffset, &textFrame);
  if (!textFrame) {
    return nsnull;
  }

  textFrame->GetPointFromOffset(aPresContext, aRendContext, aOffset, &aOutPoint);
  return textFrame;
}

/*
 * Given an offset, the x, y, width, and height values are filled appropriately.
 */
nsresult nsTextAccessibleWrap::GetCharacterExtents(PRInt32 aStartOffset, PRInt32 aEndOffset,
                                                   PRInt32* aX, PRInt32* aY, 
                                                   PRInt32* aWidth, PRInt32* aHeight) 
{
  nsCOMPtr<nsIPresShell> presShell(GetPresShell());
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsPresContext *presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);
  float t2p = presContext->TwipsToPixels();

  nsIFrame *frame = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  presShell->GetPrimaryFrameFor(content, &frame);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  nsIViewManager* viewManager = presShell->GetViewManager();
  NS_ASSERTION(viewManager, "No view manager for pres shell");

  nsCOMPtr<nsIWidget> widget;
  viewManager->GetWidget(getter_AddRefs(widget));

  nsIRenderingContext *rendContext;
  rendContext = widget->GetRenderingContext();

  nsPoint startPoint, endPoint;
  nsIFrame *startFrame = GetPointFromOffset(frame, presContext, rendContext, 
                                            aStartOffset, startPoint);
  nsIFrame *endFrame = GetPointFromOffset(frame, presContext, rendContext, 
                                          aEndOffset, endPoint);
  if (!startFrame || !endFrame) {
    return E_FAIL;
  }

  nsRect startRect = startFrame->GetRect();
  nsRect endRect = endFrame->GetRect();

  *aX      = NSTwipsToIntPixels(startPoint.x + startRect.x, t2p);
  *aY      = NSTwipsToIntPixels(startPoint.y, t2p);
  *aWidth  = NSTwipsToIntPixels(endPoint.x + endRect.x,     t2p) - *aX;
  *aHeight = NSTwipsToIntPixels(endRect.y + endRect.height -
                                startPoint.y - startRect.y , t2p);

  return NS_OK;
}

STDMETHODIMP nsTextAccessibleWrap::get_fontFamily(
    /* [retval][out] */ BSTR __RPC_FAR *aFontFamily)
{
  nsIFrame *frame = GetFrame();
  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!frame || !presShell) {
    return E_FAIL;
  }

  nsCOMPtr<nsIRenderingContext> rc;
  presShell->CreateRenderingContext(frame, getter_AddRefs(rc));
  if (!rc) {
    return E_FAIL;
  }

  const nsStyleFont *font = frame->GetStyleFont();

  const nsStyleVisibility *visibility = frame->GetStyleVisibility();

  if (NS_FAILED(rc->SetFont(font->mFont, visibility->mLangGroup))) {
    return E_FAIL;
  }

  nsCOMPtr<nsIDeviceContext> deviceContext;
  rc->GetDeviceContext(*getter_AddRefs(deviceContext));
  if (!deviceContext) {
    return E_FAIL;
  }

  nsIFontMetrics *fm;
  rc->GetFontMetrics(fm);
  if (!fm) {
    return E_FAIL;
  }

  const nsFont *actualFont = nsnull;
  fm->GetFont(actualFont);
  if (!actualFont) {
    return E_FAIL;
  }

  nsAutoString fontFamily;
  deviceContext->FirstExistingFont(*actualFont, fontFamily);
  
  *aFontFamily = ::SysAllocString(fontFamily.get());
  return S_OK;
}
