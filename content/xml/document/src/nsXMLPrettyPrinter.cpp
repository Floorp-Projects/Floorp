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
#include "nsContentUtils.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIObserver.h"
#include "nsIXSLTProcessor.h"
#include "nsSyncLoadService.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "mozilla/dom/Element.h"
#include "nsIDOMDocumentFragment.h"
#include "nsBindingManager.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS2(nsXMLPrettyPrinter,
                   nsIDocumentObserver,
                   nsIMutationObserver)

nsXMLPrettyPrinter::nsXMLPrettyPrinter() : mDocument(nsnull),
                                           mUpdateDepth(0),
                                           mUnhookPending(PR_FALSE)
{
}

nsXMLPrettyPrinter::~nsXMLPrettyPrinter()
{
    NS_ASSERTION(!mDocument, "we shouldn't be referencing the document still");
}

nsresult
nsXMLPrettyPrinter::PrettyPrint(nsIDocument* aDocument,
                                bool* aDidPrettyPrint)
{
    *aDidPrettyPrint = PR_FALSE;
    
    // Check for iframe with display:none. Such iframes don't have presshells
    if (!aDocument->GetShell()) {
        return NS_OK;
    }

    // check if we're in an invisible iframe
    nsPIDOMWindow *internalWin = aDocument->GetWindow();
    nsCOMPtr<nsIDOMElement> frameElem;
    if (internalWin) {
        internalWin->GetFrameElement(getter_AddRefs(frameElem));
    }

    if (frameElem) {
        nsCOMPtr<nsIDOMCSSStyleDeclaration> computedStyle;
        nsCOMPtr<nsIDOMDocument> frameOwnerDoc;
        frameElem->GetOwnerDocument(getter_AddRefs(frameOwnerDoc));
        if (frameOwnerDoc) {
            nsCOMPtr<nsIDOMWindow> window;
            frameOwnerDoc->GetDefaultView(getter_AddRefs(window));
            if (window) {
                window->GetComputedStyle(frameElem,
                                         EmptyString(),
                                         getter_AddRefs(computedStyle));
            }
        }

        if (computedStyle) {
            nsAutoString visibility;
            computedStyle->GetPropertyValue(NS_LITERAL_STRING("visibility"),
                                            visibility);
            if (!visibility.EqualsLiteral("visible")) {

                return NS_OK;
            }
        }
    }

    // check the pref
    if (!Preferences::GetBool("layout.xml.prettyprint", true)) {
        return NS_OK;
    }

    // Ok, we should prettyprint. Let's do it!
    *aDidPrettyPrint = PR_TRUE;
    nsresult rv = NS_OK;

    // Load the XSLT
    nsCOMPtr<nsIURI> xslUri;
    rv = NS_NewURI(getter_AddRefs(xslUri),
                   NS_LITERAL_CSTRING("chrome://global/content/xml/XMLPrettyPrint.xsl"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMDocument> xslDocument;
    rv = nsSyncLoadService::LoadDocument(xslUri, nsnull, nsnull, PR_TRUE,
                                         getter_AddRefs(xslDocument));
    NS_ENSURE_SUCCESS(rv, rv);

    // Transform the document
    nsCOMPtr<nsIXSLTProcessor> transformer =
        do_CreateInstance("@mozilla.org/document-transformer;1?type=xslt", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = transformer->ImportStylesheet(xslDocument);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMDocumentFragment> resultFragment;
    nsCOMPtr<nsIDOMDocument> sourceDocument = do_QueryInterface(aDocument);
    rv = transformer->TransformToFragment(sourceDocument, sourceDocument,
                                          getter_AddRefs(resultFragment));
    NS_ENSURE_SUCCESS(rv, rv);

    // Add the binding
    nsCOMPtr<nsIDOMDocumentXBL> xblDoc = do_QueryInterface(aDocument);
    NS_ASSERTION(xblDoc, "xml document doesn't implement nsIDOMDocumentXBL");
    NS_ENSURE_TRUE(xblDoc, NS_ERROR_FAILURE);

    nsCOMPtr<nsIURI> bindingUri;
    rv = NS_NewURI(getter_AddRefs(bindingUri),
        NS_LITERAL_STRING("chrome://global/content/xml/XMLPrettyPrint.xml#prettyprint"));
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsCOMPtr<nsIPrincipal> sysPrincipal;
    nsContentUtils::GetSecurityManager()->
        GetSystemPrincipal(getter_AddRefs(sysPrincipal));
    aDocument->BindingManager()->LoadBindingDocument(aDocument, bindingUri,
                                                     sysPrincipal);

    nsCOMPtr<nsIContent> rootCont = aDocument->GetRootElement();
    NS_ENSURE_TRUE(rootCont, NS_ERROR_UNEXPECTED);

    rv = aDocument->BindingManager()->AddLayeredBinding(rootCont, bindingUri,
                                                        sysPrincipal);
    NS_ENSURE_SUCCESS(rv, rv);

    // Hand the result document to the binding
    nsCOMPtr<nsIObserver> binding;
    aDocument->BindingManager()->GetBindingImplementation(rootCont,
                                              NS_GET_IID(nsIObserver),
                                              (void**)getter_AddRefs(binding));
    NS_ASSERTION(binding, "Prettyprint binding doesn't implement nsIObserver");
    NS_ENSURE_TRUE(binding, NS_ERROR_UNEXPECTED);
    
    rv = binding->Observe(resultFragment, "prettyprint-dom-created",
                          EmptyString().get());
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
    // If there either aContent is null (the document-node was modified) or
    // there isn't a binding parent we know it's non-anonymous content.
    if (!aContent || !aContent->GetBindingParent() && !mUnhookPending) {
        // Can't blindly to mUnhookPending after AddScriptRunner,
        // since AddScriptRunner _could_ in theory run us
        // synchronously
        mUnhookPending = PR_TRUE;
        nsContentUtils::AddScriptRunner(
          NS_NewRunnableMethod(this, &nsXMLPrettyPrinter::Unhook));
    }
}

void
nsXMLPrettyPrinter::Unhook()
{
    mDocument->RemoveObserver(this);
    nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(mDocument);
    nsCOMPtr<nsIDOMElement> rootElem;
    document->GetDocumentElement(getter_AddRefs(rootElem));

    if (rootElem) {
        nsCOMPtr<nsIDOMDocumentXBL> xblDoc = do_QueryInterface(mDocument);
        xblDoc->RemoveBinding(rootElem,
                              NS_LITERAL_STRING("chrome://global/content/xml/XMLPrettyPrint.xml#prettyprint"));
    }

    mDocument = nsnull;

    NS_RELEASE_THIS();
}

void
nsXMLPrettyPrinter::AttributeChanged(nsIDocument* aDocument,
                                     Element* aElement,
                                     PRInt32 aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     PRInt32 aModType)
{
    MaybeUnhook(aElement);
}

void
nsXMLPrettyPrinter::ContentAppended(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aFirstNewContent,
                                    PRInt32 aNewIndexInContainer)
{
    MaybeUnhook(aContainer);
}

void
nsXMLPrettyPrinter::ContentInserted(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aChild,
                                    PRInt32 aIndexInContainer)
{
    MaybeUnhook(aContainer);
}

void
nsXMLPrettyPrinter::ContentRemoved(nsIDocument* aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild,
                                   PRInt32 aIndexInContainer,
                                   nsIContent* aPreviousSibling)
{
    MaybeUnhook(aContainer);
}

void
nsXMLPrettyPrinter::NodeWillBeDestroyed(const nsINode* aNode)
{
    mDocument = nsnull;
    NS_RELEASE_THIS();
}


nsresult NS_NewXMLPrettyPrinter(nsXMLPrettyPrinter** aPrinter)
{
    *aPrinter = new nsXMLPrettyPrinter;
    NS_ENSURE_TRUE(*aPrinter, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(*aPrinter);
    return NS_OK;
}
