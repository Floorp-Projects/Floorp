/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMappedAttributeElement.h"
#include "nsIDocument.h"


bool
nsMappedAttributeElement::SetAndSwapMappedAttribute(nsAtom* aName,
                                                    nsAttrValue& aValue,
                                                    bool* aValueWasSet,
                                                    nsresult* aRetval)
{
  nsHTMLStyleSheet* sheet = OwnerDoc()->GetAttributeStyleSheet();
  *aRetval = mAttrsAndChildren.SetAndSwapMappedAttr(aName, aValue,
                                                    this, sheet, aValueWasSet);
  return true;
}

nsMapRuleToAttributesFunc
nsMappedAttributeElement::GetAttributeMappingFunction() const
{
  return &MapNoAttributesInto;
}

void
nsMappedAttributeElement::MapNoAttributesInto(const nsMappedAttributes*,
                                              mozilla::MappedDeclarations&)
{
}

void
nsMappedAttributeElement::NodeInfoChanged(nsIDocument* aOldDoc)
{
  nsHTMLStyleSheet* sheet = OwnerDoc()->GetAttributeStyleSheet();
  mAttrsAndChildren.SetMappedAttrStyleSheet(sheet);
  nsMappedAttributeElementBase::NodeInfoChanged(aOldDoc);
}
