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
 *   Chris Waterson <waterson@netscape.com>
 *   L. David Baron <dbaron@dbaron.org>
 *   Ben Goodger <ben@netscape.com>
 *   Mark Hammond <mhammond@skippinet.com.au>
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

#ifndef nsXULPrototypeDocument_h__
#define nsXULPrototypeDocument_h__

#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsISerializable.h"
#include "nsIDocument.h"
#include "nsCycleCollectionParticipant.h"

class nsIAtom;
class nsIPrincipal;
class nsIURI;
class nsNodeInfoManager;
class nsXULDocument;
class nsXULPrototypeElement;
class nsXULPrototypePI;
class nsXULPDGlobalObject;

/**
 * A "prototype" document that stores shared document information
 * for the XUL cache.
 * Among other things, stores the tree of nsXULPrototype*
 * objects, from which the real DOM tree is built later in
 * nsXULDocument::ResumeWalk.
 */
class nsXULPrototypeDocument : public nsIScriptGlobalObjectOwner,
                               public nsISerializable
{
public:
    static nsresult
    Create(nsIURI* aURI, nsXULPrototypeDocument** aResult);

    // nsISupports interface
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS

    // nsISerializable interface
    NS_DECL_NSISERIALIZABLE

    nsresult InitPrincipal(nsIURI* aURI, nsIPrincipal* aPrincipal);
    nsIURI* GetURI();

    /**
     * Get/set the root nsXULPrototypeElement of the document.
     */
    nsXULPrototypeElement* GetRootElement();
    void SetRootElement(nsXULPrototypeElement* aElement);

    /**
     * Add a processing instruction to the prolog. Note that only
     * PI nodes are currently stored in a XUL prototype document's
     * prolog and that they're handled separately from the rest of
     * prototype node tree.
     *
     * @param aPI an already adrefed PI proto to add. This method takes
     *            ownership of the passed PI.
     */
    nsresult AddProcessingInstruction(nsXULPrototypePI* aPI);
    /**
     * @note GetProcessingInstructions retains the ownership (the PI
     *       protos only get deleted when the proto document is deleted)
     */
    const nsTArray<nsRefPtr<nsXULPrototypePI> >& GetProcessingInstructions() const;

    /**
     * Access the array of style overlays for this document.
     *
     * Style overlays are stylesheets that need to be applied to the
     * document, but are not referenced from within the document. They
     * are currently obtained from the chrome registry via
     * nsIXULOverlayProvider::getStyleOverlays.)
     */
    void AddStyleSheetReference(nsIURI* aStyleSheet);
    const nsCOMArray<nsIURI>& GetStyleSheetReferences() const;

    /**
     * Access HTTP header data.
     * @note Not implemented.
     */
    NS_IMETHOD GetHeaderData(nsIAtom* aField, nsAString& aData) const;
    NS_IMETHOD SetHeaderData(nsIAtom* aField, const nsAString& aData);

    nsIPrincipal *DocumentPrincipal();
    void SetDocumentPrincipal(nsIPrincipal *aPrincipal);

    /**
     * If current prototype document has not yet finished loading,
     * appends aDocument to the list of documents to notify (via
     * nsXULDocument::OnPrototypeLoadDone()) and sets aLoaded to PR_FALSE.
     * Otherwise sets aLoaded to PR_TRUE.
     */
    nsresult AwaitLoadDone(nsXULDocument* aDocument, bool* aResult);

    /**
     * Notifies each document registered via AwaitLoadDone on this
     * prototype document that the prototype has finished loading.
     * The notification is performed by calling
     * nsIXULDocument::OnPrototypeLoadDone on the registered documents.
     */
    nsresult NotifyLoadDone();

    nsNodeInfoManager *GetNodeInfoManager();

    // nsIScriptGlobalObjectOwner methods
    virtual nsIScriptGlobalObject* GetScriptGlobalObject();

    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXULPrototypeDocument,
                                             nsIScriptGlobalObjectOwner)

protected:
    nsCOMPtr<nsIURI> mURI;
    nsRefPtr<nsXULPrototypeElement> mRoot;
    nsTArray<nsRefPtr<nsXULPrototypePI> > mProcessingInstructions;
    nsCOMArray<nsIURI> mStyleSheetReferences;

    nsRefPtr<nsXULPDGlobalObject> mGlobalObject;

    bool mLoaded;
    nsTArray< nsRefPtr<nsXULDocument> > mPrototypeWaiters;

    nsRefPtr<nsNodeInfoManager> mNodeInfoManager;

    nsXULPrototypeDocument();
    virtual ~nsXULPrototypeDocument();
    nsresult Init();

    friend NS_IMETHODIMP
    NS_NewXULPrototypeDocument(nsXULPrototypeDocument** aResult);

    nsXULPDGlobalObject *NewXULPDGlobalObject();

    static nsIPrincipal* gSystemPrincipal;
    static nsXULPDGlobalObject* gSystemGlobal;
    static PRUint32 gRefCnt;

    friend class nsXULPDGlobalObject;
};

#endif // nsXULPrototypeDocument_h__
