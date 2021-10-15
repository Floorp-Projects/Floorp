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

nsresult HTMLMetaElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                       const nsAttrValue* aValue,
                                       const nsAttrValue* aOldValue,
                                       nsIPrincipal* aSubjectPrincipal,
                                       bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None) {
    if (Document* document = GetUncomposedDoc()) {
      if (aName == nsGkAtoms::content) {
        if (const nsAttrValue* name = GetParsedAttr(nsGkAtoms::name)) {
          MetaAddedOrChanged(*document, *name, FromChange::Yes);
        }
        CreateAndDispatchEvent(*document, u"DOMMetaChanged"_ns);
      } else if (aName == nsGkAtoms::name) {
        if (aOldValue) {
          MetaRemoved(*document, *aOldValue, FromChange::Yes);
        }
        if (aValue) {
          MetaAddedOrChanged(*document, *aValue, FromChange::Yes);
        }
        CreateAndDispatchEvent(*document, u"DOMMetaChanged"_ns);
      }
    }
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

  if (const nsAttrValue* name = GetParsedAttr(nsGkAtoms::name)) {
    MetaAddedOrChanged(doc, *name, FromChange::No);
  }
  CreateAndDispatchEvent(doc, u"DOMMetaAdded"_ns);
  return rv;
}

void HTMLMetaElement::UnbindFromTree(bool aNullParent) {
  if (Document* oldDoc = GetUncomposedDoc()) {
    if (const nsAttrValue* name = GetParsedAttr(nsGkAtoms::name)) {
      MetaRemoved(*oldDoc, *name, FromChange::No);
    }
    CreateAndDispatchEvent(*oldDoc, u"DOMMetaRemoved"_ns);
  }
  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

void HTMLMetaElement::CreateAndDispatchEvent(Document&,
                                             const nsAString& aEventName) {
  RefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(
      this, aEventName, CanBubble::eYes, ChromeOnlyDispatch::eYes);
  asyncDispatcher->RunDOMEventWhenSafe();
}

JSObject* HTMLMetaElement::WrapNode(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return HTMLMetaElement_Binding::Wrap(aCx, this, aGivenProto);
}

void HTMLMetaElement::MetaAddedOrChanged(Document& aDoc,
                                         const nsAttrValue& aName,
                                         FromChange aFromChange) {
  nsAutoString content;
  const bool hasContent = GetAttr(nsGkAtoms::content, content);
  if (aName.Equals(nsGkAtoms::viewport, eIgnoreCase)) {
    if (hasContent) {
      aDoc.SetMetaViewportData(MakeUnique<ViewportMetaData>(content));
    }
    return;
  }

  if (aName.Equals(nsGkAtoms::referrer, eIgnoreCase)) {
    content = nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
        content);
    return aDoc.UpdateReferrerInfoFromMeta(content,
                                           /* aPreload = */ false);
  }
}

void HTMLMetaElement::MetaRemoved(Document& aDoc, const nsAttrValue& aName,
                                  FromChange aFromChange) {
  // TODO(emilio): We probably want to deal with <meta name=color-scheme> and co
  // here.
}

}  // namespace mozilla::dom
