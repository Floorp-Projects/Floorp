/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAttrValue.h"
#include "nsAttrValueOrString.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "mozilla/dom/Document.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"  // for nsCaseInsensitiveStringComparator()
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsServiceManagerUtils.h"
#include "nsError.h"
#include "nsTArray.h"
#include "nsDOMJSUtils.h"
#include "nsIScriptError.h"
#include "nsISupportsImpl.h"
#include "mozilla/dom/FetchPriority.h"
#include "mozilla/dom/HTMLScriptElement.h"
#include "mozilla/dom/HTMLScriptElementBinding.h"
#include "mozilla/Assertions.h"
#include "mozilla/StaticPrefs_dom.h"

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Script)

using JS::loader::ScriptKind;

namespace mozilla::dom {

JSObject* HTMLScriptElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLScriptElement_Binding::Wrap(aCx, this, aGivenProto);
}

HTMLScriptElement::HTMLScriptElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    FromParser aFromParser)
    : nsGenericHTMLElement(std::move(aNodeInfo)), ScriptElement(aFromParser) {
  AddMutationObserver(this);
}

HTMLScriptElement::~HTMLScriptElement() = default;

NS_IMPL_ISUPPORTS_INHERITED(HTMLScriptElement, nsGenericHTMLElement,
                            nsIScriptLoaderObserver, nsIScriptElement,
                            nsIMutationObserver)

nsresult HTMLScriptElement::BindToTree(BindContext& aContext,
                                       nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsInComposedDoc()) {
    MaybeProcessScript();
  }

  return NS_OK;
}

bool HTMLScriptElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                       const nsAString& aValue,
                                       nsIPrincipal* aMaybeScriptedPrincipal,
                                       nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::crossorigin) {
      ParseCORSValue(aValue, aResult);
      return true;
    }

    if (aAttribute == nsGkAtoms::integrity) {
      aResult.ParseStringOrAtom(aValue);
      return true;
    }

    if (aAttribute == nsGkAtoms::fetchpriority) {
      ParseFetchPriority(aValue, aResult);
      return true;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

nsresult HTMLScriptElement::Clone(dom::NodeInfo* aNodeInfo,
                                  nsINode** aResult) const {
  *aResult = nullptr;

  HTMLScriptElement* it = new (aNodeInfo->NodeInfoManager())
      HTMLScriptElement(do_AddRef(aNodeInfo), NOT_FROM_PARSER);

  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv = const_cast<HTMLScriptElement*>(this)->CopyInnerTo(it);
  NS_ENSURE_SUCCESS(rv, rv);

  // The clone should be marked evaluated if we are.
  it->mAlreadyStarted = mAlreadyStarted;
  it->mLineNumber = mLineNumber;
  it->mMalformed = mMalformed;

  kungFuDeathGrip.swap(*aResult);

  return NS_OK;
}

void HTMLScriptElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                     const nsAttrValue* aValue,
                                     const nsAttrValue* aOldValue,
                                     nsIPrincipal* aMaybeScriptedPrincipal,
                                     bool aNotify) {
  if (nsGkAtoms::async == aName && kNameSpaceID_None == aNamespaceID) {
    mForceAsync = false;
  }
  if (nsGkAtoms::src == aName && kNameSpaceID_None == aNamespaceID) {
    mSrcTriggeringPrincipal = nsContentUtils::GetAttrTriggeringPrincipal(
        this, aValue ? aValue->GetStringValue() : EmptyString(),
        aMaybeScriptedPrincipal);
  }
  return nsGenericHTMLElement::AfterSetAttr(
      aNamespaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

void HTMLScriptElement::GetInnerHTML(nsAString& aInnerHTML,
                                     OOMReporter& aError) {
  if (!nsContentUtils::GetNodeTextContent(this, false, aInnerHTML, fallible)) {
    aError.ReportOOM();
  }
}

void HTMLScriptElement::SetInnerHTML(const nsAString& aInnerHTML,
                                     nsIPrincipal* aScriptedPrincipal,
                                     ErrorResult& aError) {
  aError = nsContentUtils::SetNodeTextContent(this, aInnerHTML, true);
}

void HTMLScriptElement::GetText(nsAString& aValue, ErrorResult& aRv) const {
  if (!nsContentUtils::GetNodeTextContent(this, false, aValue, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
  }
}

void HTMLScriptElement::SetText(const nsAString& aValue, ErrorResult& aRv) {
  aRv = nsContentUtils::SetNodeTextContent(this, aValue, true);
}

// variation of this code in SVGScriptElement - check if changes
// need to be transfered when modifying

void HTMLScriptElement::GetScriptText(nsAString& text) const {
  GetText(text, IgnoreErrors());
}

void HTMLScriptElement::GetScriptCharset(nsAString& charset) {
  GetCharset(charset);
}

void HTMLScriptElement::FreezeExecutionAttrs(const Document* aOwnerDoc) {
  if (mFrozen) {
    return;
  }

  // Determine whether this is a(n) classic/module/importmap script.
  DetermineKindFromType(aOwnerDoc);

  // variation of this code in SVGScriptElement - check if changes
  // need to be transfered when modifying.  Note that we don't use GetSrc here
  // because it will return the base URL when the attr value is "".
  nsAutoString src;
  if (GetAttr(nsGkAtoms::src, src)) {
    // Empty src should be treated as invalid URL.
    if (!src.IsEmpty()) {
      nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(mUri), src,
                                                OwnerDoc(), GetBaseURI());

      if (!mUri) {
        AutoTArray<nsString, 2> params = {u"src"_ns, src};

        nsContentUtils::ReportToConsole(
            nsIScriptError::warningFlag, "HTML"_ns, OwnerDoc(),
            nsContentUtils::eDOM_PROPERTIES, "ScriptSourceInvalidUri", params,
            nullptr, u""_ns, GetScriptLineNumber(),
            GetScriptColumnNumber().oneOriginValue());
      }
    } else {
      AutoTArray<nsString, 1> params = {u"src"_ns};

      nsContentUtils::ReportToConsole(
          nsIScriptError::warningFlag, "HTML"_ns, OwnerDoc(),
          nsContentUtils::eDOM_PROPERTIES, "ScriptSourceEmpty", params, nullptr,
          u""_ns, GetScriptLineNumber(),
          GetScriptColumnNumber().oneOriginValue());
    }

    // At this point mUri will be null for invalid URLs.
    mExternal = true;
  }

  bool async = (mExternal || mKind == ScriptKind::eModule) && Async();
  bool defer = mExternal && Defer();

  mDefer = !async && defer;
  mAsync = async;

  mFrozen = true;
}

CORSMode HTMLScriptElement::GetCORSMode() const {
  return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));
}

FetchPriority HTMLScriptElement::GetFetchPriority() const {
  return nsGenericHTMLElement::GetFetchPriority();
}

mozilla::dom::ReferrerPolicy HTMLScriptElement::GetReferrerPolicy() {
  return GetReferrerPolicyAsEnum();
}

bool HTMLScriptElement::HasScriptContent() {
  return (mFrozen ? mExternal : HasAttr(nsGkAtoms::src)) ||
         nsContentUtils::HasNonEmptyTextContent(this);
}

// https://html.spec.whatwg.org/multipage/scripting.html#dom-script-supports
/* static */
bool HTMLScriptElement::Supports(const GlobalObject& aGlobal,
                                 const nsAString& aType) {
  nsAutoString type(aType);
  return aType.EqualsLiteral("classic") || aType.EqualsLiteral("module") ||

         aType.EqualsLiteral("importmap");
}

}  // namespace mozilla::dom
