/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextLeafAccessibleWrap.h"
#include "ISimpleDOMText_i.c"

#include "nsCoreUtils.h"
#include "DocAccessible.h"
#include "Statistics.h"
#include "nsIFrame.h"
#include "nsFontMetrics.h"
#include "nsPresContext.h"
#include "nsLayoutUtils.h"

#include "gfxFont.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// TextLeafAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

TextLeafAccessibleWrap::
  TextLeafAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
  TextLeafAccessible(aContent, aDoc)
{
}

STDMETHODIMP_(ULONG)
TextLeafAccessibleWrap::AddRef()
{
  return nsAccessNode::AddRef();
}

STDMETHODIMP_(ULONG)
TextLeafAccessibleWrap::Release()
{
  return nsAccessNode::Release();
}

STDMETHODIMP
TextLeafAccessibleWrap::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = nullptr;

  if (IID_IUnknown == iid) {
    *ppv = static_cast<ISimpleDOMText*>(this);
  } else if (IID_ISimpleDOMText == iid) {
    statistics::ISimpleDOMUsed();
    *ppv = static_cast<ISimpleDOMText*>(this);
  } else {
    return AccessibleWrap::QueryInterface(iid, ppv);
  }
   
  (reinterpret_cast<IUnknown*>(*ppv))->AddRef(); 
  return S_OK;
}

STDMETHODIMP
TextLeafAccessibleWrap::get_domText( 
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

STDMETHODIMP
TextLeafAccessibleWrap::get_clippedSubstringBounds( 
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

  DocAccessible* docAccessible = Document();
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

STDMETHODIMP
TextLeafAccessibleWrap::get_unclippedSubstringBounds( 
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

STDMETHODIMP
TextLeafAccessibleWrap::scrollToSubstring(
    /* [in] */ unsigned int aStartIndex,
    /* [in] */ unsigned int aEndIndex)
{
__try {
  if (IsDefunct())
    return E_FAIL;

  nsRefPtr<nsRange> range = new nsRange();
  if (NS_FAILED(range->SetStart(mContent, aStartIndex)))
      return E_FAIL;

  if (NS_FAILED(range->SetEnd(mContent, aEndIndex)))
  return E_FAIL;

  nsresult rv =
    nsCoreUtils::ScrollSubstringTo(GetFrame(), range,
                                   nsIAccessibleScrollType::SCROLL_TYPE_ANYWHERE);
  if (NS_FAILED(rv))
    return E_FAIL;
} __except(FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return S_OK;
}

nsIFrame*
TextLeafAccessibleWrap::GetPointFromOffset(nsIFrame* aContainingFrame, 
                                           PRInt32 aOffset, 
                                           bool aPreferNext, 
                                           nsPoint& aOutPoint)
{
  nsIFrame *textFrame = nullptr;
  PRInt32 outOffset;
  aContainingFrame->GetChildFrameContainingOffset(aOffset, aPreferNext, &outOffset, &textFrame);
  if (!textFrame) {
    return nullptr;
  }

  textFrame->GetPointFromOffset(aOffset, &aOutPoint);
  return textFrame;
}

/*
 * Given an offset, the x, y, width, and height values are filled appropriately.
 */
nsresult
TextLeafAccessibleWrap::GetCharacterExtents(PRInt32 aStartOffset,
                                            PRInt32 aEndOffset,
                                            PRInt32* aX,
                                            PRInt32* aY,
                                            PRInt32* aWidth,
                                            PRInt32* aHeight)
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

STDMETHODIMP
TextLeafAccessibleWrap::get_fontFamily(
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
