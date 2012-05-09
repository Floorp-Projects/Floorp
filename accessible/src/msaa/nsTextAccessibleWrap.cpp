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
 * Netscape Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Leventhal
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

#include "nsTextAccessibleWrap.h"
#include "ISimpleDOMText_i.c"

#include "nsCoreUtils.h"
#include "nsDocAccessible.h"
#include "Statistics.h"
#include "nsIFrame.h"
#include "nsFontMetrics.h"
#include "nsPresContext.h"
#include "nsLayoutUtils.h"

#include "gfxFont.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsTextAccessibleWrap Accessible
////////////////////////////////////////////////////////////////////////////////

nsTextAccessibleWrap::
  nsTextAccessibleWrap(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsTextAccessible(aContent, aDoc)
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

  if (IID_IUnknown == iid) {
    *ppv = static_cast<ISimpleDOMText*>(this);
  } else if (IID_ISimpleDOMText == iid) {
    statistics::ISimpleDOMUsed();
    *ppv = static_cast<ISimpleDOMText*>(this);
  } else {
    return nsAccessibleWrap::QueryInterface(iid, ppv);
  }
   
  (reinterpret_cast<IUnknown*>(*ppv))->AddRef(); 
  return S_OK;
}

STDMETHODIMP nsTextAccessibleWrap::get_domText( 
    /* [retval][out] */ BSTR __RPC_FAR *aDomText)
{
__try {
  *aDomText = NULL;

  if (IsDefunct())
    return E_FAIL;

  nsAutoString nodeValue;

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  DOMNode->GetNodeValue(nodeValue);
  if (nodeValue.IsEmpty())
    return S_FALSE;

  *aDomText = ::SysAllocStringLen(nodeValue.get(), nodeValue.Length());
  if (!*aDomText)
    return E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

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
__try {
  *aX = *aY = *aWidth = *aHeight = 0;
  nscoord x, y, width, height, docX, docY, docWidth, docHeight;
  HRESULT rv = get_unclippedSubstringBounds(aStartIndex, aEndIndex, &x, &y, &width, &height);
  if (FAILED(rv)) {
    return rv;
  }

  nsDocAccessible* docAccessible = Document();
  NS_ASSERTION(docAccessible,
               "There must always be a doc accessible, but there isn't. Crash!");

  docAccessible->GetBounds(&docX, &docY, &docWidth, &docHeight);

  nsIntRect unclippedRect(x, y, width, height);
  nsIntRect docRect(docX, docY, docWidth, docHeight);
  nsIntRect clippedRect;

  clippedRect.IntersectRect(unclippedRect, docRect);

  *aX = clippedRect.x;
  *aY = clippedRect.y;
  *aWidth = clippedRect.width;
  *aHeight = clippedRect.height;
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

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
__try {
  *aX = *aY = *aWidth = *aHeight = 0;

  if (IsDefunct())
    return E_FAIL;

  if (NS_FAILED(GetCharacterExtents(aStartIndex, aEndIndex, 
                                    aX, aY, aWidth, aHeight))) {
    return NS_ERROR_FAILURE;
  }
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return S_OK;
}


STDMETHODIMP nsTextAccessibleWrap::scrollToSubstring(
    /* [in] */ unsigned int aStartIndex,
    /* [in] */ unsigned int aEndIndex)
{
__try {
  if (IsDefunct())
    return E_FAIL;

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  nsresult rv =
    nsCoreUtils::ScrollSubstringTo(GetFrame(), DOMNode, aStartIndex,
                                   DOMNode, aEndIndex,
                                   nsIAccessibleScrollType::SCROLL_TYPE_ANYWHERE);
  if (NS_FAILED(rv))
    return E_FAIL;
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return S_OK;
}

nsIFrame* nsTextAccessibleWrap::GetPointFromOffset(nsIFrame *aContainingFrame, 
                                                   PRInt32 aOffset, 
                                                   bool aPreferNext, 
                                                   nsPoint& aOutPoint)
{
  nsIFrame *textFrame = nsnull;
  PRInt32 outOffset;
  aContainingFrame->GetChildFrameContainingOffset(aOffset, aPreferNext, &outOffset, &textFrame);
  if (!textFrame) {
    return nsnull;
  }

  textFrame->GetPointFromOffset(aOffset, &aOutPoint);
  return textFrame;
}

/*
 * Given an offset, the x, y, width, and height values are filled appropriately.
 */
nsresult nsTextAccessibleWrap::GetCharacterExtents(PRInt32 aStartOffset, PRInt32 aEndOffset,
                                                   PRInt32* aX, PRInt32* aY, 
                                                   PRInt32* aWidth, PRInt32* aHeight) 
{
  *aX = *aY = *aWidth = *aHeight = 0;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsPresContext* presContext = mDoc->PresContext();

  nsIFrame *frame = GetFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  nsPoint startPoint, endPoint;
  nsIFrame *startFrame = GetPointFromOffset(frame, aStartOffset, true, startPoint);
  nsIFrame *endFrame = GetPointFromOffset(frame, aEndOffset, false, endPoint);
  if (!startFrame || !endFrame) {
    return E_FAIL;
  }
  
  nsIntRect sum(0, 0, 0, 0);
  nsIFrame *iter = startFrame;
  nsIFrame *stopLoopFrame = endFrame->GetNextContinuation();
  for (; iter != stopLoopFrame; iter = iter->GetNextContinuation()) {
    nsIntRect rect = iter->GetScreenRectExternal();
    nscoord start = (iter == startFrame) ? presContext->AppUnitsToDevPixels(startPoint.x) : 0;
    nscoord end = (iter == endFrame) ? presContext->AppUnitsToDevPixels(endPoint.x) :
                                       rect.width;
    rect.x += start;
    rect.width = end - start;
    sum.UnionRect(sum, rect);
  }

  *aX      = sum.x;
  *aY      = sum.y;
  *aWidth  = sum.width;
  *aHeight = sum.height;

  return NS_OK;
}

STDMETHODIMP nsTextAccessibleWrap::get_fontFamily(
    /* [retval][out] */ BSTR __RPC_FAR *aFontFamily)
{
__try {
  *aFontFamily = NULL;

  nsIFrame* frame = GetFrame();
  if (!frame) {
    return E_FAIL;
  }

  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(frame, getter_AddRefs(fm));

  const nsString& name = fm->GetThebesFontGroup()->GetFontAt(0)->GetName();
  if (name.IsEmpty())
    return S_FALSE;

  *aFontFamily = ::SysAllocStringLen(name.get(), name.Length());
  if (!*aFontFamily)
    return E_OUTOFMEMORY;

} __except(FilterA11yExceptions(::GetExceptionCode(),
                                GetExceptionInformation())) { }

  return S_OK;
}
