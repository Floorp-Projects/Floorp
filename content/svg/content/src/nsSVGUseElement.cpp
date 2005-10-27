/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#include "nsSVGGraphicElement.h"
#include "nsIDOMSVGGElement.h"
#include "nsSVGAtoms.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsSVGLength.h"
#include "nsISVGSVGElement.h"
#include "nsSVGCoordCtxProvider.h"
#include "nsIDOMSVGURIReference.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsIDOMSVGUseElement.h"
#include "nsSVGAnimatedLength.h"
#include "nsSVGAnimatedString.h"
#include "nsIDOMDocument.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGSymbolElement.h"
#include "nsIDocument.h"
#include "nsIDOMMutationListener.h"
#include "nsISupportsArray.h"
#include "nsIPresShell.h"
#include "nsIAnonymousContentCreator.h"

#define NS_SVG_USE_ELEMENT_IMPL_CID \
{ 0xa95c13d3, 0xc193, 0x465f, {0x81, 0xf0, 0x02, 0x6d, 0x67, 0x05, 0x54, 0x58 } }

nsresult
NS_NewSVGSVGElement(nsIContent **aResult, nsINodeInfo *aNodeInfo);

typedef nsSVGGraphicElement nsSVGUseElementBase;

class nsSVGUseElement : public nsSVGUseElementBase,
                        public nsIDOMSVGURIReference,
                        public nsIDOMSVGUseElement,
                        public nsIDOMMutationListener,
                        public nsIAnonymousContentCreator
{
protected:
  friend nsresult NS_NewSVGUseElement(nsIContent **aResult,
                                      nsINodeInfo *aNodeInfo);
  nsSVGUseElement(nsINodeInfo *aNodeInfo);
  virtual ~nsSVGUseElement();
  virtual nsresult Init();
  
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_SVG_USE_ELEMENT_IMPL_CID);

  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGUSEELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGUseElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGUseElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGUseElementBase::)

  // nsISVGContent specializations:
  virtual void ParentChainChanged();

  // nsISVGValueObserver specializations:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

  // nsIDOMMutationListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  NS_IMETHOD SubtreeModified(nsIDOMEvent* aMutationEvent);
  NS_IMETHOD NodeInserted(nsIDOMEvent* aMutationEvent);
  NS_IMETHOD NodeRemoved(nsIDOMEvent* aMutationEvent);
  NS_IMETHOD NodeRemovedFromDocument(nsIDOMEvent* aMutationEvent);
  NS_IMETHOD NodeInsertedIntoDocument(nsIDOMEvent* aMutationEvent);
  NS_IMETHOD AttrModified(nsIDOMEvent* aMutationEvent);
  NS_IMETHOD CharacterDataModified(nsIDOMEvent* aMutationEvent);

  // nsIAnonymousContentCreator
  NS_IMETHOD CreateAnonymousContent(nsPresContext* aPresContext,
                                    nsISupportsArray& aAnonymousItems);
  NS_IMETHOD CreateFrameFor(nsPresContext *aPresContext,
                            nsIContent *aContent,
                            nsIFrame **aFrame);

protected:

  nsresult LookupHref(nsIDOMSVGElement **aElement);
  void TriggerReclone();
  void RemoveListeners();

  nsCOMPtr<nsIDOMSVGAnimatedString> mHref;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mX;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mY;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mWidth;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mHeight;

  nsCOMPtr<nsIContent> mOriginal; // if we've been cloned, our "real" copy
  nsCOMPtr<nsIContent> mClone;  // cloned tree
};

////////////////////////////////////////////////////////////////////////
// implementation


NS_IMPL_NS_NEW_SVG_ELEMENT(Use)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGUseElement,nsSVGUseElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGUseElement,nsSVGUseElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGUseElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGURIReference)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGUseElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMutationListener)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGUseElement)
  if (aIID.Equals(NS_GET_IID(nsSVGUseElement)))
    foundInterface = NS_REINTERPRET_CAST(nsISupports*, this);
  else
NS_INTERFACE_MAP_END_INHERITING(nsSVGUseElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGUseElement::nsSVGUseElement(nsINodeInfo *aNodeInfo)
  : nsSVGUseElementBase(aNodeInfo)
{
}

nsSVGUseElement::~nsSVGUseElement()
{
  RemoveListeners();
}

nsresult
nsSVGUseElement::Init()
{
  nsresult rv = nsSVGUseElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

  // DOM property: x ,  #IMPLIED attrib: x
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mX), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::x, mX);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: y ,  #IMPLIED attrib: y
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mY), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::y, mY);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: width ,  #REQUIRED  attrib: width
  // XXX: enforce requiredness
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         100.0f, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mWidth), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::width, mWidth);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: height ,  #REQUIRED  attrib: height
  // XXX: enforce requiredness
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         100.0f, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mHeight), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::height, mHeight);
    NS_ENSURE_SUCCESS(rv,rv);
  }


  // DOM property: href , #REQUIRED attrib: xlink:href
  // XXX: enforce requiredness
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mHref));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::href, mHref, kNameSpaceID_XLink);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsISVGContent methods

