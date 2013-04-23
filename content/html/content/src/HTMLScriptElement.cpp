/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
#include "jsapi.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPConnect.h"
#include "nsServiceManagerUtils.h"
#include "nsError.h"
#include "nsIArray.h"
#include "nsTArray.h"
#include "nsDOMJSUtils.h"
#include "mozilla/dom/HTMLScriptElement.h"
#include "mozilla/dom/HTMLScriptElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Script)

namespace mozilla {
namespace dom {

JSObject*
HTMLScriptElement::WrapNode(JSContext *aCx, JSObject *aScope)
{
  return HTMLScriptElementBinding::Wrap(aCx, aScope, this);
}

HTMLScriptElement::HTMLScriptElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                     FromParser aFromParser)
  : nsGenericHTMLElement(aNodeInfo)
  , nsScriptElement(aFromParser)
{
  SetIsDOMBinding();
  AddMutationObserver(this);
}

HTMLScriptElement::~HTMLScriptElement()
{
}


NS_IMPL_ADDREF_INHERITED(HTMLScriptElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLScriptElement, Element)

// QueryInterface implementation for HTMLScriptElement
NS_INTERFACE_TABLE_HEAD(HTMLScriptElement)
  NS_HTML_CONTENT_INTERFACE_TABLE4(HTMLScriptElement,
                                   nsIDOMHTMLScriptElement,
                                   nsIScriptLoaderObserver,
                                   nsIScriptElement,
                                   nsIMutationObserver)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLScriptElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
HTMLScriptElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
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
  if (aNamespaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::crossorigin) {
    ParseCORSValue(aValue, aResult);
    return true;
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

nsresult
HTMLScriptElement::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nullptr;

  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  HTMLScriptElement* it =
    new HTMLScriptElement(ni.forget(), NOT_FROM_PARSER);

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

NS_IMETHODIMP
HTMLScriptElement::GetText(nsAString& aValue)
{
  nsContentUtils::GetNodeTextContent(this, false, aValue);
  return NS_OK;
}

NS_IMETHODIMP
HTMLScriptElement::SetText(const nsAString& aValue)
{
  ErrorResult rv;
  SetText(aValue, rv);
  return rv.ErrorCode();
}

void
HTMLScriptElement::SetText(const nsAString& aValue, ErrorResult& rv)
{
  rv = nsContentUtils::SetNodeTextContent(this, aValue, true);
}


NS_IMPL_STRING_ATTR(HTMLScriptElement, Charset, charset)
NS_IMPL_BOOL_ATTR(HTMLScriptElement, Defer, defer)
NS_IMPL_URI_ATTR(HTMLScriptElement, Src, src)
NS_IMPL_STRING_ATTR(HTMLScriptElement, Type, type)
NS_IMPL_STRING_ATTR(HTMLScriptElement, HtmlFor, _for)
NS_IMPL_STRING_ATTR(HTMLScriptElement, Event, event)
NS_IMPL_STRING_ATTR(HTMLScriptElement, CrossOrigin, crossorigin)

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

void
HTMLScriptElement::SetCrossOrigin(const nsAString& aCrossOrigin, ErrorResult& rv)
{
  SetHTMLAttr(nsGkAtoms::crossorigin, aCrossOrigin, rv);
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
  return rv.ErrorCode();
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

void
HTMLScriptElement::GetInnerHTML(nsAString& aInnerHTML, ErrorResult& aError)
{
  nsContentUtils::GetNodeTextContent(this, false, aInnerHTML);
}

void
HTMLScriptElement::SetInnerHTML(const nsAString& aInnerHTML,
                                ErrorResult& aError)
{
  aError = nsContentUtils::SetNodeTextContent(this, aInnerHTML, true);
}

// variation of this code in nsSVGScriptElement - check if changes
// need to be transfered when modifying

void
HTMLScriptElement::GetScriptType(nsAString& type)
{
  GetType(type);
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
  // need to be transfered when modifying
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::src)) {
    nsAutoString src;
    GetSrc(src);
    NS_NewURI(getter_AddRefs(mUri), src);
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
