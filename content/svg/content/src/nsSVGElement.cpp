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
#include "nsICSSStyleRule.h"
#include "nsISVGSVGElement.h"
#include "nsRuleWalker.h"
#include "nsSVGStyleValue.h"

nsSVGElement::nsSVGElement()
    : mAttributes(nsnull)
{
}

nsSVGElement::~nsSVGElement()
{
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
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3Node, new nsNode3Tearoff(this))
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISVGContent)
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

void
nsSVGElement::SetParent(nsIContent* aParent)
{
  nsGenericElement::SetParent(aParent);

  ParentChainChanged();
}

PRBool
nsSVGElement::CanContainChildren() const
{
  return PR_TRUE;
}

PRUint32
nsSVGElement::GetChildCount() const
{
  return mAttrsAndChildren.ChildCount();
}

nsIContent *
nsSVGElement::GetChildAt(PRUint32 aIndex) const
{
  return mAttrsAndChildren.GetSafeChildAt(aIndex);
}

PRInt32
nsSVGElement::IndexOf(nsIContent* aPossibleChild) const
{
  return mAttrsAndChildren.IndexOfChild(aPossibleChild);
}

nsIAtom *
nsSVGElement::GetIDAttributeName() const
{
  return nsSVGAtoms::id;
}

const nsAttrName*
nsSVGElement::InternalGetExistingAttrNameFromQName(const nsAString& aStr) const
{
  return mAttributes->GetExistingAttrNameFromQName(aStr);
}

nsresult
nsSVGElement::SetAttr(PRInt32 aNamespaceID, nsIAtom* aName, nsIAtom* aPrefix,
                      const nsAString& aValue,
                      PRBool aNotify)
{
  return mAttributes->SetAttr(aNamespaceID, aName, aPrefix, aValue, aNotify);  
}

nsresult
nsSVGElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                           nsAString& aResult) const
{
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown,
               "must have a real namespace ID!");

  return mAttributes->GetAttr(aNameSpaceID, aName, aResult);
}

nsresult
nsSVGElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                        PRBool aNotify)
{
  return mAttributes->UnsetAttr(aNameSpaceID, aName, aNotify);
}

PRBool
nsSVGElement::HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const
{
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown,
               "must have a real namespace ID!");
  return mAttributes->HasAttr(aNameSpaceID, aName);
}

nsresult
nsSVGElement::GetAttrNameAt(PRUint32 aIndex,
                            PRInt32* aNameSpaceID,
                            nsIAtom** aName,
                            nsIAtom** aPrefix) const
{
  return mAttributes->GetAttrNameAt(aIndex, aNameSpaceID, aName, aPrefix);
}

PRUint32
nsSVGElement::GetAttrCount() const
{
  return mAttributes->Count();
}

#ifdef DEBUG
void
nsSVGElement::List(FILE* out, PRInt32 aIndent) const
{
  // XXX
  fprintf(out, "some SVG element\n");
}

void
nsSVGElement::DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const
{
  // XXX
  fprintf(out, "some SVG element\n");
}
#endif // DEBUG

nsresult
nsSVGElement::SetBindingParent(nsIContent* aParent)
{
  nsresult rv = nsGenericElement::SetBindingParent(aParent);

  // XXX Are parent and bindingparent always in sync? (in which case
  // we don't have to call ParentChainChanged() here)
  ParentChainChanged();
  return rv;
}

//----------------------------------------------------------------------
// nsIStyledContent methods

NS_IMETHODIMP
nsSVGElement::GetID(nsIAtom** aId)const
{
  nsAutoString value;
  
  nsresult rv = NS_CONST_CAST(nsSVGElement*,this)->
                    GetAttribute(NS_LITERAL_STRING("id"), value);
  if (NS_SUCCEEDED(rv))
    *aId = NS_NewAtom(value);
  else
    *aId = nsnull;
  
  return rv;
}

NS_IMETHODIMP
nsSVGElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
#ifdef DEBUG
//  printf("nsSVGElement(%p)::WalkContentStyleRules()\n", this);
#endif
  nsCOMPtr<nsIStyleRule> rule;
  mAttributes->GetContentStyleRule(getter_AddRefs(rule));
  if (rule)  
    aRuleWalker->Forward(rule);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::GetInlineStyleRule(nsICSSStyleRule** aStyleRule)
{
  return mStyle->GetStyleRule(this, aStyleRule);
}

// PresentationAttributes-FillStroke
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sFillStrokeMap[] = {
  { &nsSVGAtoms::fill },
  { &nsSVGAtoms::fill_opacity },
  { &nsSVGAtoms::fill_rule },
  { &nsSVGAtoms::stroke },
  { &nsSVGAtoms::stroke_dasharray },
  { &nsSVGAtoms::stroke_dashoffset },
  { &nsSVGAtoms::stroke_linecap },
  { &nsSVGAtoms::stroke_linejoin },
  { &nsSVGAtoms::stroke_miterlimit },
  { &nsSVGAtoms::stroke_opacity },
  { &nsSVGAtoms::stroke_width },
  { nsnull }
};