void nsSVGUseElement::ParentChainChanged()
{
  // set new context information on our length-properties:
  
  nsCOMPtr<nsIDOMSVGSVGElement> svg_elem;
  GetOwnerSVGElement(getter_AddRefs(svg_elem));
  if (!svg_elem) return;

  nsCOMPtr<nsSVGCoordCtxProvider> ctx = do_QueryInterface(svg_elem);
  NS_ASSERTION(ctx, "<svg> element missing interface");

  // x:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mX->GetAnimVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");
    
    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextX()));
  }

  // y:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mY->GetAnimVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");
    
    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextY()));
  }

  // width:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mWidth->GetAnimVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");
    
    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextX()));
  }

  // height:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mHeight->GetAnimVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> length = do_QueryInterface(dom_length);
    NS_ASSERTION(length, "svg length missing interface");
    
    length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextY()));
  }
}

//----------------------------------------------------------------------
// nsIDOMNode methods

nsresult
nsSVGUseElement::Clone(nsINodeInfo *aNodeInfo, PRBool aDeep,
                       nsIContent **aResult) const
{
  *aResult = nsnull;

  nsSVGUseElement *it = new nsSVGUseElement(aNodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIContent> kungFuDeathGrip(it);
  nsresult rv = it->Init();
  rv |= CopyInnerTo(it, aDeep);

  // nsSVGUseElement specific portion - record who we cloned from
  it->mOriginal = NS_CONST_CAST(nsSVGUseElement*, this);

  if (NS_SUCCEEDED(rv)) {
    kungFuDeathGrip.swap(*aResult);
  }

  return rv;
}

NS_IMETHODIMP
nsSVGUseElement::CloneNode(PRBool aDeep, nsIDOMNode **aResult)
{
  return nsGenericElement::CloneNode(aDeep, this, aResult);
}

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
  NS_IMETHODIMP nsSVGUseElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = mHref;
  NS_IF_ADDREF(*aHref);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGUseElement methods

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP nsSVGUseElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  *aX = mX;
  NS_IF_ADDREF(*aX);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGUseElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  *aY = mY;
  NS_IF_ADDREF(*aY);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGUseElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  *aWidth = mWidth;
  NS_IF_ADDREF(*aWidth);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGUseElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  *aHeight = mHeight;
  NS_IF_ADDREF(*aHeight);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGUseElement::WillModifySVGObservable(nsISVGValue* aObservable,
                                         nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGAnimatedString> s = do_QueryInterface(aObservable);

  // we're about to point at some other reference geometry, so
  // remove all the handlers we stuck on the old one.
  if (s && mHref == s)
    RemoveListeners();

  return nsSVGUseElementBase::WillModifySVGObservable(aObservable, aModType);
}

NS_IMETHODIMP
nsSVGUseElement::DidModifySVGObservable(nsISVGValue* aObservable,
                                        nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGAnimatedString> s = do_QueryInterface(aObservable);

  if (s && mHref == s) {
    // we're changing our nature, clear out the clone information
    mOriginal = nsnull;

    TriggerReclone();
    nsCOMPtr<nsIDOMSVGElement> element;
    LookupHref(getter_AddRefs(element));
    if (element) {
      nsresult rv;
      nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(element));
      NS_ASSERTION(target, "No dom event target for use reference");
      rv = target->AddEventListener(NS_LITERAL_STRING("DOMNodeInserted"),
                                    this, PR_TRUE);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
      rv = target->AddEventListener(NS_LITERAL_STRING("DOMNodeRemoved"),
                                    this, PR_TRUE);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
      rv = target->AddEventListener(NS_LITERAL_STRING("DOMAttrModified"),
                                    this, PR_TRUE);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
      rv = target->AddEventListener(NS_LITERAL_STRING("DOMCharacterDataModified"),
                                    this, PR_TRUE);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
    }
  }

  if (mClone) {
    nsCOMPtr<nsIDOMSVGAnimatedLength> l = do_QueryInterface(aObservable);
    nsCOMPtr<nsIDOMSVGSymbolElement> symbol = do_QueryInterface(mClone);
    nsCOMPtr<nsIDOMSVGSVGElement>    svg    = do_QueryInterface(mClone);

    if (l && (symbol || svg)) {
      if (l == mWidth) {
        nsAutoString width;
        GetAttr(kNameSpaceID_None, nsSVGAtoms::width, width);
        mClone->SetAttr(kNameSpaceID_None, nsSVGAtoms::width, width, PR_FALSE);
      }

      if (l == mHeight) {
        nsAutoString height;
        GetAttr(kNameSpaceID_None, nsSVGAtoms::height, height);
        mClone->SetAttr(kNameSpaceID_None, nsSVGAtoms::height, height, PR_FALSE);
      }
    }
  }

  return nsSVGUseElementBase::DidModifySVGObservable(aObservable, aModType);
}

