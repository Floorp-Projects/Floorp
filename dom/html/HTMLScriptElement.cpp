/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "mozilla/dom/Document.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"  // for nsCaseInsensitiveStringComparator()
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPConnect.h"
#include "nsServiceManagerUtils.h"
#include "nsError.h"
#include "nsIArray.h"
#include "nsTArray.h"
#include "nsDOMJSUtils.h"
#include "nsIScriptError.h"
#include "nsISupportsImpl.h"
#include "mozilla/dom/HTMLScriptElement.h"
#include "mozilla/dom/HTMLScriptElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Script)

namespace mozilla {
namespace dom {

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

HTMLScriptElement::~HTMLScriptElement() {}

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
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

nsresult HTMLScriptElement::Clone(dom::NodeInfo* aNodeInfo,
                                  nsINode** aResult) const {
  *aResult = nullptr;

  HTMLScriptElement* it =
      new HTMLScriptElement(do_AddRef(aNodeInfo), NOT_FROM_PARSER);

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

nsresult HTMLScriptElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
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

void HTMLScriptElement::GetText(nsAString& aValue, ErrorResult& aRv) {
  if (!nsContentUtils::GetNodeTextContent(this, false, aValue, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
  }
}

void HTMLScriptElement::SetText(const nsAString& aValue, ErrorResult& aRv) {
  aRv = nsContentUtils::SetNodeTextContent(this, aValue, true);
}

// variation of this code in nsSVGScriptElement - check if changes
// need to be transfered when modifying

bool HTMLScriptElement::GetScriptType(nsAString& aType) {
  nsAutoString type;
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::type, type)) {
    return false;
  }

  // ASCII whitespace https://infra.spec.whatwg.org/#ascii-whitespace:
  // U+0009 TAB, U+000A LF, U+000C FF, U+000D CR, or U+0020 SPACE.
  static const char kASCIIWhitespace[] = "\t\n\f\r ";
  type.Trim(kASCIIWhitespace);

  aType.Assign(type);
  return true;
}

void HTMLScriptElement::GetScriptText(nsAString& text) {
  GetText(text, IgnoreErrors());
}

void HTMLScriptElement::GetScriptCharset(nsAString& charset) {
  GetCharset(charset);
}

void HTMLScriptElement::FreezeExecutionAttrs(Document* aOwnerDoc) {
  if (mFrozen) {
    return;
  }

  MOZ_ASSERT(!mIsModule && !mAsync && !mDefer && !mExternal);

  // Determine whether this is a classic script or a module script.
  nsAutoString type;
  GetScriptType(type);
  mIsModule = aOwnerDoc->ModuleScriptsEnabled() && !type.IsEmpty() &&
              type.LowerCaseEqualsASCII("module");

  // variation of this code in nsSVGScriptElement - check if changes
  // need to be transfered when modifying.  Note that we don't use GetSrc here
  // because it will return the base URL when the attr value is "".
  nsAutoString src;
  if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
    // Empty src should be treated as invalid URL.
    if (!src.IsEmpty()) {
      nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(mUri), src,
                                                OwnerDoc(), GetBaseURI());

      if (!mUri) {
        AutoTArray<nsString, 2> params = {NS_LITERAL_STRING("src"), src};

        nsContentUtils::ReportToConsole(
            nsIScriptError::warningFlag, NS_LITERAL_CSTRING("HTML"), OwnerDoc(),
            nsContentUtils::eDOM_PROPERTIES, "ScriptSourceInvalidUri", params,
            nullptr, EmptyString(), GetScriptLineNumber(),
            GetScriptColumnNumber());
      }
    } else {
      AutoTArray<nsString, 1> params = {NS_LITERAL_STRING("src")};

      nsContentUtils::ReportToConsole(
          nsIScriptError::warningFlag, NS_LITERAL_CSTRING("HTML"), OwnerDoc(),
          nsContentUtils::eDOM_PROPERTIES, "ScriptSourceEmpty", params, nullptr,
          EmptyString(), GetScriptLineNumber(), GetScriptColumnNumber());
    }

    // At this point mUri will be null for invalid URLs.
    mExternal = true;
  }

  bool async = (mExternal || mIsModule) && Async();
  bool defer = mExternal && Defer();

  mDefer = !async && defer;
  mAsync = async;

  mFrozen = true;
}

CORSMode HTMLScriptElement::GetCORSMode() const {
  return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));
}

mozilla::dom::ReferrerPolicy HTMLScriptElement::GetReferrerPolicy() {
  return GetReferrerPolicyAsEnum();
}

bool HTMLScriptElement::HasScriptContent() {
  return (mFrozen ? mExternal : HasAttr(kNameSpaceID_None, nsGkAtoms::src)) ||
         nsContentUtils::HasNonEmptyTextContent(this);
}

}  // namespace dom
}  // namespace mozilla
