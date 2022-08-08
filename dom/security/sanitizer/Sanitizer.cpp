/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BindingDeclarations.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/SanitizerBinding.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"
#include "nsTreeSanitizer.h"
#include "Sanitizer.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Sanitizer, mGlobal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Sanitizer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Sanitizer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Sanitizer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* Sanitizer::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return Sanitizer_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<Sanitizer> Sanitizer::Constructor(
    const GlobalObject& aGlobal, const SanitizerConfig& aOptions,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Sanitizer> sanitizer = new Sanitizer(global, aOptions);
  return sanitizer.forget();
}

/* static */
already_AddRefed<DocumentFragment> Sanitizer::InputToNewFragment(
    const mozilla::dom::DocumentFragmentOrDocument& aInput, ErrorResult& aRv) {
  // turns an DocumentFragmentOrDocument into a new DocumentFragment for
  // internal use with nsTreeSanitizer

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (!window || !window->GetDoc()) {
    // FIXME: Should we throw another exception?
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // We need to create a new docfragment based on the input
  // and can't use a live document (possibly with mutation observershandlers)
  nsAutoString innerHTML;
  if (aInput.IsDocumentFragment()) {
    RefPtr<DocumentFragment> inFragment = &aInput.GetAsDocumentFragment();
    inFragment->GetInnerHTML(innerHTML);
  } else if (aInput.IsDocument()) {
    RefPtr<Document> doc = &aInput.GetAsDocument();
    nsCOMPtr<Element> docElement = doc->GetDocumentElement();
    if (docElement) {
      docElement->GetInnerHTML(innerHTML, IgnoreErrors());
    }
  }
  if (innerHTML.IsEmpty()) {
    AutoTArray<nsString, 1> params = {};
    LogLocalizedString("SanitizerRcvdNoInput", params,
                       nsIScriptError::warningFlag);

    RefPtr<DocumentFragment> emptyFragment =
        window->GetDoc()->CreateDocumentFragment();
    return emptyFragment.forget();
  }
  // Create an inert HTML document, loaded as data.
  // this ensures we do not cause any requests.
  RefPtr<Document> emptyDoc =
      nsContentUtils::CreateInertHTMLDocument(window->GetDoc());
  if (!emptyDoc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  // We don't have a context element yet. let's create a mock HTML body element
  RefPtr<mozilla::dom::NodeInfo> info =
      emptyDoc->NodeInfoManager()->GetNodeInfo(
          nsGkAtoms::body, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);

  nsCOMPtr<nsINode> context = NS_NewHTMLBodyElement(
      info.forget(), mozilla::dom::FromParser::FROM_PARSER_FRAGMENT);
  RefPtr<DocumentFragment> fragment = nsContentUtils::CreateContextualFragment(
      context, innerHTML, true /* aPreventScriptExecution */, aRv);
  if (aRv.Failed()) {
    aRv.ThrowInvalidStateError("Could not parse input");
    return nullptr;
  }
  return fragment.forget();
}

already_AddRefed<DocumentFragment> Sanitizer::Sanitize(
    const mozilla::dom::DocumentFragmentOrDocument& aInput, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (!window || !window->GetDoc()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  RefPtr<DocumentFragment> fragment =
      Sanitizer::InputToNewFragment(aInput, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  mTreeSanitizer.Sanitize(fragment);
  return fragment.forget();
}

RefPtr<DocumentFragment> Sanitizer::SanitizeFragment(
    RefPtr<DocumentFragment> aFragment, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (!window || !window->GetDoc()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  // FIXME(freddyb)
  // (how) can we assert that the supplied doc is indeed inert?
  mTreeSanitizer.Sanitize(aFragment);
  return aFragment.forget();
}

/* ------ Logging ------ */

void Sanitizer::LogLocalizedString(const char* aName,
                                   const nsTArray<nsString>& aParams,
                                   uint32_t aFlags) {
  uint64_t innerWindowID = 0;
  bool isPrivateBrowsing = true;
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
  if (window && window->GetDoc()) {
    auto* doc = window->GetDoc();
    innerWindowID = doc->InnerWindowID();
    isPrivateBrowsing = nsContentUtils::IsInPrivateBrowsing(doc);
  }
  nsAutoString logMsg;
  nsContentUtils::FormatLocalizedString(nsContentUtils::eSECURITY_PROPERTIES,
                                        aName, aParams, logMsg);
  LogMessage(logMsg, aFlags, innerWindowID, isPrivateBrowsing);
}

/* static */
void Sanitizer::LogMessage(const nsAString& aMessage, uint32_t aFlags,
                           uint64_t aInnerWindowID, bool aFromPrivateWindow) {
  // Prepending 'Sanitizer' to the outgoing console message
  nsString message;
  message.AppendLiteral(u"Sanitizer: ");
  message.Append(aMessage);

  // Allow for easy distinction in devtools code.
  constexpr auto category = "Sanitizer"_ns;

  if (aInnerWindowID > 0) {
    // Send to content console
    nsContentUtils::ReportToConsoleByWindowID(message, aFlags, category,
                                              aInnerWindowID);
  } else {
    // Send to browser console
    nsContentUtils::LogSimpleConsoleError(message, category, aFromPrivateWindow,
                                          true /* from chrome context */,
                                          aFlags);
  }
}

}  // namespace mozilla::dom
