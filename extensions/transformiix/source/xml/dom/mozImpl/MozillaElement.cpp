/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 * Contributor(s): Tom Kneeland
 *                 Peter Van der Beken <peter.vanderbeken@pandora.be>
 *
 */

/**
 * Implementation of the wrapper class to convert the Mozilla nsIDOMElement
 * interface into a TransforMiiX Element interface.
 */

#include "mozilladom.h"
#include "nsIDOMElement.h"
#include "nsINodeInfo.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aElement the nsIDOMElement you want to wrap
 * @param aOwner the document that owns this object
 */
Element::Element(nsIDOMElement* aElement, Document* aOwner) :
        Node(aElement, aOwner)
{
    nsCOMPtr<nsIContent> cont(do_QueryInterface(aElement));
    NS_ASSERTION(cont, "Element doesn't implement nsIContent");
    nsCOMPtr<nsINodeInfo> nodeInfo;
    cont->GetNodeInfo(*getter_AddRefs(nodeInfo));
    NS_ASSERTION(nodeInfo, "a element without nodeinfo");
    nodeInfo->GetNamespaceID(mNamespaceID);
}

/**
 * Destructor
 */
Element::~Element()
{
}

/**
 * Call nsIDOMElement::SetAttributeNS to set an attribute to the specified
 * value.
 *
 * @param aNamespaceURI the URI of the namespace of the attribute
 * @param aName the name of the attribute you want to set
 * @param aValue the value you want to set the attribute to
 */
void Element::setAttributeNS(const nsAString& aNamespaceURI,
                             const nsAString& aName,
                             const nsAString& aValue)
{
    NSI_FROM_TX(Element);
    nsElement->SetAttributeNS(aNamespaceURI, aName, aValue);
}

/**
 * Call nsIContent::GetAttr for the localname and nsID.
 */
MBool Element::getAttr(txAtom* aLocalName, PRInt32 aNSID,
                       nsAString& aValue)
{
    nsCOMPtr<nsIContent> cont(do_QueryInterface(mMozObject));
    NS_ASSERTION(cont, "Element doesn't implement nsIContent");
    if (!cont || !cont->HasAttr(aNSID, aLocalName)) {
        aValue.Truncate();
        return MB_FALSE;
    }
    nsresult rv;
    rv = cont->GetAttr(aNSID, aLocalName, aValue);
    NS_ENSURE_SUCCESS(rv, MB_FALSE);
    return MB_TRUE;
}

/**
 * Call nsIContent::GetAttr for the localname and nsID.
 */
MBool Element::hasAttr(txAtom* aLocalName, PRInt32 aNSID)
{
    nsCOMPtr<nsIContent> cont(do_QueryInterface(mMozObject));
    NS_ASSERTION(cont, "Element doesn't implement nsIContent");
    if (!cont) {
        return MB_FALSE;
    }
    return cont->HasAttr(aNSID, aLocalName);
}

/**
 * Returns the local name atomized
 *
 * @return the node's localname atom
 */
MBool Element::getLocalName(txAtom** aLocalName)
{
    NS_ENSURE_TRUE(aLocalName, MB_FALSE);
    nsCOMPtr<nsIContent> cont(do_QueryInterface(mMozObject));
    NS_ASSERTION(cont, "Element doesn't implement nsIContent");
    if (!cont) {
        return MB_FALSE;
    }
    cont->GetTag(*aLocalName);
    return MB_TRUE;
}
