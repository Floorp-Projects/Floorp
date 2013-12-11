/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "Accessible-inl.h"
#include "HyperTextAccessible.h"
#include "nsMai.h"
#include "nsMaiHyperlink.h"
#include "mozilla/Likely.h"

using namespace mozilla::a11y;

extern "C" {

static AtkHyperlink*
getLinkCB(AtkHypertext *aText, gint aLinkIndex)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return nullptr;

  HyperTextAccessible* hyperText = accWrap->AsHyperText();
  NS_ENSURE_TRUE(hyperText, nullptr);

  Accessible* hyperLink = hyperText->LinkAt(aLinkIndex);
  if (!hyperLink)
    return nullptr;

  AtkObject* hyperLinkAtkObj = AccessibleWrap::GetAtkObject(hyperLink);
  AccessibleWrap* accChild = GetAccessibleWrap(hyperLinkAtkObj);
  NS_ENSURE_TRUE(accChild, nullptr);

  MaiHyperlink *maiHyperlink = accChild->GetMaiHyperlink();
  NS_ENSURE_TRUE(maiHyperlink, nullptr);
  return maiHyperlink->GetAtkHyperlink();
}

static gint
getLinkCountCB(AtkHypertext *aText)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return -1;

  HyperTextAccessible* hyperText = accWrap->AsHyperText();
  NS_ENSURE_TRUE(hyperText, -1);

  return hyperText->LinkCount();
}

static gint
getLinkIndexCB(AtkHypertext *aText, gint aCharIndex)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return -1;

  HyperTextAccessible* hyperText = accWrap->AsHyperText();
  NS_ENSURE_TRUE(hyperText, -1);

  return hyperText->LinkIndexAtOffset(aCharIndex);
}
}

void
hypertextInterfaceInitCB(AtkHypertextIface* aIface)
{
  NS_ASSERTION(aIface, "no interface!");
  if (MOZ_UNLIKELY(!aIface))
    return;

  aIface->get_link = getLinkCB;
  aIface->get_n_links = getLinkCountCB;
  aIface->get_link_index = getLinkIndexCB;
}
