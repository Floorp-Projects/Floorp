/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleHypertext.h"

#include "AccessibleHypertext_i.c"
#include "AccessibleHypertext2_i.c"

#include "mozilla/a11y/HyperTextAccessibleBase.h"

#include "IUnknownImpl.h"

using namespace mozilla::a11y;

HyperTextAccessibleBase* ia2AccessibleHypertext::TextAcc() {
  Accessible* acc = Acc();
  return acc ? acc->AsHyperTextBase() : nullptr;
}

// IUnknown
STDMETHODIMP
ia2AccessibleHypertext::QueryInterface(REFIID aIID, void** aInstancePtr) {
  if (!aInstancePtr) return E_FAIL;

  *aInstancePtr = nullptr;

  Accessible* acc = Acc();
  if (acc && acc->IsTextRole()) {
    if (aIID == IID_IAccessibleText) {
      *aInstancePtr =
          static_cast<IAccessibleText*>(static_cast<ia2AccessibleText*>(this));
    } else if (aIID == IID_IAccessibleHypertext) {
      *aInstancePtr = static_cast<IAccessibleHypertext*>(this);
    } else if (aIID == IID_IAccessibleHypertext2) {
      *aInstancePtr = static_cast<IAccessibleHypertext2*>(this);
    } else if (aIID == IID_IAccessibleEditableText) {
      *aInstancePtr = static_cast<IAccessibleEditableText*>(this);
    } else if (aIID == IID_IAccessibleTextSelectionContainer) {
      *aInstancePtr = static_cast<IAccessibleTextSelectionContainer*>(this);
    }

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

  HyperTextAccessibleBase* hyperText = TextAcc();
  if (!hyperText) return CO_E_OBJNOTCONNECTED;

  *aHyperlinkCount = hyperText->LinkCount();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleHypertext::get_hyperlink(long aLinkIndex,
                                      IAccessibleHyperlink** aHyperlink) {
  if (!aHyperlink) return E_INVALIDARG;

  *aHyperlink = nullptr;

  HyperTextAccessibleBase* hyperText = TextAcc();
  if (!hyperText) {
    return CO_E_OBJNOTCONNECTED;
  }

  Accessible* hyperLink = hyperText->LinkAt(aLinkIndex);

  if (!hyperLink) return E_FAIL;

  RefPtr<IAccessibleHyperlink> result = MsaaAccessible::GetFrom(hyperLink);
  result.forget(aHyperlink);
  return S_OK;
}

STDMETHODIMP
ia2AccessibleHypertext::get_hyperlinkIndex(long aCharIndex,
                                           long* aHyperlinkIndex) {
  if (!aHyperlinkIndex) return E_INVALIDARG;

  *aHyperlinkIndex = 0;

  HyperTextAccessibleBase* hyperAcc = TextAcc();
  if (!hyperAcc) return CO_E_OBJNOTCONNECTED;

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

  HyperTextAccessibleBase* hyperText = TextAcc();
  if (!hyperText) {
    return CO_E_OBJNOTCONNECTED;
  }

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
    Accessible* hyperLink = hyperText->LinkAt(i);
    MOZ_ASSERT(hyperLink);
    RefPtr<IAccessibleHyperlink> iaHyper = MsaaAccessible::GetFrom(hyperLink);
    iaHyper.forget(&(*aHyperlinks)[i]);
  }

  return S_OK;
}
