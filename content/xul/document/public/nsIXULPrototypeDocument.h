/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*



 */

#ifndef nsIXULPrototypeDocument_h__
#define nsIXULPrototypeDocument_h__

#include "nsISerializable.h"
#include "nsAString.h"

class nsIAtom;
class nsIPrincipal;
class nsIStyleSheet;
class nsIURI;
class nsString;
class nsVoidArray;
class nsXULPrototypeElement;
class nsIXULDocument;
class nsIScriptGlobalObject;
class nsNodeInfoManager;
class nsISupportsArray;

#define NS_IXULPROTOTYPEDOCUMENT_IID \
{ 0x726f0ab8, 0xb3cb, 0x11d8, { 0xb2, 0x67, 0x00, 0x0a, 0x95, 0xdc, 0x23, 0x4c } }

class nsIXULPrototypeDocument : public nsISerializable
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

    NS_IMETHOD GetHeaderData(nsIAtom* aField, nsAString& aData) const = 0;
    NS_IMETHOD SetHeaderData(nsIAtom* aField, const nsAString& aData) = 0;

    virtual nsIPrincipal *GetDocumentPrincipal() = 0;
    virtual void SetDocumentPrincipal(nsIPrincipal *aPrincipal) = 0;

    virtual nsNodeInfoManager *GetNodeInfoManager() = 0;

    NS_IMETHOD AwaitLoadDone(nsIXULDocument* aDocument, PRBool* aResult) = 0;
    NS_IMETHOD NotifyLoadDone() = 0;
};


// CID for factory-based creation, used only for deserialization.
#define NS_XULPROTOTYPEDOCUMENT_CLASSNAME "XUL Prototype Document"
#define NS_XULPROTOTYPEDOCUMENT_CID \
    {0xa08101ae,0xc0e8,0x4464,{0x99,0x9e,0xe5,0xa4,0xd7,0x09,0xa9,0x28}}


NS_IMETHODIMP
NS_NewXULPrototypeDocument(nsISupports* aOuter, REFNSIID aIID, void** aResult);

#endif // nsIXULPrototypeDocument_h__
