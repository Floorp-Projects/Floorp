/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * nsMappedAttributeElement is the base for elements supporting style mapped
 * attributes via nsMappedAttributes (HTML and MathML).
 */

#ifndef NS_MAPPEDATTRIBUTEELEMENT_H_
#define NS_MAPPEDATTRIBUTEELEMENT_H_

#include "mozilla/Attributes.h"
#include "nsStyledElement.h"

namespace mozilla {
class MappedDeclarations;
}

class nsMappedAttributes;
struct nsRuleData;

typedef void (*nsMapRuleToAttributesFunc)(const nsMappedAttributes* aAttributes,
                                          mozilla::MappedDeclarations&);

typedef nsStyledElement nsMappedAttributeElementBase;

class nsMappedAttributeElement : public nsMappedAttributeElementBase
{

protected:

  explicit nsMappedAttributeElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsMappedAttributeElementBase(aNodeInfo)
  {}

public:
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;

  static void MapNoAttributesInto(const nsMappedAttributes* aAttributes,
                                  mozilla::MappedDeclarations&);

  virtual bool SetAndSwapMappedAttribute(nsAtom* aName,
                                         nsAttrValue& aValue,
                                         bool* aValueWasSet,
                                         nsresult* aRetval) override;

  virtual void NodeInfoChanged(nsIDocument* aOldDoc) override;
};

#endif // NS_MAPPEDATTRIBUTEELEMENT_H_
