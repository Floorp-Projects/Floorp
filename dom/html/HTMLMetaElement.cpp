/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/HTMLMetaElement.h"
#include "mozilla/dom/HTMLMetaElementBinding.h"
#include "mozilla/dom/nsCSPService.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/ViewportMetaData.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_security.h"
#include "nsContentUtils.h"
#include "nsSandboxFlags.h"
#include "nsStyleConsts.h"
#include "nsIXMLContentSink.h"

static mozilla::LazyLogModule gMetaElementLog("nsMetaElement");
#define LOG(msg) MOZ_LOG(gMetaElementLog, mozilla::LogLevel::Debug, msg)
#define LOG_ENABLED() MOZ_LOG_TEST(gMetaElementLog, mozilla::LogLevel::Debug)

NS_IMPL_NS_NEW_HTML_ELEMENT(Meta)

namespace mozilla::dom {

HTMLMetaElement::HTMLMetaElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {}

HTMLMetaElement::~HTMLMetaElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLMetaElement)

void HTMLMetaElement::SetMetaReferrer(Document* aDocument) {
  if (!aDocument || !AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                 nsGkAtoms::referrer, eIgnoreCase)) {
    return;
  }
  nsAutoString content;
  GetContent(content);
  content =
      nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(content);
  aDocument->UpdateReferrerInfoFromMeta(content, false);
}

nsresult HTMLMetaElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                       const nsAttrValue* aValue,
                                       const nsAttrValue* aOldValue,
                                       nsIPrincipal* aSubjectPrincipal,
                                       bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None) {
    Document* document = GetUncomposedDoc();
    if (aName == nsGkAtoms::content) {
      if (document && AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                  nsGkAtoms::viewport, eIgnoreCase)) {
        ProcessViewportContent(document);
      }
      CreateAndDispatchEvent(document, u"DOMMetaChanged"_ns);
    } else if (document && aName == nsGkAtoms::name) {
      if (aValue && aValue->Equals(nsGkAtoms::viewport, eIgnoreCase)) {
        ProcessViewportContent(document);
      } else if (aOldValue &&
                 aOldValue->Equals(nsGkAtoms::viewport, eIgnoreCase)) {
        DiscardViewportContent(document);
      }
      CreateAndDispatchEvent(document, u"DOMMetaChanged"_ns);
    }
    // Update referrer policy when it got changed from JS
    SetMetaReferrer(document);
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

nsresult HTMLMetaElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!IsInUncomposedDoc()) {
    return rv;
  }
  Document& doc = aContext.OwnerDoc();

  bool shouldProcessMeta = true;
  // We don't want to call ProcessMETATag when we are pretty print
  // the document
  if (doc.IsXMLDocument()) {
    if (nsCOMPtr<nsIXMLContentSink> xmlSink =
            do_QueryInterface(doc.GetCurrentContentSink())) {
      if (xmlSink->IsPrettyPrintXML() &&
          xmlSink->IsPrettyPrintHasSpecialRoot()) {
        shouldProcessMeta = false;
      }
    }
  }

  if (shouldProcessMeta) {
    doc.ProcessMETATag(this);
  }

  if (AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, nsGkAtoms::viewport,
                  eIgnoreCase)) {
    ProcessViewportContent(&doc);
  }

  if (AttrValueIs(kNameSpaceID_None, nsGkAtoms::httpEquiv, nsGkAtoms::headerCSP,
                  eIgnoreCase)) {
    // only accept <meta http-equiv="Content-Security-Policy" content=""> if it
    // appears in the <head> element.
    Element* headElt = doc.GetHeadElement();
    if (headElt && IsInclusiveDescendantOf(headElt)) {
      nsAutoString content;
      GetContent(content);

      if (LOG_ENABLED()) {
        nsAutoCString documentURIspec;
        if (nsIURI* documentURI = doc.GetDocumentURI()) {
          documentURI->GetAsciiSpec(documentURIspec);
        }

        LOG(
            ("HTMLMetaElement %p sets CSP '%s' on document=%p, "
             "document-uri=%s",
             this, NS_ConvertUTF16toUTF8(content).get(), &doc,
             documentURIspec.get()));
      }
      CSP_ApplyMetaCSPToDoc(doc, content);
    }
  }

  SetMetaReferrer(&doc);
  CreateAndDispatchEvent(&doc, u"DOMMetaAdded"_ns);
  return rv;
}

void HTMLMetaElement::UnbindFromTree(bool aNullParent) {
  nsCOMPtr<Document> oldDoc = GetUncomposedDoc();
  if (oldDoc && AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                            nsGkAtoms::viewport, eIgnoreCase)) {
    DiscardViewportContent(oldDoc);
  }
  CreateAndDispatchEvent(oldDoc, u"DOMMetaRemoved"_ns);
  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

void HTMLMetaElement::CreateAndDispatchEvent(Document* aDoc,
                                             const nsAString& aEventName) {
  if (!aDoc) return;

  RefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(
      this, aEventName, CanBubble::eYes, ChromeOnlyDispatch::eYes);
  asyncDispatcher->RunDOMEventWhenSafe();
}

JSObject* HTMLMetaElement::WrapNode(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return HTMLMetaElement_Binding::Wrap(aCx, this, aGivenProto);
}

void HTMLMetaElement::ProcessViewportContent(Document* aDocument) {
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::content)) {
    // Call Document::RemoveMetaViewportElement for cases that the content
    // attribute is removed.
    // NOTE: RemoveMetaViewportElement enumerates all existing meta viewport
    // tags in the case where this element hasn't been there, i.e. this element
    // is newly added to the document, but it should be fine because a document
    // unlikely has a bunch of meta viewport tags.
    aDocument->RemoveMetaViewportElement(this);
    return;
  }

  nsAutoString content;
  GetContent(content);

  aDocument->SetHeaderData(nsGkAtoms::viewport, content);

  ViewportMetaData data(content);
  aDocument->AddMetaViewportElement(this, std::move(data));
}

void HTMLMetaElement::DiscardViewportContent(Document* aDocument) {
  aDocument->RemoveMetaViewportElement(this);
}

}  // namespace mozilla::dom
