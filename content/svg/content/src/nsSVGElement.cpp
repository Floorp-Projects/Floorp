/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 *    Alex Fritze <alex.fritze@crocodile-clips.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsSVGElement.h"
#include "nsIDocument.h"
#include "nsRange.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMMutationEvent.h"
#include "nsMutationEvent.h"
#include "nsIBindingManager.h"
#include "nsIXBLBinding.h"
#include "nsStyleConsts.h"
#include "nsDOMError.h"
#include "nsIPresShell.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIServiceManager.h"
#include "nsIXBLService.h"
#include "nsSVGAtoms.h"
#include "nsIStyleRule.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsRuleWalker.h"
#include "nsSVGStyleValue.h"

nsSVGElement::nsSVGElement()
    : mAttributes(nsnull)
{
}

nsSVGElement::~nsSVGElement()
{

  PRInt32 count = mChildren.Count();
  PRInt32 index;
  for (index = 0; index < count; index++) {
    nsIContent* kid = (nsIContent *)mChildren.ElementAt(index);
    kid->SetParent(nsnull);
    NS_RELEASE(kid);
  }

  if (mAttributes)
    NS_RELEASE(mAttributes);  
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGElement,nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsSVGElement,nsGenericElement)

NS_INTERFACE_MAP_BEGIN(nsSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIXMLContent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3Node, nsNode3Tearoff(this))
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
// provided by nsGenericElement:
//  NS_INTERFACE_MAP_ENTRY(nsIStyledContent)
//  NS_INTERFACE_MAP_ENTRY(nsIContent)
NS_INTERFACE_MAP_END_INHERITING(nsGenericElement)

//----------------------------------------------------------------------
// Implementation
  
