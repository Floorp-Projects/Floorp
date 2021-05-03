/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleHypertext.h"

#include "AccessibleHypertext_i.c"

#include "HyperTextAccessibleWrap.h"
#include "IUnknownImpl.h"

using namespace mozilla::a11y;

HyperTextAccessibleWrap* ia2AccessibleHypertext::TextAcc() {
  AccessibleWrap* acc = LocalAcc();
  return static_cast<HyperTextAccessibleWrap*>(acc);
}

// IUnknown
STDMETHODIMP
ia2AccessibleHypertext::QueryInterface(REFIID aIID, void** aInstancePtr) {
  if (!aInstancePtr) return E_FAIL;

  *aInstancePtr = nullptr;

  HyperTextAccessibleWrap* hyp = TextAcc();
  if (hyp && hyp->IsTextRole()) {
    if (aIID == IID_IAccessibleText)
      *aInstancePtr =
          static_cast<IAccessibleText*>(static_cast<ia2AccessibleText*>(this));
    else if (aIID == IID_IAccessibleHypertext)
      *aInstancePtr = static_cast<IAccessibleHypertext*>(this);
    else if (aIID == IID_IAccessibleHypertext2)
      *aInstancePtr = static_cast<IAccessibleHypertext2*>(this);
    else if (aIID == IID_IAccessibleEditableText)
      *aInstancePtr = static_cast<IAccessibleEditableText*>(this);

    if (*aInstancePtr) {
      AddRef();
      return S_OK;
    }
  }

  return MsaaAccessible::QueryInterface(aIID, aInstancePtr);
}

// IAccessibleHypertext

STDMETHODIMP
ia2AccessibleHypertext::get_nHyperlinks(long* aHyperlinkCount) {
  if (!aHyperlinkCount) return E_INVALIDARG;

  *aHyperlinkCount = 0;

  HyperTextAccessibleWrap* hyperText = TextAcc();
  if (!hyperText) return CO_E_OBJNOTCONNECTED;
  MOZ_ASSERT(!hyperText->IsProxy());

  *aHyperlinkCount = hyperText->LinkCount();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleHypertext::get_hyperlink(long aLinkIndex,
                                      IAccessibleHyperlink** aHyperlink) {
  if (!aHyperlink) return E_INVALIDARG;

  *aHyperlink = nullptr;

  LocalAccessible* hyperLink;
  HyperTextAccessibleWrap* hyperText = TextAcc();
  if (!hyperText) {
    return CO_E_OBJNOTCONNECTED;
  }
  MOZ_ASSERT(!hyperText->IsProxy());

  hyperLink = hyperText->LinkAt(aLinkIndex);

  if (!hyperLink) return E_FAIL;

  // GetNativeInterface returns an IAccessible, but we need an
  // IAccessibleHyperlink, so use MsaaAccessible::GetFrom instead and let
  // RefPtr cast it.
  RefPtr<IAccessibleHyperlink> result = MsaaAccessible::GetFrom(hyperLink);
  result.forget(aHyperlink);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleHypertext::get_hyperlinkIndex(long aCharIndex,
                                           long* aHyperlinkIndex) {
  if (!aHyperlinkIndex) return E_INVALIDARG;

  *aHyperlinkIndex = 0;

  HyperTextAccessibleWrap* hyperAcc = TextAcc();
  if (!hyperAcc) return CO_E_OBJNOTCONNECTED;
  MOZ_ASSERT(!hyperAcc->IsProxy());

  *aHyperlinkIndex = hyperAcc->LinkIndexAtOffset(aCharIndex);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleHypertext::get_hyperlinks(IAccessibleHyperlink*** aHyperlinks,
                                       long* aNHyperlinks) {
  if (!aHyperlinks || !aNHyperlinks) {
    return E_INVALIDARG;
  }

  *aHyperlinks = nullptr;
  *aNHyperlinks = 0;

  HyperTextAccessibleWrap* hyperText = TextAcc();
  if (!hyperText) {
    return CO_E_OBJNOTCONNECTED;
  }
  MOZ_ASSERT(!hyperText->IsProxy());

  uint32_t count = hyperText->LinkCount();
  *aNHyperlinks = count;

  if (count == 0) {
    *aHyperlinks = nullptr;
    return S_FALSE;
  }

  *aHyperlinks = static_cast<IAccessibleHyperlink**>(
      ::CoTaskMemAlloc(sizeof(IAccessibleHyperlink*) * count));
  if (!*aHyperlinks) {
    return E_OUTOFMEMORY;
  }

  for (uint32_t i = 0; i < count; ++i) {
    LocalAccessible* hyperLink = hyperText->LinkAt(i);
    MOZ_ASSERT(hyperLink);
    // GetNativeInterface returns an IAccessible, but we need an
    // IAccessibleHyperlink, so use MsaaAccessible::GetFrom instead and let
    // RefPtr cast it.
    RefPtr<IAccessibleHyperlink> iaHyper = MsaaAccessible::GetFrom(hyperLink);
    iaHyper.forget(&(*aHyperlinks)[i]);
  }

  return S_OK;
}
