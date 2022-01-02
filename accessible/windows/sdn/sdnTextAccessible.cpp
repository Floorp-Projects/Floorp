/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdnTextAccessible.h"

#include "ISimpleDOM.h"

#include "nsCoreUtils.h"
#include "DocAccessible.h"

#include "nsIFrame.h"
#include "nsFontMetrics.h"
#include "nsPresContext.h"
#include "nsLayoutUtils.h"
#include "nsRange.h"
#include "gfxTextRun.h"
#include "nsIAccessibleTypes.h"
#include "mozilla/gfx/2D.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// sdnTextAccessible
////////////////////////////////////////////////////////////////////////////////

IMPL_IUNKNOWN_QUERY_HEAD(sdnTextAccessible)
IMPL_IUNKNOWN_QUERY_IFACE(ISimpleDOMText)
IMPL_IUNKNOWN_QUERY_TAIL_AGGREGATED(mMsaa)

STDMETHODIMP
sdnTextAccessible::get_domText(BSTR __RPC_FAR* aText) {
  if (!aText) return E_INVALIDARG;
  *aText = nullptr;

  AccessibleWrap* acc = mMsaa->LocalAcc();
  if (!acc) return CO_E_OBJNOTCONNECTED;

  nsAutoString nodeValue;

  acc->GetContent()->GetNodeValue(nodeValue);
  if (nodeValue.IsEmpty()) return S_FALSE;

  *aText = ::SysAllocStringLen(nodeValue.get(), nodeValue.Length());
  return *aText ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
sdnTextAccessible::get_clippedSubstringBounds(
    unsigned int aStartIndex, unsigned int aEndIndex, int __RPC_FAR* aX,
    int __RPC_FAR* aY, int __RPC_FAR* aWidth, int __RPC_FAR* aHeight) {
  nscoord x = 0, y = 0, width = 0, height = 0;
  HRESULT rv = get_unclippedSubstringBounds(aStartIndex, aEndIndex, &x, &y,
                                            &width, &height);
  if (FAILED(rv)) return rv;

  DocAccessible* document = mMsaa->LocalAcc()->Document();
  NS_ASSERTION(
      document,
      "There must always be a doc accessible, but there isn't. Crash!");

  nsIntRect docRect = document->Bounds();
  nsIntRect unclippedRect(x, y, width, height);

  nsIntRect clippedRect;
  clippedRect.IntersectRect(unclippedRect, docRect);

  *aX = clippedRect.X();
  *aY = clippedRect.Y();
  *aWidth = clippedRect.Width();
  *aHeight = clippedRect.Height();
  return S_OK;
}

STDMETHODIMP
sdnTextAccessible::get_unclippedSubstringBounds(
    unsigned int aStartIndex, unsigned int aEndIndex, int __RPC_FAR* aX,
    int __RPC_FAR* aY, int __RPC_FAR* aWidth, int __RPC_FAR* aHeight) {
  if (!aX || !aY || !aWidth || !aHeight) return E_INVALIDARG;
  *aX = *aY = *aWidth = *aHeight = 0;

  AccessibleWrap* acc = mMsaa->LocalAcc();
  if (!acc) return CO_E_OBJNOTCONNECTED;

  nsIFrame* frame = acc->GetFrame();
  NS_ENSURE_TRUE(frame, E_FAIL);

  nsPoint startPoint, endPoint;
  nsIFrame* startFrame =
      GetPointFromOffset(frame, aStartIndex, true, startPoint);
  nsIFrame* endFrame = GetPointFromOffset(frame, aEndIndex, false, endPoint);
  if (!startFrame || !endFrame) return E_FAIL;

  nsRect sum;
  nsIFrame* iter = startFrame;
  nsIFrame* stopLoopFrame = endFrame->GetNextContinuation();
  for (; iter != stopLoopFrame; iter = iter->GetNextContinuation()) {
    nsRect rect = iter->GetScreenRectInAppUnits();
    nscoord start = (iter == startFrame) ? startPoint.x : 0;
    nscoord end = (iter == endFrame) ? endPoint.x : rect.Width();
    rect.MoveByX(start);
    rect.SetWidth(end - start);
    sum.UnionRect(sum, rect);
  }

  nsPresContext* presContext = acc->Document()->PresContext();
  *aX = presContext->AppUnitsToDevPixels(sum.X());
  *aY = presContext->AppUnitsToDevPixels(sum.Y());
  *aWidth = presContext->AppUnitsToDevPixels(sum.Width());
  *aHeight = presContext->AppUnitsToDevPixels(sum.Height());

  return S_OK;
}

STDMETHODIMP
sdnTextAccessible::scrollToSubstring(unsigned int aStartIndex,
                                     unsigned int aEndIndex) {
  AccessibleWrap* acc = mMsaa->LocalAcc();
  if (!acc) return CO_E_OBJNOTCONNECTED;

  RefPtr<nsRange> range = nsRange::Create(acc->GetContent());
  if (NS_FAILED(range->SetStart(acc->GetContent(), aStartIndex))) return E_FAIL;

  if (NS_FAILED(range->SetEnd(acc->GetContent(), aEndIndex))) return E_FAIL;

  nsresult rv = nsCoreUtils::ScrollSubstringTo(
      acc->GetFrame(), range, nsIAccessibleScrollType::SCROLL_TYPE_ANYWHERE);
  return GetHRESULT(rv);
}

STDMETHODIMP
sdnTextAccessible::get_fontFamily(BSTR __RPC_FAR* aFontFamily) {
  if (!aFontFamily) return E_INVALIDARG;
  *aFontFamily = nullptr;

  AccessibleWrap* acc = mMsaa->LocalAcc();
  if (!acc) return CO_E_OBJNOTCONNECTED;

  nsIFrame* frame = acc->GetFrame();
  if (!frame) return E_FAIL;

  RefPtr<nsFontMetrics> fm = nsLayoutUtils::GetFontMetricsForFrame(frame, 1.0f);

  const nsCString& name =
      fm->GetThebesFontGroup()->GetFirstValidFont()->GetName();
  if (name.IsEmpty()) return S_FALSE;

  NS_ConvertUTF8toUTF16 str(name);
  *aFontFamily = ::SysAllocStringLen(str.get(), str.Length());
  return *aFontFamily ? S_OK : E_OUTOFMEMORY;
}

nsIFrame* sdnTextAccessible::GetPointFromOffset(nsIFrame* aContainingFrame,
                                                int32_t aOffset,
                                                bool aPreferNext,
                                                nsPoint& aOutPoint) {
  nsIFrame* textFrame = nullptr;
  int32_t outOffset;
  aContainingFrame->GetChildFrameContainingOffset(aOffset, aPreferNext,
                                                  &outOffset, &textFrame);
  if (textFrame) textFrame->GetPointFromOffset(aOffset, &aOutPoint);

  return textFrame;
}