nsresult
nsSVGElement::Init()
{
  nsresult rv;
  
  rv = nsSVGAttributes::Create(this,&mAttributes);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsINodeInfoManager> nimgr;  
  rv = mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr<nsINodeInfo> ni;

  // Create mapped properties:
  
  // style #IMPLIED
  rv = NS_NewSVGStyleValue(getter_AddRefs(mStyle));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = mAttributes->AddMappedSVGValue(nsSVGAtoms::style, mStyle);
  NS_ENSURE_SUCCESS(rv,rv);
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP
nsSVGElement::CanContainChildren(PRBool& aResult) const
{
  aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::ChildCount(PRInt32& aResult) const
{
  aResult = mChildren.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
  nsIContent *child = (nsIContent *)mChildren.SafeElementAt(aIndex);
  if (nsnull != child) {
    NS_ADDREF(child);
  }
  aResult = child;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const
{
  NS_PRECONDITION(nsnull != aPossibleChild, "null ptr");
  aResult = mChildren.IndexOf(aPossibleChild);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                            PRBool aNotify,
                            PRBool aDeepSetDocument)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  nsIDocument* doc = mDocument;
  if (aNotify && (nsnull != doc)) {
    doc->BeginUpdate();
  }
  PRBool rv = mChildren.InsertElementAt(aKid, aIndex);/* XXX fix up void array api to use nsresult's*/
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    nsRange::OwnerChildInserted(this, aIndex);
    if (nsnull != doc) {
      aKid->SetDocument(doc, aDeepSetDocument, PR_TRUE);
      if (aNotify) {
        doc->ContentInserted(this, aKid, aIndex);
      }

      if (nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
        nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(aKid));
        nsMutationEvent mutation;
        mutation.eventStructType = NS_MUTATION_EVENT;
        mutation.message = NS_MUTATION_NODEINSERTED;
        mutation.mTarget = node;

        nsCOMPtr<nsIDOMNode> relNode(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
        mutation.mRelatedNode = relNode;

        nsEventStatus status = nsEventStatus_eIgnore;
        nsCOMPtr<nsIDOMEvent> domEvent;
        aKid->HandleDOMEvent(nsnull, &mutation, getter_AddRefs(domEvent), NS_EVENT_FLAG_INIT, &status);
      }
    }
  }
  if (aNotify && (nsnull != doc)) {
    doc->EndUpdate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                             PRBool aNotify,
                             PRBool aDeepSetDocument)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  nsIDocument* doc = mDocument;
  if (aNotify && (nsnull != mDocument)) {
    doc->BeginUpdate();
  }
  nsIContent* oldKid = (nsIContent *)mChildren.ElementAt(aIndex);
  nsRange::OwnerChildReplaced(this, aIndex, oldKid);
  PRBool rv = mChildren.ReplaceElementAt(aKid, aIndex);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    if (nsnull != doc) {
      aKid->SetDocument(doc, aDeepSetDocument, PR_TRUE);
      if (aNotify) {
        doc->ContentReplaced(this, oldKid, aKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }
  if (aNotify && (nsnull != mDocument)) {
    doc->EndUpdate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                            PRBool aDeepSetDocument)
{
  NS_PRECONDITION(nsnull != aKid && this != aKid, "null ptr");
  nsIDocument* doc = mDocument;
  if (aNotify && (nsnull != doc)) {
    doc->BeginUpdate();
  }
  PRBool rv = mChildren.AppendElement(aKid);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    // ranges don't need adjustment since new child is at end of list
    if (nsnull != doc) {
      aKid->SetDocument(doc, aDeepSetDocument, PR_TRUE);
      if (aNotify) {
        doc->ContentAppended(this, mChildren.Count() - 1);
      }

      if (nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
        nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(aKid));
        nsMutationEvent mutation;
        mutation.eventStructType = NS_MUTATION_EVENT;
        mutation.message = NS_MUTATION_NODEINSERTED;
        mutation.mTarget = node;

        nsCOMPtr<nsIDOMNode> relNode(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
        mutation.mRelatedNode = relNode;

        nsEventStatus status = nsEventStatus_eIgnore;
        nsCOMPtr<nsIDOMEvent> domEvent;
        aKid->HandleDOMEvent(nsnull, &mutation, getter_AddRefs(domEvent), NS_EVENT_FLAG_INIT, &status);
      }
    }
  }
  if (aNotify && (nsnull != doc)) {
    doc->EndUpdate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
  nsIDocument* doc = mDocument;
  if (aNotify && (nsnull != doc)) {
    doc->BeginUpdate();
  }
  nsIContent* oldKid = (nsIContent *)mChildren.ElementAt(aIndex);
  if (nsnull != oldKid ) {

    if (nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEREMOVED)) {
      nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(oldKid));
      nsMutationEvent mutation;
      mutation.eventStructType = NS_MUTATION_EVENT;
      mutation.message = NS_MUTATION_NODEREMOVED;
      mutation.mTarget = node;

      nsCOMPtr<nsIDOMNode> relNode(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
      mutation.mRelatedNode = relNode;

      nsEventStatus status = nsEventStatus_eIgnore;
      nsCOMPtr<nsIDOMEvent> domEvent;
      oldKid->HandleDOMEvent(nsnull, &mutation, getter_AddRefs(domEvent),
                             NS_EVENT_FLAG_INIT, &status);
    }

    nsRange::OwnerChildRemoved(this, aIndex, oldKid);

    mChildren.RemoveElementAt(aIndex);
    if (aNotify) {
      if (nsnull != doc) {
        doc->ContentRemoved(this, oldKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }
  if (aNotify && (nsnull != doc)) {
    doc->EndUpdate();
  }

  return NS_OK;  
}

NS_IMETHODIMP
nsSVGElement::NormalizeAttrString(const nsAString& aStr,
                                       nsINodeInfo*& aNodeInfo)
{
  return mAttributes->NormalizeAttrString(aStr, aNodeInfo);
}

NS_IMETHODIMP
nsSVGElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                      const nsAString& aValue,
                      PRBool aNotify)
{
    nsCOMPtr<nsINodeInfoManager> nimgr;

    mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
    NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

    nsCOMPtr<nsINodeInfo> ni;
    nimgr->GetNodeInfo(aName, nsnull, aNameSpaceID, *getter_AddRefs(ni));

    return SetAttr(ni, aValue, aNotify);
}

NS_IMETHODIMP
nsSVGElement::SetAttr(nsINodeInfo* aNodeInfo,
                      const nsAString& aValue,
                      PRBool aNotify)
{
  return mAttributes->SetAttr(aNodeInfo, aValue, aNotify);  
}

NS_IMETHODIMP
nsSVGElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                           nsAString& aResult) const
{
  nsCOMPtr<nsIAtom> prefix;
  return GetAttr(aNameSpaceID, aName, *getter_AddRefs(prefix), aResult);
}

NS_IMETHODIMP
nsSVGElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                      nsIAtom*& aPrefix,
                      nsAString& aResult) const
{
  return mAttributes->GetAttr(aNameSpaceID, aName, aPrefix, aResult);
}

NS_IMETHODIMP
nsSVGElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                        PRBool aNotify)
{
  return mAttributes->UnsetAttr(aNameSpaceID, aName, aNotify);
}

NS_IMETHODIMP_(PRBool)
nsSVGElement::HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const
{
  return mAttributes->HasAttr(aNameSpaceID, aName);
}

NS_IMETHODIMP
nsSVGElement::GetAttrNameAt(PRInt32 aIndex,
                            PRInt32& aNameSpaceID, 
                            nsIAtom*& aName,
                            nsIAtom*& aPrefix) const
{
  return mAttributes->GetAttrNameAt(aIndex, aNameSpaceID, aName, aPrefix);
}

NS_IMETHODIMP
nsSVGElement::GetAttrCount(PRInt32& aResult) const
{
  aResult = mAttributes->Count();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::List(FILE* out, PRInt32 aIndent) const
{
  // XXX
  fprintf(out, "some SVG element\n");
  return NS_OK; 
}

NS_IMETHODIMP
nsSVGElement::DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const
{
  // XXX
  fprintf(out, "some SVG element\n");
  return NS_OK; 
}

NS_IMETHODIMP
nsSVGElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
//  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}  


//----------------------------------------------------------------------
// nsIStyledContent methods

NS_IMETHODIMP
nsSVGElement::GetID(nsIAtom*& aId)const
{
  nsresult rv;  
  nsAutoString value;
  
  rv = NS_CONST_CAST(nsSVGElement*,this)->GetAttribute(NS_LITERAL_STRING("id"), value);
  if (NS_SUCCEEDED(rv))
    aId = NS_NewAtom(value);
  
  return rv;
}

NS_IMETHODIMP
nsSVGElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::GetInlineStyleRule(nsIStyleRule** aStyleRule)
{
  return mStyle->GetStyleRule(mDocument, aStyleRule);
}

NS_IMETHODIMP
nsSVGElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                       nsChangeHint& aHint) const
{
  // we don't rely on the cssframeconstructor to map attribute changes
  // to changes in our frames. an exception is css.
  // style_hint_content will trigger a re-resolve of the style context
  // if the attribute is used in a css selector:
  aHint = NS_STYLE_HINT_CONTENT;

  // ... and we special case the style attribute
  if (aAttribute == nsSVGAtoms::style) {
    aHint = NS_STYLE_HINT_VISUAL;
//    aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  
  return NS_OK;
}


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMETHODIMP
nsSVGElement::GetNodeName(nsAString& aNodeName)
{
  return mNodeInfo->GetQualifiedName(aNodeName);
}

NS_IMETHODIMP
nsSVGElement::GetNodeValue(nsAString& aNodeValue)
{
  aNodeValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::SetNodeValue(const nsAString& aNodeValue)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsSVGElement::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::ELEMENT_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::GetParentNode(nsIDOMNode** aParentNode)
{
  if (mParent) {
    return CallQueryInterface(mParent, aParentNode);
  }
  if (mDocument) {
    // we're the root content
    return CallQueryInterface(mDocument, aParentNode);
  }

  // A standalone element (i.e. one without a parent or a document)
  *aParentNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  nsDOMSlots *slots = GetDOMSlots();

  if (!slots->mChildNodes) {
    slots->mChildNodes = new nsChildContentList(this);
    if (!slots->mChildNodes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(slots->mChildNodes);
  }

  return CallQueryInterface(slots->mChildNodes, aChildNodes);
}

NS_IMETHODIMP
nsSVGElement::GetFirstChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent *)mChildren.ElementAt(0);
  if (child) {
    nsresult res = CallQueryInterface(child, aNode);
    NS_ASSERTION(NS_SUCCEEDED(res), "Must be a DOM Node"); // must be a DOM Node
    return res;
  }
  *aNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::GetLastChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent *)mChildren.ElementAt(mChildren.Count()-1);
  if (child) {
    nsresult res = CallQueryInterface(child, aNode);
    NS_ASSERTION(NS_SUCCEEDED(res), "Must be a DOM Node"); // must be a DOM Node
    return res;
  }
  *aNode = nsnull;
  return NS_OK;

}

NS_IMETHODIMP
nsSVGElement::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
  return nsGenericElement::GetPreviousSibling(aPreviousSibling);
}

NS_IMETHODIMP
nsSVGElement::GetNextSibling(nsIDOMNode** aNextSibling)
{
  return nsGenericElement::GetNextSibling(aNextSibling);
}

NS_IMETHODIMP
nsSVGElement::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  *aAttributes = mAttributes;
  NS_ADDREF(*aAttributes);
  return NS_OK; 
}

