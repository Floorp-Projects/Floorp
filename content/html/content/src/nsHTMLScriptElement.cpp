/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIDocument.h"
#include "nsScriptElement.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"  // for nsCaseInsensitiveStringComparator()
#include "jsapi.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPConnect.h"
#include "nsServiceManagerUtils.h"
#include "nsIDOMDocument.h"
#include "nsContentErrors.h"
#include "nsIArray.h"
#include "nsTArray.h"
#include "nsDOMJSUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

class nsHTMLScriptElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLScriptElement,
                            public nsScriptElement
{
public:
  using nsGenericElement::GetText;
  using nsGenericElement::SetText;

  nsHTMLScriptElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                      FromParser aFromParser);
  virtual ~nsHTMLScriptElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_BASIC(nsGenericHTMLElement::)
  NS_SCRIPTABLE NS_IMETHOD Click() {
    return nsGenericHTMLElement::Click();
  }
  NS_SCRIPTABLE NS_IMETHOD GetTabIndex(PRInt32* aTabIndex) {
    return nsGenericHTMLElement::GetTabIndex(aTabIndex);
  }
  NS_SCRIPTABLE NS_IMETHOD SetTabIndex(PRInt32 aTabIndex) {
    return nsGenericHTMLElement::SetTabIndex(aTabIndex);
  }
  NS_SCRIPTABLE NS_IMETHOD Focus() {
    return nsGenericHTMLElement::Focus();
  }
  NS_SCRIPTABLE NS_IMETHOD GetDraggable(bool* aDraggable) {
    return nsGenericHTMLElement::GetDraggable(aDraggable);
  }
  NS_SCRIPTABLE NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML);
  NS_SCRIPTABLE NS_IMETHOD SetInnerHTML(const nsAString& aInnerHTML);

  // nsIDOMHTMLScriptElement
  NS_DECL_NSIDOMHTMLSCRIPTELEMENT

  // nsIScriptElement
  virtual void GetScriptType(nsAString& type);
  virtual void GetScriptText(nsAString& text);
  virtual void GetScriptCharset(nsAString& charset);
  virtual void FreezeUriAsyncDefer();
  virtual CORSMode GetCORSMode() const;

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsGenericElement
  virtual nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify);

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  // nsScriptElement
  virtual bool HasScriptContent();
};


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Script)


nsHTMLScriptElement::nsHTMLScriptElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                         FromParser aFromParser)
  : nsGenericHTMLElement(aNodeInfo)
  , nsScriptElement(aFromParser)
{
  AddMutationObserver(this);
}

nsHTMLScriptElement::~nsHTMLScriptElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLScriptElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLScriptElement, nsGenericElement)

DOMCI_NODE_DATA(HTMLScriptElement, nsHTMLScriptElement)

// QueryInterface implementation for nsHTMLScriptElement
NS_INTERFACE_TABLE_HEAD(nsHTMLScriptElement)
  NS_HTML_CONTENT_INTERFACE_TABLE4(nsHTMLScriptElement,
                                   nsIDOMHTMLScriptElement,
                                   nsIScriptLoaderObserver,
                                   nsIScriptElement,
                                   nsIMutationObserver)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLScriptElement,
                                               nsGenericHTMLElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(HTMLScriptElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLScriptElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
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
nsHTMLScriptElement::ParseAttribute(PRInt32 aNamespaceID,
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
nsHTMLScriptElement::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nsnull;

  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  nsHTMLScriptElement* it =
    new nsHTMLScriptElement(ni.forget(), NOT_FROM_PARSER);

  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv = CopyInnerTo(it);
  NS_ENSURE_SUCCESS(rv, rv);

  // The clone should be marked evaluated if we are.
  it->mAlreadyStarted = mAlreadyStarted;
  it->mLineNumber = mLineNumber;
  it->mMalformed = mMalformed;

  kungFuDeathGrip.swap(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::GetText(nsAString& aValue)
{
  nsContentUtils::GetNodeTextContent(this, false, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScriptElement::SetText(const nsAString& aValue)
{
  return nsContentUtils::SetNodeTextContent(this, aValue, true);
}


NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Charset, charset)
NS_IMPL_BOOL_ATTR(nsHTMLScriptElement, Defer, defer)
NS_IMPL_URI_ATTR(nsHTMLScriptElement, Src, src)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, HtmlFor, _for)
NS_IMPL_STRING_ATTR(nsHTMLScriptElement, Event, event)

nsresult
nsHTMLScriptElement::GetAsync(bool* aValue)
{
  if (mForceAsync) {
    *aValue = true;
    return NS_OK;
  }
  return GetBoolAttr(nsGkAtoms::async, aValue);
}

nsresult
nsHTMLScriptElement::SetAsync(bool aValue)
{
  mForceAsync = false;
  return SetBoolAttr(nsGkAtoms::async, aValue);
}

nsresult
nsHTMLScriptElement::AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                  const nsAttrValue* aValue, bool aNotify)
{
  if (nsGkAtoms::async == aName && kNameSpaceID_None == aNamespaceID) {
    mForceAsync = false;
  }
  return nsGenericHTMLElement::AfterSetAttr(aNamespaceID, aName, aValue,
                                            aNotify);
}

nsresult
nsHTMLScriptElement::GetInnerHTML(nsAString& aInnerHTML)
{
  nsContentUtils::GetNodeTextContent(this, false, aInnerHTML);
  return NS_OK;
}

nsresult
nsHTMLScriptElement::SetInnerHTML(const nsAString& aInnerHTML)
{
  return nsContentUtils::SetNodeTextContent(this, aInnerHTML, true);
}

// variation of this code in nsSVGScriptElement - check if changes
// need to be transfered when modifying

void
nsHTMLScriptElement::GetScriptType(nsAString& type)
{
  GetType(type);
}

void
nsHTMLScriptElement::GetScriptText(nsAString& text)
{
  GetText(text);
}

void
nsHTMLScriptElement::GetScriptCharset(nsAString& charset)
{
  GetCharset(charset);
}

void
nsHTMLScriptElement::FreezeUriAsyncDefer()
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
nsHTMLScriptElement::GetCORSMode() const
{
  return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));
}

bool
nsHTMLScriptElement::HasScriptContent()
{
  return (mFrozen ? mExternal : HasAttr(kNameSpaceID_None, nsGkAtoms::src)) ||
         nsContentUtils::HasNonEmptyTextContent(this);
}
