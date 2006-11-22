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

#ifndef nsIXULPrototypeDocument_h__
#define nsIXULPrototypeDocument_h__

#include "nsISerializable.h"
#include "nsTArray.h"
#include "nsAString.h"

class nsIAtom;
class nsIPrincipal;
class nsIURI;
class nsXULPrototypeElement;
class nsXULPrototypePI;
class nsIXULDocument;
class nsNodeInfoManager;
class nsISupportsArray;

#define NS_IXULPROTOTYPEDOCUMENT_IID \
{ 0x054a0bfe, 0xe7bc, 0x44b0, \
 { 0x90, 0x97, 0x6c, 0x95, 0xe4, 0xd6, 0x5f, 0xa3 } }

/**
 * A "prototype" document that stores shared document information
 * for the XUL cache.
 */
class nsIXULPrototypeDocument : public nsISerializable
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXULPROTOTYPEDOCUMENT_IID)

    /**
     * Method to initialize a prototype document's principal.  Either this
     * method or Read() must be called immediately after the prototype document
     * is created.  aURI must be non-null.  aPrincipal is allowed to be null;
     * in this case the prototype will get a null principal object.  If this
     * method returns error, do NOT use the prototype document for anything.
     */
    NS_IMETHOD InitPrincipal(nsIURI* aURI, nsIPrincipal* aPrincipal) = 0;

    /**
     * Retrieve the URI of the document
     */
    NS_IMETHOD GetURI(nsIURI** aResult) = 0;

    /**
     * Access the root nsXULPrototypeElement of the document.
     */
    NS_IMETHOD GetRootElement(nsXULPrototypeElement** aResult) = 0;
    NS_IMETHOD SetRootElement(nsXULPrototypeElement* aElement) = 0;

    /**
     * Add a processing instruction to the prolog. Note that only
     * PI nodes are currently stored in a XUL prototype document's
     * prolog and that they're handled separately from the rest of
     * prototype node tree.
     *
     * @param aPI an already adrefed PI proto to add. This method takes
     *            ownership of the passed PI.
     */
    NS_IMETHOD AddProcessingInstruction(nsXULPrototypePI* aPI) = 0;
    /**
     * @note GetProcessingInstructions retains the ownership (the PI
     *       protos only get deleted when the proto document is deleted)
     */
    virtual const nsTArray<nsXULPrototypePI*>&
        GetProcessingInstructions() const = 0;

    /**
     * Access the array of style overlays for this document.
     *
     * XXX shouldn't use nsISupportsArray here
     */
    NS_IMETHOD AddStyleSheetReference(nsIURI* aStyleSheet) = 0;
    NS_IMETHOD GetStyleSheetReferences(nsISupportsArray** aResult) = 0;

    /**
     * Access HTTP header data.
     * @note Not implemented.
     */
    NS_IMETHOD GetHeaderData(nsIAtom* aField, nsAString& aData) const = 0;
    NS_IMETHOD SetHeaderData(nsIAtom* aField, const nsAString& aData) = 0;

    // Guaranteed to return non-null for a properly-initialized prototype
    // document.
    virtual nsIPrincipal *DocumentPrincipal() = 0;

    virtual nsNodeInfoManager *GetNodeInfoManager() = 0;

    /**
     * If current prototype document has not yet finished loading,
     * appends aDocument to the list of documents to notify (via
     * OnPrototypeLoadDone()) and sets aLoaded to PR_FALSE.
     * Otherwise sets aLoaded to PR_TRUE.
     */
    NS_IMETHOD AwaitLoadDone(nsIXULDocument* aDocument,
                             PRBool* aLoaded) = 0;
    /**
     * Notifies each document registered via AwaitLoadDone on this
     * prototype document that the prototype has finished loading.
     * The notification is performed by calling
     * nsIXULDocument::OnPrototypeLoadDone on the registered documents.
     */
    NS_IMETHOD NotifyLoadDone() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXULPrototypeDocument,
                              NS_IXULPROTOTYPEDOCUMENT_IID)

// CID for factory-based creation, used only for deserialization.
#define NS_XULPROTOTYPEDOCUMENT_CLASSNAME "XUL Prototype Document"
#define NS_XULPROTOTYPEDOCUMENT_CID \
    {0xa08101ae,0xc0e8,0x4464,{0x99,0x9e,0xe5,0xa4,0xd7,0x09,0xa9,0x28}}


NS_IMETHODIMP
NS_NewXULPrototypeDocument(nsISupports* aOuter, REFNSIID aIID, void** aResult);

#endif // nsIXULPrototypeDocument_h__
