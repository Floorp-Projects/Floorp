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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc> (Original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 
#include "nsXMLPrettyPrinter.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMViewCSS.h"
#include "nsIPrefService.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIBindingManager.h"
#include "nsIObserver.h"
#include "nsIXSLTProcessor.h"
#include "nsISyncLoadDOMService.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsIContent.h"
#include "nsIDOMDocumentFragment.h"

NS_IMPL_ISUPPORTS1(nsXMLPrettyPrinter,
                   nsIDocumentObserver)

nsXMLPrettyPrinter::nsXMLPrettyPrinter() : mUnhookPending(PR_FALSE),
                                           mDocument(nsnull),
                                           mUpdateDepth(0)
{
    NS_INIT_ISUPPORTS();
}

nsXMLPrettyPrinter::~nsXMLPrettyPrinter()
{
    NS_ASSERTION(!mDocument, "we shouldn't be referencing the document still");
}

nsresult
nsXMLPrettyPrinter::PrettyPrint(nsIDocument* aDocument)
{
    // Check for iframe with display:none. Such iframes don't have presshells
    if (!aDocument->GetNumberOfShells()) {
        return NS_OK;
    }

    // check if we're in an invisible iframe
    nsCOMPtr<nsIScriptGlobalObject> sgo;
    aDocument->GetScriptGlobalObject(getter_AddRefs(sgo));
    nsCOMPtr<nsIDOMWindowInternal> internalWin = do_QueryInterface(sgo);
    nsCOMPtr<nsIDOMElement> frameElem;
    if (internalWin) {
        internalWin->GetFrameElement(getter_AddRefs(frameElem));
    }

    if (frameElem) {
        nsCOMPtr<nsIDOMCSSStyleDeclaration> computedStyle;
        nsCOMPtr<nsIDOMDocument> frameOwnerDoc;
        frameElem->GetOwnerDocument(getter_AddRefs(frameOwnerDoc));
        nsCOMPtr<nsIDOMDocumentView> docView = do_QueryInterface(frameOwnerDoc);
        if (docView) {
            nsCOMPtr<nsIDOMAbstractView> defaultView;
            docView->GetDefaultView(getter_AddRefs(defaultView));
            nsCOMPtr<nsIDOMViewCSS> defaultCSSView =
                do_QueryInterface(defaultView);
            if (defaultCSSView) {
                defaultCSSView->GetComputedStyle(frameElem,
                                                 NS_LITERAL_STRING(""),
                                                 getter_AddRefs(computedStyle));
            }
        }

        if (computedStyle) {
            nsAutoString visibility;
            computedStyle->GetPropertyValue(NS_LITERAL_STRING("visibility"),
                                            visibility);
            if (!visibility.Equals(NS_LITERAL_STRING("visible"))) {

                return NS_OK;
            }
        }
    }

    // check the pref
    nsCOMPtr<nsIPrefBranch> prefBranch =
        do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefBranch) {
        PRBool pref = PR_TRUE;
        prefBranch->GetBoolPref("layout.xml.prettyprint", &pref);
        if (!pref) {
            return NS_OK;
        }
    }


    // Ok, we should prettyprint. Let's do it!
    nsresult rv = NS_OK;

    // Load the XSLT
    nsCOMPtr<nsIURI> xslUri;
    rv = NS_NewURI(getter_AddRefs(xslUri),
                   NS_LITERAL_CSTRING("chrome://communicator/content/xml/XMLPrettyPrint.xsl"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), xslUri, nsnull, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMDocument> xslDocument;
    nsCOMPtr<nsISyncLoadDOMService> loader =
       do_GetService("@mozilla.org/content/syncload-dom-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = loader->LoadLocalDocument(channel, nsnull, getter_AddRefs(xslDocument));
    NS_ENSURE_SUCCESS(rv, rv);

    // Transform the document
    nsCOMPtr<nsIXSLTProcessor> transformer =
        do_CreateInstance("@mozilla.org/document-transformer;1?type=text/xsl", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = transformer->ImportStylesheet(xslDocument);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMDocumentFragment> resultFragment;
    nsCOMPtr<nsIDOMDocument> sourceDocument = do_QueryInterface(aDocument);
    rv = transformer->TransformToFragment(sourceDocument, xslDocument,
                                          getter_AddRefs(resultFragment));
    NS_ENSURE_SUCCESS(rv, rv);

    // Add the binding
    nsCOMPtr<nsIDOMDocumentXBL> xblDoc = do_QueryInterface(aDocument);
    NS_ASSERTION(xblDoc, "xml document doesn't implement nsIDOMDocumentXBL");
    NS_ENSURE_TRUE(xblDoc, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMDocument> dummy;
    xblDoc->LoadBindingDocument(NS_LITERAL_STRING("chrome://communicator/content/xml/XMLPrettyPrint.xml"),
                                getter_AddRefs(dummy));

    nsCOMPtr<nsIDOMElement> rootElem;
    sourceDocument->GetDocumentElement(getter_AddRefs(rootElem));
    NS_ENSURE_TRUE(rootElem, NS_ERROR_UNEXPECTED);

    rv = xblDoc->AddBinding(rootElem,
                            NS_LITERAL_STRING("chrome://communicator/content/xml/XMLPrettyPrint.xml#prettyprint"));
    NS_ENSURE_SUCCESS(rv, rv);

    // Hand the result document to the binding
    nsCOMPtr<nsIBindingManager> manager;
    aDocument->GetBindingManager(getter_AddRefs(manager));
    nsCOMPtr<nsIObserver> binding;
    nsCOMPtr<nsIContent> rootCont = do_QueryInterface(rootElem);
    NS_ASSERTION(rootCont, "Element doesn't implement nsIContent");
    manager->GetBindingImplementation(rootCont, NS_GET_IID(nsIObserver),
                                      (void**)getter_AddRefs(binding));
    NS_ASSERTION(binding, "Prettyprint binding doesn't implement nsIObserver");
    NS_ENSURE_TRUE(binding, NS_ERROR_UNEXPECTED);
    
    rv = binding->Observe(resultFragment, "prettyprint-dom-created", NS_LITERAL_STRING("").get());
    NS_ENSURE_SUCCESS(rv, rv);
    
    // Observe the document so we know when to switch to "normal" view
    aDocument->AddObserver(this);
    mDocument = aDocument;

    NS_ADDREF_THIS();

    return NS_OK;
}

void
nsXMLPrettyPrinter::MaybeUnhook(nsIContent* aContent)
{
    nsCOMPtr<nsIContent> bindingParent;
    if (aContent) {
        aContent->GetBindingParent(getter_AddRefs(bindingParent));
    }
    // If there either aContent is null (the document-node was modified) or
    // there isn't a binding parent we know it's non-anonymous content.
    if (!bindingParent) {
        mUnhookPending = PR_TRUE;
    }
}

// nsIDocumentObserver implementation
NS_IMETHODIMP
nsXMLPrettyPrinter::BeginUpdate(nsIDocument* aDocument)
{
    mUpdateDepth++;
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::EndUpdate(nsIDocument* aDocument)
{
    mUpdateDepth--;

    // Only remove the binding once we're outside all updates. This protects us
    // from nasty surprices of elements being removed from the document in the
    // midst of setting attributes etc.
    if (mUnhookPending && mUpdateDepth == 0) {
        mDocument->RemoveObserver(this);
        nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(mDocument);
        nsCOMPtr<nsIDOMElement> rootElem;
        document->GetDocumentElement(getter_AddRefs(rootElem));
        nsCOMPtr<nsIDOMDocumentXBL> xblDoc = do_QueryInterface(mDocument);
        xblDoc->RemoveBinding(rootElem,
                              NS_LITERAL_STRING("chrome://communicator/content/xml/XMLPrettyPrint.xml#prettyprint"));

        mDocument = nsnull;

        NS_RELEASE_THIS();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::BeginLoad(nsIDocument* aDocument)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::EndLoad(nsIDocument* aDocument)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::BeginReflow(nsIDocument* aDocument,
                                nsIPresShell* aShell)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::EndReflow(nsIDocument* aDocument,
                              nsIPresShell* aShell)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::ContentChanged(nsIDocument* aDocument,
                                   nsIContent *aContent,
                                   nsISupports *aSubContent)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::ContentStatesChanged(nsIDocument* aDocument,
                                         nsIContent* aContent1,
                                         nsIContent* aContent2,
                                         PRInt32 aStateMask)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::AttributeChanged(nsIDocument* aDocument,
                                     nsIContent* aContent,
                                     PRInt32 aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     PRInt32 aModType,
                                     nsChangeHint aHint)
{
    MaybeUnhook(aContent);
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::ContentAppended(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    PRInt32 aNewIndexInContainer)
{
    MaybeUnhook(aContainer);
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::ContentInserted(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aChild,
                                    PRInt32 aIndexInContainer)
{
    MaybeUnhook(aContainer);
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::ContentReplaced(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aOldChild,
                                    nsIContent* aNewChild,
                                    PRInt32 aIndexInContainer)
{
    MaybeUnhook(aContainer);
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::ContentRemoved(nsIDocument* aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild,
                                   PRInt32 aIndexInContainer)
{
    MaybeUnhook(aContainer);
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::StyleSheetAdded(nsIDocument* aDocument,
                                    nsIStyleSheet* aStyleSheet)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::StyleSheetRemoved(nsIDocument* aDocument,
                                      nsIStyleSheet* aStyleSheet)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::StyleSheetDisabledStateChanged(nsIDocument* aDocument,
                                                   nsIStyleSheet* aStyleSheet,
                                                   PRBool aDisabled)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::StyleRuleChanged(nsIDocument* aDocument,
                                     nsIStyleSheet* aStyleSheet,
                                     nsIStyleRule* aStyleRule,
                                     nsChangeHint aHint)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::StyleRuleAdded(nsIDocument* aDocument,
                                   nsIStyleSheet* aStyleSheet,
                                   nsIStyleRule* aStyleRule)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::StyleRuleRemoved(nsIDocument* aDocument,
                                     nsIStyleSheet* aStyleSheet,
                                     nsIStyleRule* aStyleRule)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXMLPrettyPrinter::DocumentWillBeDestroyed(nsIDocument* aDocument)
{
    mDocument = nsnull;
    NS_RELEASE_THIS();

    return NS_OK;
}


nsresult NS_NewXMLPrettyPrinter(nsXMLPrettyPrinter** aPrinter)
{
    *aPrinter = new nsXMLPrettyPrinter;
    NS_ENSURE_TRUE(*aPrinter, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(*aPrinter);
    return NS_OK;
}
