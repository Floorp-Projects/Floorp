/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXMLElement.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"

class nsXMLEventsElement : public nsXMLElement {
public:
  nsXMLEventsElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsXMLEventsElement();
  NS_FORWARD_NSIDOMNODE(nsXMLElement::)

  virtual nsIAtom *GetIDAttributeName() const;
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
};

nsXMLEventsElement::nsXMLEventsElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsXMLElement(aNodeInfo)
{
}

nsXMLEventsElement::~nsXMLEventsElement()
{
}

nsIAtom *
nsXMLEventsElement::GetIDAttributeName() const
{
  if (mNodeInfo->Equals(nsGkAtoms::listener))
    return nsGkAtoms::id;
  return nsXMLElement::GetIDAttributeName();
}

nsresult
nsXMLEventsElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsIAtom* aPrefix,
                            const nsAString& aValue, bool aNotify)
{
  if (mNodeInfo->Equals(nsGkAtoms::listener) && 
      mNodeInfo->GetDocument() && aNameSpaceID == kNameSpaceID_None && 
      aName == nsGkAtoms::event)
    mNodeInfo->GetDocument()->AddXMLEventsContent(this);
  return nsXMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                   aNotify);
}

NS_IMPL_ELEMENT_CLONE(nsXMLEventsElement)

nsresult
NS_NewXMLEventsElement(nsIContent** aInstancePtrResult,
                       already_AddRefed<nsINodeInfo> aNodeInfo)
{
  nsXMLEventsElement* it = new nsXMLEventsElement(aNodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(*aInstancePtrResult = it);
  return NS_OK;
}

