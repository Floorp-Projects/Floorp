/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdnTextAccessible.h"

#include "ISimpleDOMText_i.c"

#include "nsCoreUtils.h"
#include "DocAccessible.h"

#include "nsIFrame.h"
#include "nsFontMetrics.h"
#include "nsPresContext.h"
#include "nsLayoutUtils.h"
#include "gfxFont.h"
#include "nsIAccessibleTypes.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// sdnTextAccessible
////////////////////////////////////////////////////////////////////////////////

IMPL_IUNKNOWN_QUERY_HEAD(sdnTextAccessible)
  IMPL_IUNKNOWN_QUERY_IFACE(ISimpleDOMText)
IMPL_IUNKNOWN_QUERY_TAIL_AGGREGATED(mAccessible)

STDMETHODIMP
sdnTextAccessible::get_domText(BSTR __RPC_FAR* aText)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aText)
    return E_INVALIDARG;
  *aText = nullptr;

  if (mAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString nodeValue;

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mAccessible->GetContent()));
  DOMNode->GetNodeValue(nodeValue);
  if (nodeValue.IsEmpty())
    return S_FALSE;

  *aText = ::SysAllocStringLen(nodeValue.get(), nodeValue.Length());
  return *aText ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnTextAccessible::get_clippedSubstringBounds(unsigned int aStartIndex,
                                              unsigned int aEndIndex,
                                              int __RPC_FAR* aX,
                                              int __RPC_FAR* aY,
                                              int __RPC_FAR* aWidth,
                                              int __RPC_FAR* aHeight)
{
  A11Y_TRYBLOCK_BEGIN

  nscoord x = 0, y = 0, width = 0, height = 0;
  HRESULT rv = get_unclippedSubstringBounds(aStartIndex, aEndIndex,
                                            &x, &y, &width, &height);
  if (FAILED(rv))
    return rv;

  DocAccessible* document = mAccessible->Document();
  NS_ASSERTION(document,
               "There must always be a doc accessible, but there isn't. Crash!");

  nscoord docX = 0, docY = 0, docWidth = 0, docHeight = 0;
  document->GetBounds(&docX, &docY, &docWidth, &docHeight);

  nsIntRect unclippedRect(x, y, width, height);
  nsIntRect docRect(docX, docY, docWidth, docHeight);

  nsIntRect clippedRect;
  clippedRect.IntersectRect(unclippedRect, docRect);

  *aX = clippedRect.x;
  *aY = clippedRect.y;
  *aWidth = clippedRect.width;
  *aHeight = clippedRect.height;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnTextAccessible::get_unclippedSubstringBounds(unsigned int aStartIndex,
                                                unsigned int aEndIndex,
                                                int __RPC_FAR* aX,
                                                int __RPC_FAR* aY,
                                                int __RPC_FAR* aWidth,
                                                int __RPC_FAR* aHeight)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aX || !aY || !aWidth || !aHeight)
    return E_INVALIDARG;
  *aX = *aY = *aWidth = *aHeight = 0;

  if (mAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsIFrame *frame = mAccessible->GetFrame();
  NS_ENSURE_TRUE(frame, E_FAIL);

  nsPoint startPoint, endPoint;
  nsIFrame* startFrame = GetPointFromOffset(frame, aStartIndex, true, startPoint);
  nsIFrame* endFrame = GetPointFromOffset(frame, aEndIndex, false, endPoint);
  if (!startFrame || !endFrame)
    return E_FAIL;

  nsRect sum;
  nsIFrame* iter = startFrame;
  nsIFrame* stopLoopFrame = endFrame->GetNextContinuation();
  for (; iter != stopLoopFrame; iter = iter->GetNextContinuation()) {
    nsRect rect = iter->GetScreenRectInAppUnits();
    nscoord start = (iter == startFrame) ? startPoint.x : 0;
    nscoord end = (iter == endFrame) ? endPoint.x : rect.width;
    rect.x += start;
    rect.width = end - start;
    sum.UnionRect(sum, rect);
  }

  nsPresContext* presContext = mAccessible->Document()->PresContext();
  *aX = presContext->AppUnitsToDevPixels(sum.x);
  *aY = presContext->AppUnitsToDevPixels(sum.y);
  *aWidth = presContext->AppUnitsToDevPixels(sum.width);
  *aHeight = presContext->AppUnitsToDevPixels(sum.height);

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnTextAccessible::scrollToSubstring(unsigned int aStartIndex,
                                     unsigned int aEndIndex)
{
  A11Y_TRYBLOCK_BEGIN

  if (mAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsRefPtr<nsRange> range = new nsRange(mAccessible->GetContent());
  if (NS_FAILED(range->SetStart(mAccessible->GetContent(), aStartIndex)))
    return E_FAIL;

  if (NS_FAILED(range->SetEnd(mAccessible->GetContent(), aEndIndex)))
    return E_FAIL;

  nsresult rv =
    nsCoreUtils::ScrollSubstringTo(mAccessible->GetFrame(), range,
                                   nsIAccessibleScrollType::SCROLL_TYPE_ANYWHERE);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnTextAccessible::get_fontFamily(BSTR __RPC_FAR* aFontFamily)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aFontFamily)
    return E_INVALIDARG;
  *aFontFamily = nullptr;

  if (mAccessible->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsIFrame* frame = mAccessible->GetFrame();
  if (!frame)
    return E_FAIL;

  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(frame, getter_AddRefs(fm));

  const nsString& name = fm->GetThebesFontGroup()->GetFontAt(0)->GetName();
  if (name.IsEmpty())
    return S_FALSE;

  *aFontFamily = ::SysAllocStringLen(name.get(), name.Length());
  return *aFontFamily ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

nsIFrame*
sdnTextAccessible::GetPointFromOffset(nsIFrame* aContainingFrame,
                                      int32_t aOffset,
                                      bool aPreferNext,
                                      nsPoint& aOutPoint)
{
  nsIFrame* textFrame = nullptr;
  int32_t outOffset;
  aContainingFrame->GetChildFrameContainingOffset(aOffset, aPreferNext,
                                                  &outOffset, &textFrame);
  if (textFrame)
    textFrame->GetPointFromOffset(aOffset, &aOutPoint);

  return textFrame;
}
