/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/dom/Element.h"

namespace mozilla {

void
ServoElementSnapshot::AddAttrs(Element* aElement)
{
  MOZ_ASSERT(aElement);

  if (!HasAny(Flags::Attributes)) {
    return;
  }

  uint32_t attrCount = aElement->GetAttrCount();
  const nsAttrName* attrName;
  for (uint32_t i = 0; i < attrCount; ++i) {
    attrName = aElement->GetAttrNameAt(i);
    const nsAttrValue* attrValue =
      aElement->GetParsedAttr(attrName->LocalName(), attrName->NamespaceID());
    mAttrs.AppendElement(ServoAttrSnapshot(*attrName, *attrValue));
  }
  mContains |= Flags::Attributes;
}

} // namespace mozilla
