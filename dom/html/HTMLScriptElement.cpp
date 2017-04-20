/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIDocument.h"
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

JSObject*
HTMLScriptElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLScriptElementBinding::Wrap(aCx, this, aGivenProto);
}

HTMLScriptElement::HTMLScriptElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                                     FromParser aFromParser)
  : nsGenericHTMLElement(aNodeInfo)
  , nsScriptElement(aFromParser)
{
  AddMutationObserver(this);
}

HTMLScriptElement::~HTMLScriptElement()
{
}

NS_IMPL_ISUPPORTS_INHERITED(HTMLScriptElement, nsGenericHTMLElement,
                            nsIDOMHTMLScriptElement,
                            nsIScriptLoaderObserver,
                            nsIScriptElement,
                            nsIMutationObserver)

nsresult
HTMLScriptElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (GetComposedDoc()) {
    MaybeProcessScript();
  }

  return NS_OK;
}

bool
HTMLScriptElement::ParseAttribute(int32_t aNamespaceID,
                                  nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
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
                                              aResult);
}

nsresult
HTMLScriptElement::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const
{
  *aResult = nullptr;

  already_AddRefed<mozilla::dom::NodeInfo> ni = RefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();
  HTMLScriptElement* it = new HTMLScriptElement(ni, NOT_FROM_PARSER);

  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv = const_cast<HTMLScriptElement*>(this)->CopyInnerTo(it, aPreallocateChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  // The clone should be marked evaluated if we are.
  it->mAlreadyStarted = mAlreadyStarted;
  it->mLineNumber = mLineNumber;
  it->mMalformed = mMalformed;

  kungFuDeathGrip.swap(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
HTMLScriptElement::GetText(nsAString& aValue)
{
  if (!nsContentUtils::GetNodeTextContent(this, false, aValue, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLScriptElement::SetText(const nsAString& aValue)
{
  ErrorResult rv;
  SetText(aValue, rv);
  return rv.StealNSResult();
}

void
HTMLScriptElement::SetText(const nsAString& aValue, ErrorResult& rv)
{
  rv = nsContentUtils::SetNodeTextContent(this, aValue, true);
}


NS_IMPL_STRING_ATTR(HTMLScriptElement, Charset, charset)
NS_IMPL_BOOL_ATTR(HTMLScriptElement, Defer, defer)
// If this ever gets changed to return "" if the attr value is "" (see
// https://github.com/whatwg/html/issues/1739 for why it might not get changed),
// it may be worth it to use GetSrc instead of GetAttr and manual
// NewURIWithDocumentCharset in FreezeUriAsyncDefer.
NS_IMPL_URI_ATTR(HTMLScriptElement, Src, src)
NS_IMPL_STRING_ATTR(HTMLScriptElement, Type, type)
NS_IMPL_STRING_ATTR(HTMLScriptElement, HtmlFor, _for)
NS_IMPL_STRING_ATTR(HTMLScriptElement, Event, event)

void
HTMLScriptElement::SetCharset(const nsAString& aCharset, ErrorResult& rv)
{
  SetHTMLAttr(nsGkAtoms::charset, aCharset, rv);
}

void
HTMLScriptElement::SetDefer(bool aDefer, ErrorResult& rv)
{
  SetHTMLBoolAttr(nsGkAtoms::defer, aDefer, rv);
}

bool
HTMLScriptElement::Defer()
{
  return GetBoolAttr(nsGkAtoms::defer);
}

void
HTMLScriptElement::SetSrc(const nsAString& aSrc, ErrorResult& rv)
{
  rv = SetAttrHelper(nsGkAtoms::src, aSrc);
}

void
HTMLScriptElement::SetType(const nsAString& aType, ErrorResult& rv)
{
  SetHTMLAttr(nsGkAtoms::type, aType, rv);
}

void
HTMLScriptElement::SetHtmlFor(const nsAString& aHtmlFor, ErrorResult& rv)
{
  SetHTMLAttr(nsGkAtoms::_for, aHtmlFor, rv);
}

void
HTMLScriptElement::SetEvent(const nsAString& aEvent, ErrorResult& rv)
{
  SetHTMLAttr(nsGkAtoms::event, aEvent, rv);
}

nsresult
HTMLScriptElement::GetAsync(bool* aValue)
{
  *aValue = Async();
  return NS_OK;
}

bool
HTMLScriptElement::Async()
{
  return mForceAsync || GetBoolAttr(nsGkAtoms::async);
}

nsresult
HTMLScriptElement::SetAsync(bool aValue)
{
  ErrorResult rv;
  SetAsync(aValue, rv);
  return rv.StealNSResult();
}

void
HTMLScriptElement::SetAsync(bool aValue, ErrorResult& rv)
{
  mForceAsync = false;
  SetHTMLBoolAttr(nsGkAtoms::async, aValue, rv);
}

nsresult
HTMLScriptElement::AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify)
{
  if (nsGkAtoms::async == aName && kNameSpaceID_None == aNamespaceID) {
    mForceAsync = false;
  }
  return nsGenericHTMLElement::AfterSetAttr(aNamespaceID, aName, aValue,
                                            aNotify);
}

NS_IMETHODIMP
HTMLScriptElement::GetInnerHTML(nsAString& aInnerHTML)
{
  if (!nsContentUtils::GetNodeTextContent(this, false, aInnerHTML, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

void
HTMLScriptElement::SetInnerHTML(const nsAString& aInnerHTML,
                                ErrorResult& aError)
{
  aError = nsContentUtils::SetNodeTextContent(this, aInnerHTML, true);
}

// variation of this code in nsSVGScriptElement - check if changes
// need to be transfered when modifying

bool
HTMLScriptElement::GetScriptType(nsAString& type)
{
  return GetAttr(kNameSpaceID_None, nsGkAtoms::type, type);
}

void
HTMLScriptElement::GetScriptText(nsAString& text)
{
  GetText(text);
}

void
HTMLScriptElement::GetScriptCharset(nsAString& charset)
{
  GetCharset(charset);
}

void
HTMLScriptElement::FreezeUriAsyncDefer()
{
  if (mFrozen) {
    return;
  }

  // variation of this code in nsSVGScriptElement - check if changes
  // need to be transfered when modifying.  Note that we don't use GetSrc here
  // because it will return the base URL when the attr value is "".
  nsAutoString src;
  if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
    // Empty src should be treated as invalid URL.
    if (!src.IsEmpty()) {
      nsCOMPtr<nsIURI> baseURI = GetBaseURI();
      nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(mUri),
                                                src, OwnerDoc(), baseURI);

      if (!mUri) {
        const char16_t* params[] = { u"src", src.get() };

        nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
          NS_LITERAL_CSTRING("HTML"), OwnerDoc(),
          nsContentUtils::eDOM_PROPERTIES, "ScriptSourceInvalidUri",
          params, ArrayLength(params), nullptr,
          EmptyString(), GetScriptLineNumber());
      }
    } else {
      const char16_t* params[] = { u"src" };

      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
        NS_LITERAL_CSTRING("HTML"), OwnerDoc(),
        nsContentUtils::eDOM_PROPERTIES, "ScriptSourceEmpty",
        params, ArrayLength(params), nullptr,
        EmptyString(), GetScriptLineNumber());
    }

    // At this point mUri will be null for invalid URLs.
    mExternal = true;

    bool defer, async;
    GetAsync(&async);
    GetDefer(&defer);

    mDefer = !async && defer;
    mAsync = async;
  }

  mFrozen = true;
}

CORSMode
HTMLScriptElement::GetCORSMode() const
{
  return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));
}

bool
HTMLScriptElement::HasScriptContent()
{
  return (mFrozen ? mExternal : HasAttr(kNameSpaceID_None, nsGkAtoms::src)) ||
         nsContentUtils::HasNonEmptyTextContent(this);
}

} // namespace dom
} // namespace mozilla
