/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXMLPrettyPrinter.h"
#include "nsContentUtils.h"
#include "nsICSSDeclaration.h"
#include "nsSyncLoadService.h"
#include "nsPIDOMWindow.h"
#include "nsNetUtil.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Document.h"
#include "nsVariant.h"
#include "mozilla/dom/CustomEvent.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/txMozillaXSLTProcessor.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsXMLPrettyPrinter, nsIDocumentObserver, nsIMutationObserver)

nsXMLPrettyPrinter::nsXMLPrettyPrinter()
    : mDocument(nullptr), mUnhookPending(false) {}

nsXMLPrettyPrinter::~nsXMLPrettyPrinter() {
  NS_ASSERTION(!mDocument, "we shouldn't be referencing the document still");
}

nsresult nsXMLPrettyPrinter::PrettyPrint(Document* aDocument,
                                         bool* aDidPrettyPrint) {
  *aDidPrettyPrint = false;

  // check the pref
  if (!Preferences::GetBool("layout.xml.prettyprint", true)) {
    return NS_OK;
  }

  // Find the root element
  RefPtr<Element> rootElement = aDocument->GetRootElement();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_UNEXPECTED);

  // nsXMLContentSink should not ask us to pretty print an XML doc that comes
  // with a CanAttachShadowDOM() == true root element, but just in case:
  if (rootElement->CanAttachShadowDOM()) {
    MOZ_DIAGNOSTIC_ASSERT(false, "We shouldn't be getting this root element");
    return NS_ERROR_UNEXPECTED;
  }

  // Ok, we should prettyprint. Let's do it!
  *aDidPrettyPrint = true;
  nsresult rv = NS_OK;

  // Load the XSLT
  nsCOMPtr<nsIURI> xslUri;
  rv = NS_NewURI(getter_AddRefs(xslUri),
                 "chrome://global/content/xml/XMLPrettyPrint.xsl"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Document> xslDocument;
  rv = nsSyncLoadService::LoadDocument(
      xslUri, nsIContentPolicy::TYPE_XSLT, nsContentUtils::GetSystemPrincipal(),
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL, nullptr,
      aDocument->CookieJarSettings(), true, ReferrerPolicy::_empty,
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

  // Attach an UA Widget Shadow Root on it.
  rootElement->AttachAndSetUAShadowRoot(Element::NotifyUAWidgetSetup::No);
  RefPtr<ShadowRoot> shadowRoot = rootElement->GetShadowRoot();
  MOZ_RELEASE_ASSERT(shadowRoot && shadowRoot->IsUAWidget(),
                     "There should be a UA Shadow Root here.");

  // Append the document fragment to the shadow dom.
  shadowRoot->AppendChild(*resultFragment, err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  // Observe the document so we know when to switch to "normal" view
  aDocument->AddObserver(this);
  mDocument = aDocument;

  NS_ADDREF_THIS();

  return NS_OK;
}

void nsXMLPrettyPrinter::MaybeUnhook(nsIContent* aContent) {
  // If aContent is null, the document-node was modified.
  // If it is not null but in the shadow tree or the <scrollbar> NACs,
  // the change was in the generated content, and it should be ignored.
  bool isGeneratedContent =
      aContent &&
      (aContent->IsInNativeAnonymousSubtree() || aContent->IsInShadowTree());

  if (!isGeneratedContent && !mUnhookPending) {
    // Can't blindly to mUnhookPending after AddScriptRunner,
    // since AddScriptRunner _could_ in theory run us
    // synchronously
    mUnhookPending = true;
    nsContentUtils::AddScriptRunner(NewRunnableMethod(
        "nsXMLPrettyPrinter::Unhook", this, &nsXMLPrettyPrinter::Unhook));
  }
}

void nsXMLPrettyPrinter::Unhook() {
  mDocument->RemoveObserver(this);
  nsCOMPtr<Element> element = mDocument->GetDocumentElement();

  if (element) {
    // Remove the shadow root
    element->UnattachShadow();
  }

  mDocument = nullptr;

  NS_RELEASE_THIS();
}

void nsXMLPrettyPrinter::AttributeChanged(Element* aElement,
                                          int32_t aNameSpaceID,
                                          nsAtom* aAttribute, int32_t aModType,
                                          const nsAttrValue* aOldValue) {
  MaybeUnhook(aElement);
}

void nsXMLPrettyPrinter::ContentAppended(nsIContent* aFirstNewContent) {
  MaybeUnhook(aFirstNewContent->GetParent());
}

void nsXMLPrettyPrinter::ContentInserted(nsIContent* aChild) {
  MaybeUnhook(aChild->GetParent());
}

void nsXMLPrettyPrinter::ContentRemoved(nsIContent* aChild,
                                        nsIContent* aPreviousSibling) {
  MaybeUnhook(aChild->GetParent());
}

void nsXMLPrettyPrinter::NodeWillBeDestroyed(const nsINode* aNode) {
  mDocument = nullptr;
  NS_RELEASE_THIS();
}

nsresult NS_NewXMLPrettyPrinter(nsXMLPrettyPrinter** aPrinter) {
  *aPrinter = new nsXMLPrettyPrinter;
  NS_ADDREF(*aPrinter);
  return NS_OK;
}
