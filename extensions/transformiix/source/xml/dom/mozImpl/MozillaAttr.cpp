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
 * Implementation of the wrapper class to convert the Mozilla nsIDOMAttr
 * interface into a TransforMiiX Attr interface.
 */

#include "mozilladom.h"
#include "nsIDOMAttr.h"
#include "nsIDOMElement.h"
#include "nsString.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aAttr the nsIDOMAttr you want to wrap
 * @param aOwner the document that owns this object
 */
Attr::Attr(nsIDOMAttr* aAttr, Document* aOwner) : Node(aAttr, aOwner)
{
    nsCOMPtr<nsIDOMElement> parent;
    aAttr->GetOwnerElement(getter_AddRefs(parent));
    mParent = do_QueryInterface(parent);

    nsAutoString nameString;
    aAttr->GetLocalName(nameString);
    mLocalName = do_GetAtom(nameString);

    nsAutoString ns;
    aAttr->GetNamespaceURI(ns);
    mNamespaceID = kNameSpaceID_None;
    if (!ns.IsEmpty()) {
        NS_ASSERTION(gTxNameSpaceManager,"No namespace manager");
        if (gTxNameSpaceManager) {
            gTxNameSpaceManager->GetNameSpaceID(ns, mNamespaceID);
        }
    }
    mNSId = mNamespaceID;
}

/**
 * Destructor
 */
Attr::~Attr()
{
}

/**
 * Call nsIDOMAttr::GetOwnerElement to get the owning element
 *
 * @return the xpath parent
 */
Node* Attr::getXPathParent()
{
    nsCOMPtr<nsIDOMElement> ownerElem =
        do_QueryInterface(mParent);
    if (!ownerElem) {
        return nsnull;
    }
    return mOwnerDocument->createElement(ownerElem);
}

/**
 * Returns the local name atomized
 *
 * @return the node's localname atom
 */
MBool Attr::getLocalName(nsIAtom** aLocalName)
{
    if (!aLocalName) {
        return MB_FALSE;
    }
    *aLocalName = mLocalName;
    NS_ENSURE_TRUE(*aLocalName, MB_FALSE);
    TX_ADDREF_ATOM(*aLocalName);
    return MB_TRUE;
}