NS_IMETHODIMP
nsSVGElement::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  return nsGenericElement::GetOwnerDocument(aOwnerDocument);
}

NS_IMETHODIMP
nsSVGElement::GetNamespaceURI(nsAString& aNamespaceURI)
{
  return mNodeInfo->GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP
nsSVGElement::GetPrefix(nsAString& aPrefix)
{
  return nsGenericElement::GetPrefix(aPrefix);
}

NS_IMETHODIMP
nsSVGElement::SetPrefix(const nsAString& aPrefix)
{
  return nsGenericElement::SetPrefix(aPrefix);
}

NS_IMETHODIMP
nsSVGElement::GetLocalName(nsAString& aLocalName)
{
  return nsGenericElement::GetLocalName(aLocalName);
}

NS_IMETHODIMP
nsSVGElement::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  return nsGenericElement::doInsertBefore(aNewChild, aRefChild, aReturn);
}

NS_IMETHODIMP
nsSVGElement::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return nsGenericElement::doReplaceChild(aNewChild, aOldChild, aReturn);
}

NS_IMETHODIMP
nsSVGElement::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return nsGenericElement::doRemoveChild(aOldChild, aReturn);
}

NS_IMETHODIMP
nsSVGElement::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return nsGenericElement::doInsertBefore(aNewChild, nsnull, aReturn);
}

