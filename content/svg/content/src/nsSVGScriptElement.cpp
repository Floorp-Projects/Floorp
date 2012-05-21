/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGElement.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGScriptElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsCOMPtr.h"
#include "nsSVGString.h"
#include "nsIDocument.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsScriptElement.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

typedef nsSVGElement nsSVGScriptElementBase;

class nsSVGScriptElement : public nsSVGScriptElementBase,
                           public nsIDOMSVGScriptElement, 
                           public nsIDOMSVGURIReference,
                           public nsScriptElement
{
protected:
  friend nsresult NS_NewSVGScriptElement(nsIContent **aResult,
                                         already_AddRefed<nsINodeInfo> aNodeInfo,
                                         FromParser aFromParser);
  nsSVGScriptElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                     FromParser aFromParser);
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSCRIPTELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE(nsSVGScriptElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGScriptElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGScriptElementBase::)

  // nsIScriptElement
  virtual void GetScriptType(nsAString& type);
  virtual void GetScriptText(nsAString& text);
  virtual void GetScriptCharset(nsAString& charset);
  virtual void FreezeUriAsyncDefer();
  virtual CORSMode GetCORSMode() const;
  
  // nsScriptElement
  virtual bool HasScriptContent();

  // nsIContent specializations:
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify);
  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  virtual StringAttributesInfo GetStringInfo();

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

nsSVGElement::StringInfo nsSVGScriptElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, false }
};

NS_IMPL_NS_NEW_SVG_ELEMENT_CHECK_PARSER(Script)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGScriptElement,nsSVGScriptElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGScriptElement,nsSVGScriptElementBase)

DOMCI_NODE_DATA(SVGScriptElement, nsSVGScriptElement)

NS_INTERFACE_TABLE_HEAD(nsSVGScriptElement)
  NS_NODE_INTERFACE_TABLE8(nsSVGScriptElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGScriptElement,
                           nsIDOMSVGURIReference, nsIScriptLoaderObserver,
                           nsIScriptElement, nsIMutationObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGScriptElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGScriptElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGScriptElement::nsSVGScriptElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                       FromParser aFromParser)
  : nsSVGScriptElementBase(aNodeInfo)
  , nsScriptElement(aFromParser)
{
  AddMutationObserver(this);
}

//----------------------------------------------------------------------
// nsIDOMNode methods

nsresult
nsSVGScriptElement::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nsnull;

  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  nsSVGScriptElement* it = new nsSVGScriptElement(ni.forget(), NOT_FROM_PARSER);

  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv = it->Init();
  rv |= CopyInnerTo(it);
  NS_ENSURE_SUCCESS(rv, rv);

  // The clone should be marked evaluated if we are.
  it->mAlreadyStarted = mAlreadyStarted;
  it->mLineNumber = mLineNumber;
  it->mMalformed = mMalformed;

  kungFuDeathGrip.swap(*aResult);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGScriptElement methods

/* attribute DOMString type; */
NS_IMPL_STRING_ATTR(nsSVGScriptElement, Type, type)
/* attribute DOMString crossOrigin */
NS_IMPL_STRING_ATTR(nsSVGScriptElement, CrossOrigin, crossorigin)

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGScriptElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  return mStringAttributes[HREF].ToDOMAnimatedString(aHref, this);
}

//----------------------------------------------------------------------
// nsIScriptElement methods

void
nsSVGScriptElement::GetScriptType(nsAString& type)
{
  GetType(type);
}

void
nsSVGScriptElement::GetScriptText(nsAString& text)
{
  nsContentUtils::GetNodeTextContent(this, false, text);
}

void
nsSVGScriptElement::GetScriptCharset(nsAString& charset)
{
  charset.Truncate();
}

void
nsSVGScriptElement::FreezeUriAsyncDefer()
{
  if (mFrozen) {
    return;
  }

  if (mStringAttributes[HREF].IsExplicitlySet()) {
    // variation of this code in nsHTMLScriptElement - check if changes
    // need to be transfered when modifying
    nsAutoString src;
    mStringAttributes[HREF].GetAnimValue(src, this);

    nsCOMPtr<nsIURI> baseURI = GetBaseURI();
    NS_NewURI(getter_AddRefs(mUri), src, nsnull, baseURI);
    // At this point mUri will be null for invalid URLs.
    mExternal = true;
  }
  
  mFrozen = true;
}

//----------------------------------------------------------------------
// nsScriptElement methods

bool
nsSVGScriptElement::HasScriptContent()
{
  return (mFrozen ? mExternal : mStringAttributes[HREF].IsExplicitlySet()) ||
         nsContentUtils::HasNonEmptyTextContent(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
nsSVGScriptElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//----------------------------------------------------------------------
// nsIContent methods

nsresult
nsSVGScriptElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers)
{
  nsresult rv = nsSVGScriptElementBase::BindToTree(aDocument, aParent,
                                                   aBindingParent,
                                                   aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
    MaybeProcessScript();
  }

  return NS_OK;
}

nsresult
nsSVGScriptElement::AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                 const nsAttrValue* aValue, bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_XLink && aName == nsGkAtoms::href) {
    MaybeProcessScript();
  }
  return nsSVGScriptElementBase::AfterSetAttr(aNamespaceID, aName,
                                              aValue, aNotify);
}

bool
nsSVGScriptElement::ParseAttribute(PRInt32 aNamespaceID,
                                   nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::crossorigin) {
    ParseCORSValue(aValue, aResult);
    return true;
  }

  return nsSVGScriptElementBase::ParseAttribute(aNamespaceID, aAttribute,
                                                aValue, aResult);
}

CORSMode
nsSVGScriptElement::GetCORSMode() const
{
  return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));
}

