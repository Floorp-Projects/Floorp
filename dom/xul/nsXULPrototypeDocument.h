/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULPrototypeDocument_h__
#define nsXULPrototypeDocument_h__

#include "js/TracingAPI.h"
#include "mozilla/Attributes.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsISerializable.h"
#include "nsCycleCollectionParticipant.h"

class nsIAtom;
class nsIPrincipal;
class nsIURI;
class nsNodeInfoManager;
class nsXULPrototypeElement;
class nsXULPrototypePI;

namespace mozilla {
namespace dom {
class XULDocument;
} // namespace dom
} // namespace mozilla

/**
 * A "prototype" document that stores shared document information
 * for the XUL cache.
 * Among other things, stores the tree of nsXULPrototype*
 * objects, from which the real DOM tree is built later in
 * XULDocument::ResumeWalk.
 */
class nsXULPrototypeDocument final : public nsISerializable
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
    const nsTArray<RefPtr<nsXULPrototypePI> >& GetProcessingInstructions() const;

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
     * XULDocument::OnPrototypeLoadDone()) and sets aLoaded to false.
     * Otherwise sets aLoaded to true.
     */
    nsresult AwaitLoadDone(mozilla::dom::XULDocument* aDocument, bool* aResult);

    /**
     * Notifies each document registered via AwaitLoadDone on this
     * prototype document that the prototype has finished loading.
     * The notification is performed by calling
     * nsIXULDocument::OnPrototypeLoadDone on the registered documents.
     */
    nsresult NotifyLoadDone();

    nsNodeInfoManager *GetNodeInfoManager();

    void MarkInCCGeneration(uint32_t aCCGeneration);

    NS_DECL_CYCLE_COLLECTION_CLASS(nsXULPrototypeDocument)

    void TraceProtos(JSTracer* aTrc, uint32_t aGCNumber);

protected:
    nsCOMPtr<nsIURI> mURI;
    RefPtr<nsXULPrototypeElement> mRoot;
    nsTArray<RefPtr<nsXULPrototypePI> > mProcessingInstructions;
    nsCOMArray<nsIURI> mStyleSheetReferences;

    bool mLoaded;
    nsTArray< RefPtr<mozilla::dom::XULDocument> > mPrototypeWaiters;

    RefPtr<nsNodeInfoManager> mNodeInfoManager;

    uint32_t mCCGeneration;
    uint32_t mGCNumber;

    nsXULPrototypeDocument();
    virtual ~nsXULPrototypeDocument();
    nsresult Init();

    friend NS_IMETHODIMP
    NS_NewXULPrototypeDocument(nsXULPrototypeDocument** aResult);

    static uint32_t gRefCnt;
};

#endif // nsXULPrototypeDocument_h__
