/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*



 */

#ifndef nsIXULPrototypeDocument_h__
#define nsIXULPrototypeDocument_h__

#include "nsISupports.h"
#include "nsAWritableString.h"

class nsIAtom;
class nsIPrincipal;
class nsIStyleSheet;
class nsIURI;
class nsString;
class nsVoidArray;
class nsXULPrototypeElement;

// {187A63D0-8337-11d3-BE47-00104BDE6048}
#define NS_IXULPROTOTYPEDOCUMENT_IID \
{ 0x187a63d0, 0x8337, 0x11d3, { 0xbe, 0x47, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } }


class nsIXULPrototypeDocument : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXULPROTOTYPEDOCUMENT_IID);

    /**
     * Retrieve the URI of the document
     */
    NS_IMETHOD SetURI(nsIURI* aURI) = 0;
    NS_IMETHOD GetURI(nsIURI** aResult) = 0;

    /**
     * Retrieve the root XULPrototype element of the document.
     */
    NS_IMETHOD GetRootElement(nsXULPrototypeElement** aResult) = 0;
    NS_IMETHOD SetRootElement(nsXULPrototypeElement* aElement) = 0;

    NS_IMETHOD AddStyleSheetReference(nsIURI* aStyleSheet) = 0;
    NS_IMETHOD GetStyleSheetReferences(nsISupportsArray** aResult) = 0;

    NS_IMETHOD AddOverlayReference(nsIURI* aURI) = 0;
    NS_IMETHOD GetOverlayReferences(nsISupportsArray** aResult) = 0;

    NS_IMETHOD GetHeaderData(nsIAtom* aField, nsAWritableString& aData) const = 0;
    NS_IMETHOD SetHeaderData(nsIAtom* aField, const nsAReadableString& aData) = 0;

    NS_IMETHOD GetDocumentPrincipal(nsIPrincipal** aResult) = 0;
    NS_IMETHOD SetDocumentPrincipal(nsIPrincipal* aPrincipal) = 0;
};


extern NS_IMETHODIMP
NS_NewXULPrototypeDocument(nsISupports* aOuter, REFNSIID aIID, void** aResult);

#endif // nsIXULPrototypeDocument_h__
