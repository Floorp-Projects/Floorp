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

/* 
 * Implementation of the wrapper class to convert the Mozilla nsIDOMAttr
 * interface into a TransforMiiX Attr interface.
 */

#include "mozilladom.h"

/*
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aAttr the nsIDOMAttr you want to wrap
 * @param aOwner the document that owns this object
 */
Attr::Attr(nsIDOMAttr* aAttr, Document* aOwner) : Node(aAttr, aOwner)
{
    NS_ASSERTION(aAttr, "Wrapper needs nsObject");
    NS_ASSERTION(aOwner, "Wrapper needs owner document");
    if (!aAttr || !aOwner)
        return;
    nsAutoString ns;
    aAttr->GetNamespaceURI(ns);
    if (ns.IsEmpty()) {
        namespaceID = kNameSpaceID_None;
        return;
    }
    NS_ASSERTION(aOwner->nsNSManager,
                 "owner document lacks namespace manager");
    if (!aOwner->nsNSManager)
        return;
    aOwner->nsNSManager->GetNameSpaceID(ns, namespaceID);
}

/*
 * Destructor
 */
Attr::~Attr()
{
}

/*
 * Call nsIDOMAttr::GetName to retrieve the name of this attribute.
 *
 * @return the attribute's name
 */
const String& Attr::getName()
{
    NSI_FROM_TX(Attr)

    nodeName.clear();
    if (nsAttr)
        nsAttr->GetName(nodeName.getNSString());
    return nodeName;
}

/*
 * Call nsIDOMAttr::GetSpecified to retrieve the specified flag for this
 * attribute.
 *
 * @return the value of the specified flag
 */
MBool Attr::getSpecified() const
{
    NSI_FROM_TX(Attr)
    MBool specified = MB_FALSE;

    if (nsAttr)
        nsAttr->GetSpecified(&specified);
    return specified;
}

/*
 * Call nsIDOMAttr::GetValue to retrieve the value of this attribute.
 *
 * @return the attribute's value
 */
const String& Attr::getValue()
{
    NSI_FROM_TX(Attr)

    nodeValue.clear();
    if (nsAttr)
        nsAttr->GetValue(nodeValue.getNSString());
    return nodeValue;
}

/*
 * Call nsIDOMAttr::SetValue to set the value of this attribute.
 *
 * @return the attribute's value
 */
void Attr::setValue(const String& aNewValue)
{
    NSI_FROM_TX(Attr)

    if (nsAttr)
        nsAttr->SetValue(aNewValue.getConstNSString());
}

/*
 * Call nsIDOMAttr::GetOwnerElement to get the owning element
 *
 * @return the xpath parent
 */
Node* Attr::getXPathParent()
{
    NSI_FROM_TX_NULL_CHECK(Attr)
    nsCOMPtr<nsIDOMElement> ownerElem;

    if (NS_SUCCEEDED(nsAttr->GetOwnerElement(getter_AddRefs(ownerElem))))
        return ownerDocument->createWrapper(ownerElem);
    return NULL;
}

/*
 * Returns the local name atomized
 *
 * @return the node's localname atom
 */
MBool Attr::getLocalName(txAtom** aLocalName)
{
    if (!aLocalName)
        return MB_FALSE;
    NSI_FROM_TX(Attr)
    if (!nsAttr)
        return MB_FALSE;
    nsAutoString lName;
    NS_ENSURE_SUCCESS(nsAttr->GetLocalName(lName), MB_FALSE);
    *aLocalName = NS_NewAtom(lName);
    NS_ENSURE_TRUE(*aLocalName, MB_FALSE);
    return MB_TRUE;
}
