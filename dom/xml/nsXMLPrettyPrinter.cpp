/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXMLPrettyPrinter.h"
#include "nsContentUtils.h"
#include "nsICSSDeclaration.h"
#include "nsIObserver.h"
#include "nsSyncLoadService.h"
#include "nsPIDOMWindow.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "mozilla/dom/Element.h"
#include "nsBindingManager.h"
#include "nsXBLService.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/Preferences.h"
#include "nsIDocument.h"
#include "nsVariant.h"
#include "mozilla/dom/CustomEvent.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/txMozillaXSLTProcessor.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsXMLPrettyPrinter,
                  nsIDocumentObserver,
                  nsIMutationObserver)

nsXMLPrettyPrinter::nsXMLPrettyPrinter() : mDocument(nullptr),
                                           mUnhookPending(false)
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
    *aDidPrettyPrint = false;

    // Check for iframe with display:none. Such iframes don't have presshells
    nsCOMPtr<nsIPresShell> shell = aDocument->GetShell();
    if (!shell) {
        return NS_OK;
    }

    // check if we're in an invisible iframe
    nsPIDOMWindowOuter *internalWin = aDocument->GetWindow();
    nsCOMPtr<Element> frameElem;
    if (internalWin) {
        frameElem = internalWin->GetFrameElementInternal();
    }

    if (frameElem) {
        nsCOMPtr<nsICSSDeclaration> computedStyle;
        if (nsIDocument* frameOwnerDoc = frameElem->OwnerDoc()) {
            nsPIDOMWindowOuter* window = frameOwnerDoc->GetDefaultView();
            if (window) {
                nsCOMPtr<nsPIDOMWindowInner> innerWindow =
                    window->GetCurrentInnerWindow();

                ErrorResult dummy;
                computedStyle = innerWindow->GetComputedStyle(*frameElem,
                                                              EmptyString(),
                                                              dummy);
                dummy.SuppressException();
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
    *aDidPrettyPrint = true;
    nsresult rv = NS_OK;

    // Load the XSLT
    nsCOMPtr<nsIURI> xslUri;
    rv = NS_NewURI(getter_AddRefs(xslUri),
                   NS_LITERAL_CSTRING("chrome://global/content/xml/XMLPrettyPrint.xsl"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocument> xslDocument;
    rv = nsSyncLoadService::LoadDocument(xslUri, nsIContentPolicy::TYPE_XSLT,
                                         nsContentUtils::GetSystemPrincipal(),
                                         nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                                         nullptr, true, mozilla::net::RP_Unset,
                                         getter_AddRefs(xslDocument));
    NS_ENSURE_SUCCESS(rv, rv);

    // Transform the document
    RefPtr<txMozillaXSLTProcessor> transformer = new txMozillaXSLTProcessor();
    ErrorResult err;
    transformer->ImportStylesheet(*xslDocument, err);
    if (NS_WARN_IF(err.Failed())) {
        return err.StealNSResult();
    }

    RefPtr<DocumentFragment> resultFragment =
        transformer->TransformToFragment(*aDocument, *aDocument, err);
    if (NS_WARN_IF(err.Failed())) {
        return err.StealNSResult();
    }

    // Find the root element
    RefPtr<Element> rootElement = aDocument->GetRootElement();
    NS_ENSURE_TRUE(rootElement, NS_ERROR_UNEXPECTED);

    if (nsContentUtils::IsShadowDOMEnabled()) {
        // Attach a closed shadow root on it.
        RefPtr<ShadowRoot> shadowRoot =
            rootElement->AttachShadowWithoutNameChecks(ShadowRootMode::Closed);

        // Append the document fragment to the shadow dom.
        shadowRoot->AppendChild(*resultFragment, err);
        if (NS_WARN_IF(err.Failed())) {
            return err.StealNSResult();
        }
    } else {
        //
        // Apply the prettprint XBL binding.
        //
        // We take some shortcuts here. In particular, we don't bother invoking the
        // contstructor (since the binding has no constructor), and we don't bother
        // calling LoadBindingDocument because it's a chrome:// URI and thus will get
        // sync loaded no matter what.
        //

        // Grab the XBL service.
        nsXBLService* xblService = nsXBLService::GetInstance();
        NS_ENSURE_TRUE(xblService, NS_ERROR_NOT_AVAILABLE);

        // Compute the binding URI.
        nsCOMPtr<nsIURI> bindingUri;
        rv = NS_NewURI(getter_AddRefs(bindingUri),
            NS_LITERAL_STRING("chrome://global/content/xml/XMLPrettyPrint.xml#prettyprint"));
        NS_ENSURE_SUCCESS(rv, rv);

        // Grab the system principal.
        nsCOMPtr<nsIPrincipal> sysPrincipal;
        nsContentUtils::GetSecurityManager()->
            GetSystemPrincipal(getter_AddRefs(sysPrincipal));

        // Destroy any existing frames before we unbind anonymous content.
        // Note that the shell might be Destroy'ed by now (see bug 1415541).
        if (!shell->IsDestroying()) {
            shell->DestroyFramesForAndRestyle(rootElement);
        }

        // Load the bindings.
        RefPtr<nsXBLBinding> unused;
        bool ignored;
        rv = xblService->LoadBindings(rootElement, bindingUri, sysPrincipal,
                                      getter_AddRefs(unused), &ignored);
        NS_ENSURE_SUCCESS(rv, rv);

        // Fire an event at the bound element to pass it |resultFragment|.
        RefPtr<CustomEvent> event =
          NS_NewDOMCustomEvent(rootElement, nullptr, nullptr);
        MOZ_ASSERT(event);
        AutoJSAPI jsapi;
        if (!jsapi.Init(event->GetParentObject())) {
            return NS_ERROR_UNEXPECTED;
        }
        JSContext* cx = jsapi.cx();
        JS::Rooted<JS::Value> detail(cx);
        if (!ToJSValue(cx, resultFragment, &detail)) {
            return NS_ERROR_UNEXPECTED;
        }
        event->InitCustomEvent(cx, NS_LITERAL_STRING("prettyprint-dom-created"),
                               /* bubbles = */ false, /* cancelable = */ false,
                               detail);

        event->SetTrusted(true);
        rootElement->DispatchEvent(*event, err);
        if (NS_WARN_IF(err.Failed())) {
            return err.StealNSResult();
        }
    }

    // Observe the document so we know when to switch to "normal" view
    aDocument->AddObserver(this);
    mDocument = aDocument;

    NS_ADDREF_THIS();

    return NS_OK;
}

void
nsXMLPrettyPrinter::MaybeUnhook(nsIContent* aContent)
{
    // If aContent is null, the document-node was modified.
    // If it is not null but in the shadow tree, the <scrollbar> NACs,
    // or the XBL binding, the change was in the generated content, and
    // it should be ignored.
    bool isGeneratedContent = !aContent ?
        false :
        aContent->GetBindingParent() || aContent->IsInShadowTree();

    if (!isGeneratedContent && !mUnhookPending) {
        // Can't blindly to mUnhookPending after AddScriptRunner,
        // since AddScriptRunner _could_ in theory run us
        // synchronously
        mUnhookPending = true;
        nsContentUtils::AddScriptRunner(NewRunnableMethod(
          "nsXMLPrettyPrinter::Unhook", this, &nsXMLPrettyPrinter::Unhook));
    }
}

void
nsXMLPrettyPrinter::Unhook()
{
    mDocument->RemoveObserver(this);
    nsCOMPtr<Element> element = mDocument->GetDocumentElement();

    if (element) {
        // Remove the shadow root
        element->UnattachShadow();

        // Remove the bound XBL binding
        mDocument->BindingManager()->ClearBinding(element);
    }

    mDocument = nullptr;

    NS_RELEASE_THIS();
}

void
nsXMLPrettyPrinter::AttributeChanged(Element* aElement,
                                     int32_t aNameSpaceID,
                                     nsAtom* aAttribute,
                                     int32_t aModType,
                                     const nsAttrValue* aOldValue)
{
    MaybeUnhook(aElement);
}

void
nsXMLPrettyPrinter::ContentAppended(nsIContent* aFirstNewContent)
{
    MaybeUnhook(aFirstNewContent->GetParent());
}

void
nsXMLPrettyPrinter::ContentInserted(nsIContent* aChild)
{
    MaybeUnhook(aChild->GetParent());
}

void
nsXMLPrettyPrinter::ContentRemoved(nsIContent* aChild,
                                   nsIContent* aPreviousSibling)
{
    MaybeUnhook(aChild->GetParent());
}

void
nsXMLPrettyPrinter::NodeWillBeDestroyed(const nsINode* aNode)
{
    mDocument = nullptr;
    NS_RELEASE_THIS();
}


nsresult NS_NewXMLPrettyPrinter(nsXMLPrettyPrinter** aPrinter)
{
    *aPrinter = new nsXMLPrettyPrinter;
    NS_ADDREF(*aPrinter);
    return NS_OK;
}
