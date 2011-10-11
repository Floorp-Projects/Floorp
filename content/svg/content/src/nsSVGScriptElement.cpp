/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
  
  // nsScriptElement
  virtual bool HasScriptContent();

  // nsSVGElement specializations:
  virtual void DidChangeString(PRUint8 aAttrEnum);

  // nsIContent specializations:
  virtual nsresult DoneAddingChildren(bool aHaveNotified);
  virtual bool IsDoneAddingChildren();
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual StringAttributesInfo GetStringInfo();

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

nsSVGElement::StringInfo nsSVGScriptElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, PR_FALSE }
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
NS_IMETHODIMP
nsSVGScriptElement::GetType(nsAString & aType)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::type, aType);

  return NS_OK;
}
NS_IMETHODIMP
nsSVGScriptElement::SetType(const nsAString & aType)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::type, aType, PR_TRUE); 
}

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
  nsContentUtils::GetNodeTextContent(this, PR_FALSE, text);
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

  // variation of this code in nsHTMLScriptElement - check if changes
  // need to be transfered when modifying
  nsAutoString src;
  mStringAttributes[HREF].GetAnimValue(src, this);
  // preserving bug 528444 here due to being unsure how to fix correctly
  if (!src.IsEmpty()) {
    nsCOMPtr<nsIURI> baseURI = GetBaseURI();
    NS_NewURI(getter_AddRefs(mUri), src, nsnull, baseURI);
    // At this point mUri will be null for invalid URLs.
    mExternal = PR_TRUE;
  }
  
  mFrozen = PR_TRUE;
}

//----------------------------------------------------------------------
// nsScriptElement methods

bool
nsSVGScriptElement::HasScriptContent()
{
  nsAutoString src;
  mStringAttributes[HREF].GetAnimValue(src, this);
  // preserving bug 528444 here due to being unsure how to fix correctly
  return (mFrozen ? mExternal : !src.IsEmpty()) ||
         nsContentUtils::HasNonEmptyTextContent(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

void
nsSVGScriptElement::DidChangeString(PRUint8 aAttrEnum)
{
  nsSVGScriptElementBase::DidChangeString(aAttrEnum);

  if (aAttrEnum == HREF) {
    MaybeProcessScript();
  }
}

nsSVGElement::StringAttributesInfo
nsSVGScriptElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//----------------------------------------------------------------------
// nsIContent methods

nsresult
nsSVGScriptElement::DoneAddingChildren(bool aHaveNotified)
{
  mDoneAddingChildren = PR_TRUE;
  nsresult rv = MaybeProcessScript();
  if (!mAlreadyStarted) {
    // Need to lose parser-insertedness here to allow another script to cause
    // execution later.
    LoseParserInsertedness();
  }
  return rv;
}

bool
nsSVGScriptElement::IsDoneAddingChildren()
{
  return mDoneAddingChildren;
}

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

