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

// IAccessibleHypertext

STDMETHODIMP
ia2AccessibleHypertext::get_nHyperlinks(long* aHyperlinkCount)
{
  if (!aHyperlinkCount)
    return E_INVALIDARG;

  *aHyperlinkCount = 0;

  MOZ_ASSERT(!HyperTextProxyFor(this));

  HyperTextAccessibleWrap* hyperText = static_cast<HyperTextAccessibleWrap*>(this);
  if (hyperText->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  *aHyperlinkCount = hyperText->LinkCount();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleHypertext::get_hyperlink(long aLinkIndex,
                                      IAccessibleHyperlink** aHyperlink)
{
  if (!aHyperlink)
    return E_INVALIDARG;

  *aHyperlink = nullptr;

  AccessibleWrap* hyperLink;
  MOZ_ASSERT(!HyperTextProxyFor(this));
  HyperTextAccessibleWrap* hyperText = static_cast<HyperTextAccessibleWrap*>(this);
  if (hyperText->IsDefunct()) {
    return CO_E_OBJNOTCONNECTED;
  }

  hyperLink = static_cast<AccessibleWrap*>(hyperText->LinkAt(aLinkIndex));

  if (!hyperLink)
    return E_FAIL;

  *aHyperlink =
    static_cast<IAccessibleHyperlink*>(hyperLink);
  (*aHyperlink)->AddRef();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleHypertext::get_hyperlinkIndex(long aCharIndex, long* aHyperlinkIndex)
{
  if (!aHyperlinkIndex)
    return E_INVALIDARG;

  *aHyperlinkIndex = 0;

  MOZ_ASSERT(!HyperTextProxyFor(this));

  HyperTextAccessibleWrap* hyperAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (hyperAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  *aHyperlinkIndex = hyperAcc->LinkIndexAtOffset(aCharIndex);
  return S_OK;
}