NS_IMETHODIMP
nsSVGElement::HasChildNodes(PRBool* aReturn)
{
  if (0 != mChildren.Count()) {
    *aReturn = PR_TRUE;
  }
  else {
    *aReturn = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ASSERTION(1==0,"CloneNode must be implemented by subclass!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGElement::Normalize()
{
  return nsGenericElement::Normalize();
}

NS_IMETHODIMP
nsSVGElement::IsSupported(const nsAString& aFeature, const nsAString& aVersion, PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSVGElement::HasAttributes(PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  
  PRInt32 attrCount = 0;
  
  GetAttrCount(attrCount);
  
  *aReturn = (attrCount > 0);
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMElement methods

// forwarded to nsGenericElement implementations


//----------------------------------------------------------------------
// nsIDOMSVGElement methods

/* attribute DOMString id; */
NS_IMETHODIMP nsSVGElement::GetId(nsAString & aId)
{
  return GetAttribute(NS_LITERAL_STRING("id"), aId);
}

NS_IMETHODIMP nsSVGElement::SetId(const nsAString & aId)
{
  return SetAttribute(NS_LITERAL_STRING("id"), aId);
}

/* readonly attribute nsIDOMSVGSVGElement ownerSVGElement; */
NS_IMETHODIMP
nsSVGElement::GetOwnerSVGElement(nsIDOMSVGSVGElement * *aOwnerSVGElement)
{
  *aOwnerSVGElement = nsnull;

  nsCOMPtr<nsIBindingManager> bindingManager;
  if (mDocument) {
    mDocument->GetBindingManager(getter_AddRefs(bindingManager));
  }

  nsCOMPtr<nsIContent> parent;
  
  if (bindingManager) {
    // we have a binding manager -- do we have an anonymous parent?
    bindingManager->GetInsertionParent(this, getter_AddRefs(parent));
  }

  if (!parent) {
    // if we didn't find an anonymous parent, use the explicit one,
    // whether it's null or not...
    parent = mParent;
  }

  while (parent) {    
    nsCOMPtr<nsIDOMSVGSVGElement> SVGSVGElement = do_QueryInterface(parent);
    if (SVGSVGElement) {
      *aOwnerSVGElement = SVGSVGElement;
      NS_ADDREF(*aOwnerSVGElement);
      return NS_OK;
    }
    nsCOMPtr<nsIContent> next;

    if (bindingManager) {
      bindingManager->GetInsertionParent(parent, getter_AddRefs(next));
    }

    if (!next) {
      // no anonymous parent, so use explicit one
      parent->GetParent(*getter_AddRefs(next));
    }
    
    parent = next;
  }

  // we don't have a parent SVG element...

  // are _we_ the outermost SVG element? If yes, return nsnull, but don't fail
  nsCOMPtr<nsIDOMSVGSVGElement> SVGSVGElement = do_QueryInterface((nsIDOMSVGElement*)this);
  NS_ENSURE_TRUE(SVGSVGElement, NS_ERROR_FAILURE);
  return NS_OK; 
}

/* readonly attribute nsIDOMSVGElement viewportElement; */
NS_IMETHODIMP
nsSVGElement::GetViewportElement(nsIDOMSVGElement * *aViewportElement)
{
  *aViewportElement = nsnull;
  nsCOMPtr<nsIDOMSVGSVGElement> SVGSVGElement;
  nsresult rv = GetOwnerSVGElement(getter_AddRefs(SVGSVGElement));
  NS_ENSURE_SUCCESS(rv,rv);
  if (SVGSVGElement) {
    nsCOMPtr<nsIDOMSVGElement> SVGElement = do_QueryInterface(SVGSVGElement);
    *aViewportElement = SVGElement;
    NS_IF_ADDREF(*aViewportElement);
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGElement::WillModifySVGObservable(nsISVGValue* observable)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsSVGElement::DidModifySVGObservable (nsISVGValue* observable)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
// Implementation Helpers:

nsresult
nsSVGElement::CopyNode(nsSVGElement* dest, PRBool deep)
{
  nsresult rv;
  
  // copy attributes:
  NS_ASSERTION(mAttributes, "null pointer");
  NS_ASSERTION(dest->mAttributes, "null pointer");
  rv = mAttributes->CopyAttributes(dest->mAttributes);
  NS_ENSURE_SUCCESS(rv,rv);

  if (deep) {
    // copy children:
    PRInt32 count = mChildren.Count();
    for (PRInt32 i = 0; i < count; ++i) {
      nsIContent* child = NS_STATIC_CAST(nsIContent*, mChildren[i]);
      
      NS_ASSERTION(child != nsnull, "null ptr");
      if (!child)
        return NS_ERROR_UNEXPECTED;
      
      nsCOMPtr<nsIDOMNode> domchild = do_QueryInterface(child);
      NS_ASSERTION(domchild != nsnull, "child is not a DOM node");
      if (! domchild)
        return NS_ERROR_UNEXPECTED;
      
      nsCOMPtr<nsIDOMNode> newdomchild;
      rv = domchild->CloneNode(PR_TRUE, getter_AddRefs(newdomchild));
      if (NS_FAILED(rv)) return rv;
      
      nsCOMPtr<nsIContent> newchild = do_QueryInterface(newdomchild);
      NS_ASSERTION(newchild != nsnull, "newdomchild is not an nsIContent");
      if (!newchild)
        return NS_ERROR_UNEXPECTED;
      
      rv = dest->AppendChildTo(newchild, PR_FALSE, PR_FALSE);
      if (NS_FAILED(rv)) return rv;
    }
  }
  
  return rv;
}