//----------------------------------------------------------------------
// nsIDOMMutationListener methods

NS_IMETHODIMP nsSVGUseElement::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP nsSVGUseElement::SubtreeModified(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP nsSVGUseElement::NodeInserted(nsIDOMEvent* aEvent)
{
  TriggerReclone();
  return NS_OK;
}

NS_IMETHODIMP nsSVGUseElement::NodeRemoved(nsIDOMEvent* aEvent)
{
  TriggerReclone();
  return NS_OK;
}

NS_IMETHODIMP nsSVGUseElement::NodeRemovedFromDocument(nsIDOMEvent* aMutationEvent)
{
  return NS_OK;
}

NS_IMETHODIMP nsSVGUseElement::NodeInsertedIntoDocument(nsIDOMEvent* aMutationEvent)
{
  return NS_OK;
}

NS_IMETHODIMP nsSVGUseElement::AttrModified(nsIDOMEvent* aMutationEvent)
{
  TriggerReclone();
  return NS_OK;
}

NS_IMETHODIMP nsSVGUseElement::CharacterDataModified(nsIDOMEvent* aMutationEvent)
{
  TriggerReclone();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIAnonymousContentCreator methods

NS_IMETHODIMP
nsSVGUseElement::CreateAnonymousContent(nsPresContext*    aPresContext,
                                        nsISupportsArray& aAnonymousItems)
{
#ifdef DEBUG_tor
  nsAutoString href;
  mHref->GetAnimVal(href);
  fprintf(stderr, "<svg:use> reclone of \"%s\"\n", ToNewCString(href));
#endif

  mClone = nsnull;

  nsCOMPtr<nsIDOMSVGElement> element;
  nsresult rv = LookupHref(getter_AddRefs(element));

  if (!element)
    return rv;

  // make sure target is valid type for <use>
  // QIable nsSVGGraphicsElement would eliminate enumerating all elements
  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(element);
  nsIAtom *tag = targetContent->Tag();
  if (tag != nsSVGAtoms::svg &&
      tag != nsSVGAtoms::symbol &&
      tag != nsSVGAtoms::g &&
      tag != nsSVGAtoms::path &&
      tag != nsSVGAtoms::text &&
      tag != nsSVGAtoms::rect &&
      tag != nsSVGAtoms::circle &&
      tag != nsSVGAtoms::ellipse &&
      tag != nsSVGAtoms::line &&
      tag != nsSVGAtoms::polyline &&
      tag != nsSVGAtoms::polygon &&
      tag != nsSVGAtoms::image &&
      tag != nsSVGAtoms::use)
    return NS_ERROR_FAILURE;

  // circular loop detection

  // check 1 - check if we're a document descendent of the target
  if (nsContentUtils::ContentIsDescendantOf(this, targetContent))
    return NS_ERROR_FAILURE;

  // check 2 - check if we're a clone, and if we already exist in the hierarchy
  if (this->GetParent() && mOriginal) {
    for (nsCOMPtr<nsIContent> content = this->GetParent();
         content;
         content = content->GetParent()) {
      nsCOMPtr<nsIDOMSVGUseElement> useElement = do_QueryInterface(content);

      if (useElement) {
        nsRefPtr<nsSVGUseElement> useImpl;
        useElement->QueryInterface(NS_GET_IID(nsSVGUseElement),
                                   getter_AddRefs(useImpl));

        if (useImpl && useImpl->mOriginal == mOriginal)
          return NS_ERROR_FAILURE;
      }
    }
  }

  nsCOMPtr<nsIDOMNode> newchild;
  element->CloneNode(PR_TRUE, getter_AddRefs(newchild));

  if (!newchild)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIContent>             newcontent = do_QueryInterface(newchild);
  nsCOMPtr<nsIDOMSVGSymbolElement> symbol     = do_QueryInterface(newchild);
  nsCOMPtr<nsIDOMSVGSVGElement>    svg        = do_QueryInterface(newchild);

  if (symbol) {
    nsIDocument *document = GetCurrentDoc();
    if (!document)
      return NS_ERROR_FAILURE;

    nsNodeInfoManager *nodeInfoManager = document->NodeInfoManager();
    if (!nodeInfoManager)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsINodeInfo> nodeInfo;
    nodeInfoManager->GetNodeInfo(nsSVGAtoms::svg, nsnull, kNameSpaceID_SVG,
                                 getter_AddRefs(nodeInfo));
    if (!nodeInfo)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIContent> svgNode;
    NS_NewSVGSVGElement(getter_AddRefs(svgNode), nodeInfo);

    if (!svgNode)
      return NS_ERROR_FAILURE;
    
    if (newcontent->HasAttr(kNameSpaceID_None, nsSVGAtoms::viewBox)) {
      nsAutoString viewbox;
      newcontent->GetAttr(kNameSpaceID_None, nsSVGAtoms::viewBox, viewbox);
      svgNode->SetAttr(kNameSpaceID_None, nsSVGAtoms::viewBox, viewbox, PR_FALSE);
    }

    // copy attributes
    PRUint32 i;
    for (i = 0; i < newcontent->GetAttrCount(); i++) {
      PRInt32 nsID;
      nsCOMPtr<nsIAtom> name;
      nsCOMPtr<nsIAtom> prefix;
      nsAutoString value;

      newcontent->GetAttrNameAt(i, &nsID,
                                getter_AddRefs(name),
                                getter_AddRefs(prefix));
      newcontent->GetAttr(nsID, name, value);
      svgNode->SetAttr(nsID, name, prefix, value, PR_FALSE);
    }

    // move the children over
    PRUint32 num = newcontent->GetChildCount();
    for (i = 0; i < num; i++) {
      nsCOMPtr<nsIContent> child = newcontent->GetChildAt(0);
      newcontent->RemoveChildAt(0, PR_FALSE);
      svgNode->InsertChildAt(child, i, PR_TRUE);
    }

    newcontent = svgNode;
  }

  if (symbol || svg) {
    if (HasAttr(kNameSpaceID_None, nsSVGAtoms::width)) {
      nsAutoString width;
      GetAttr(kNameSpaceID_None, nsSVGAtoms::width, width);
      newcontent->SetAttr(kNameSpaceID_None, nsSVGAtoms::width, width, PR_FALSE);
    }

    if (HasAttr(kNameSpaceID_None, nsSVGAtoms::height)) {
      nsAutoString height;
      GetAttr(kNameSpaceID_None, nsSVGAtoms::height, height);
      newcontent->SetAttr(kNameSpaceID_None, nsSVGAtoms::height, height, PR_FALSE);
    }
  }

  aAnonymousItems.AppendElement(newcontent);
  mClone = newcontent;

  return NS_OK;
}

NS_IMETHODIMP
nsSVGUseElement::CreateFrameFor(nsPresContext *aPresContext,
                                nsIContent *aContent,
                                nsIFrame **aFrame)
{
  *aFrame = nsnull;
  return NS_ERROR_FAILURE;
}


//----------------------------------------------------------------------
// implementation helpers

nsresult
nsSVGUseElement::LookupHref(nsIDOMSVGElement **aResult)
{
  *aResult = nsnull;

  nsresult rv;
  nsAutoString href;
  mHref->GetAnimVal(href);
  if (href.IsEmpty())
    return NS_OK;

  // Get ID from spec
  PRInt32 pos = href.FindChar('#');
  if (pos == -1) {
    NS_WARNING("URI Spec not a reference");
    return NS_ERROR_FAILURE;
  } else if (pos > 0) {
    // XXX we don't support external <use> yet
    NS_WARNING("URI Spec not a local URI reference");
    return NS_ERROR_FAILURE;
  }

  // Strip off hash and get name
  nsAutoString id;
  href.Right(id, href.Length() - (pos + 1));

  // Get document
  nsCOMPtr<nsIDOMDocument> document;
  rv = GetOwnerDocument(getter_AddRefs(document));
  if (!NS_SUCCEEDED(rv) || !document)
    return rv;

  // Get element
  nsCOMPtr<nsIDOMElement> element;
  rv = document->GetElementById(id, getter_AddRefs(element));
  if (!NS_SUCCEEDED(rv) || !element)
    return rv;

  CallQueryInterface(element, aResult);

  return NS_OK;
}

void
nsSVGUseElement::TriggerReclone()
{
  nsIDocument *doc = GetCurrentDoc();
  if (!doc) return;
  nsIPresShell *presShell = doc->GetShellAt(0);
  if (!presShell) return;
  presShell->RecreateFramesFor(this);
}

void
nsSVGUseElement::RemoveListeners()
{
  nsCOMPtr<nsIDOMSVGElement> element;
  LookupHref(getter_AddRefs(element));
  if (element) {
    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(element));
    NS_ASSERTION(target, "No dom event target for use reference");
    
    target->RemoveEventListener(NS_LITERAL_STRING("DOMNodeInserted"),
                                this, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("DOMNodeRemoved"),
                                this, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("DOMAttrModified"),
                                this, PR_TRUE);
    target->RemoveEventListener(NS_LITERAL_STRING("DOMCharacterDataModifed"),
                                this, PR_TRUE);
  }
}
