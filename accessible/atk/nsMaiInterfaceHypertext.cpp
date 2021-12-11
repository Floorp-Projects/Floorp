/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "LocalAccessible-inl.h"
#include "HyperTextAccessible.h"
#include "nsMai.h"
#include "nsMaiHyperlink.h"
#include "RemoteAccessible.h"
#include "mozilla/Likely.h"

using namespace mozilla::a11y;

extern "C" {

static AtkHyperlink* getLinkCB(AtkHypertext* aText, gint aLinkIndex) {
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  AtkObject* atkHyperLink = nullptr;
  if (accWrap) {
    HyperTextAccessible* hyperText = accWrap->AsHyperText();
    NS_ENSURE_TRUE(hyperText, nullptr);

    LocalAccessible* hyperLink = hyperText->LinkAt(aLinkIndex);
    if (!hyperLink || !hyperLink->IsLink()) {
      return nullptr;
    }

    atkHyperLink = AccessibleWrap::GetAtkObject(hyperLink);
  } else if (RemoteAccessible* proxy = GetProxy(ATK_OBJECT(aText))) {
    RemoteAccessible* proxyLink = proxy->LinkAt(aLinkIndex);
    if (!proxyLink) return nullptr;

    atkHyperLink = GetWrapperFor(proxyLink);
  }

  NS_ENSURE_TRUE(IS_MAI_OBJECT(atkHyperLink), nullptr);
  return MAI_ATK_OBJECT(atkHyperLink)->GetAtkHyperlink();
}

static gint getLinkCountCB(AtkHypertext* aText) {
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (accWrap) {
    HyperTextAccessible* hyperText = accWrap->AsHyperText();
    NS_ENSURE_TRUE(hyperText, -1);
    return hyperText->LinkCount();
  }

  if (RemoteAccessible* proxy = GetProxy(ATK_OBJECT(aText))) {
    return proxy->LinkCount();
  }

  return -1;
}

static gint getLinkIndexCB(AtkHypertext* aText, gint aCharIndex) {
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (accWrap) {
    HyperTextAccessible* hyperText = accWrap->AsHyperText();
    NS_ENSURE_TRUE(hyperText, -1);

    return hyperText->LinkIndexAtOffset(aCharIndex);
  }

  if (RemoteAccessible* proxy = GetProxy(ATK_OBJECT(aText))) {
    return proxy->LinkIndexAtOffset(aCharIndex);
  }

  return -1;
}
}

void hypertextInterfaceInitCB(AtkHypertextIface* aIface) {
  NS_ASSERTION(aIface, "no interface!");
  if (MOZ_UNLIKELY(!aIface)) return;

  aIface->get_link = getLinkCB;
  aIface->get_n_links = getLinkCountCB;
  aIface->get_link_index = getLinkIndexCB;
}