// PresentationAttributes-Graphics
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sGraphicsMap[] = {
  { &nsSVGAtoms::clip_path },
  { &nsSVGAtoms::clip_rule },
  { &nsSVGAtoms::cursor },
  { &nsSVGAtoms::display },
  { &nsSVGAtoms::filter },
  { &nsSVGAtoms::image_rendering },
  { &nsSVGAtoms::mask },
  { &nsSVGAtoms::opacity },
  { &nsSVGAtoms::pointer_events },
  { &nsSVGAtoms::shape_rendering },
  { &nsSVGAtoms::text_rendering },
  { &nsSVGAtoms::visibility },
  { nsnull }
};

// PresentationAttributes-TextContentElements
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sTextContentElementsMap[] = {
  { &nsSVGAtoms::alignment_baseline },
  { &nsSVGAtoms::baseline_shift },
  { &nsSVGAtoms::direction },
  { &nsSVGAtoms::dominant_baseline },
  { &nsSVGAtoms::glyph_orientation_horizontal },
  { &nsSVGAtoms::glyph_orientation_vertical },
  { &nsSVGAtoms::kerning },
  { &nsSVGAtoms::letter_spacing },
  { &nsSVGAtoms::text_anchor },
  { &nsSVGAtoms::text_decoration },
  { &nsSVGAtoms::unicode_bidi },
  { &nsSVGAtoms::word_spacing },
  { nsnull }
};

// PresentationAttributes-FontSpecification
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sFontSpecificationMap[] = {
  { &nsSVGAtoms::font_family },
  { &nsSVGAtoms::font_size },
  { &nsSVGAtoms::font_size_adjust },
  { &nsSVGAtoms::font_stretch },
  { &nsSVGAtoms::font_style },
  { &nsSVGAtoms::font_variant },
  { &nsSVGAtoms::font_weight },  
  { nsnull }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMETHODIMP
nsSVGElement::GetNodeName(nsAString& aNodeName)
{
  mNodeInfo->GetQualifiedName(aNodeName);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::GetNodeValue(nsAString& aNodeValue)
{
  SetDOMStringToNull(aNodeValue);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::SetNodeValue(const nsAString& aNodeValue)
{
  // The DOM spec says that when nodeValue is defined to be null "setting it
  // has no effect", so we don't throw an exception.
  return NS_OK;
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
  return nsGenericElement::GetParentNode(aParentNode);
}

NS_IMETHODIMP
nsSVGElement::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  return nsGenericElement::GetChildNodes(aChildNodes);
}

NS_IMETHODIMP
nsSVGElement::GetFirstChild(nsIDOMNode** aNode)
{
  return nsGenericElement::GetFirstChild(aNode);
}

NS_IMETHODIMP
nsSVGElement::GetLastChild(nsIDOMNode** aNode)
{
  return nsGenericElement::GetLastChild(aNode);
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
  return nsGenericElement::doInsertBefore(this, aNewChild, aRefChild, aReturn);
}

NS_IMETHODIMP
nsSVGElement::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return nsGenericElement::doReplaceChild(this, aNewChild, aOldChild, aReturn);
}

NS_IMETHODIMP
nsSVGElement::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return nsGenericElement::doRemoveChild(this, aOldChild, aReturn);
}

NS_IMETHODIMP
nsSVGElement::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return nsGenericElement::doInsertBefore(this, aNewChild, nsnull, aReturn);
}

NS_IMETHODIMP
nsSVGElement::HasChildNodes(PRBool* aReturn)
{
  if (0 != mAttrsAndChildren.ChildCount()) {
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
  
  *aReturn = GetAttrCount() > 0;

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

  nsIBindingManager *bindingManager = nsnull;
  if (mDocument) {
    bindingManager = mDocument->GetBindingManager();
  }

  nsCOMPtr<nsIContent> parent;
  
  if (bindingManager) {
    // we have a binding manager -- do we have an anonymous parent?
    bindingManager->GetInsertionParent(this, getter_AddRefs(parent));
  }

  if (!parent) {
    // if we didn't find an anonymous parent, use the explicit one,
    // whether it's null or not...
    parent = GetParent();
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
      next = parent->GetParent();
    }
    
    parent = next;
  }

  // we don't have a parent SVG element...

  // are _we_ the outermost SVG element? If yes, return nsnull, but don't fail
  nsCOMPtr<nsIDOMSVGSVGElement> SVGSVGElement = do_QueryInterface((nsIDOMSVGElement*)this);
  if (SVGSVGElement) return NS_OK;
  
  // no owner found and we aren't the outermost SVG element either.
  // this situation can e.g. occur during content tree teardown. 
  return NS_ERROR_FAILURE;
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
// nsISVGContent methods:

// recursive helper to call ParentChainChanged across non-SVG elements
static void CallParentChainChanged(nsIContent*elem)
{
  NS_ASSERTION(elem, "null element");
  
  PRUint32 count = elem->GetChildCount();
  for (PRUint32 i=0; i<count; ++i) {
    nsIContent* child = elem->GetChildAt(i);

    nsCOMPtr<nsISVGContent> svgChild = do_QueryInterface(child);
    if (svgChild) {
      svgChild->ParentChainChanged();
    }
    else {
      // non-svg element might have an svg child, so recurse
      CallParentChainChanged(child);
    }
  }
}

void
nsSVGElement::ParentChainChanged()
{
  CallParentChainChanged(this);
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
    PRInt32 count = mAttrsAndChildren.ChildCount();
    for (PRInt32 i = 0; i < count; ++i) {
      nsIContent* child = mAttrsAndChildren.ChildAt(i);
      
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

