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

#include "nsSVGUseElement.h"
#include "nsIDOMSVGGElement.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsISVGSVGElement.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsSVGAnimatedString.h"
#include "nsIDOMDocument.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGSymbolElement.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"

////////////////////////////////////////////////////////////////////////
// implementation

nsSVGElement::LengthInfo nsSVGUseElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
};

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
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
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
  RemoveListener();
}

nsresult
nsSVGUseElement::Init()
{
  nsresult rv = nsSVGUseElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

  // DOM property: href , #REQUIRED attrib: xlink:href
  // XXX: enforce requiredness
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mHref));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::href, mHref, kNameSpaceID_XLink);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

nsresult
nsSVGUseElement::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nsnull;

  nsSVGUseElement *it = new nsSVGUseElement(aNodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsINode> kungFuDeathGrip(it);
  nsresult rv = it->Init();
  rv |= CopyInnerTo(it);

  // nsSVGUseElement specific portion - record who we cloned from
  it->mOriginal = NS_CONST_CAST(nsSVGUseElement*, this);

  if (NS_SUCCEEDED(rv)) {
    kungFuDeathGrip.swap(*aResult);
  }

  return rv;
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
  return mLengthAttributes[X].ToDOMAnimatedLength(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGUseElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  return mLengthAttributes[Y].ToDOMAnimatedLength(aY, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGUseElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  return mLengthAttributes[WIDTH].ToDOMAnimatedLength(aWidth, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGUseElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  return mLengthAttributes[HEIGHT].ToDOMAnimatedLength(aHeight, this);
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGUseElement::DidModifySVGObservable(nsISVGValue* aObservable,
                                        nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGAnimatedString> s = do_QueryInterface(aObservable);

  if (s && mHref == s) {
    // we're changing our nature, clear out the clone information
    mOriginal = nsnull;

    TriggerReclone();
  }

  return nsSVGUseElementBase::DidModifySVGObservable(aObservable, aModType);
}

//----------------------------------------------------------------------
// nsIMutationObserver methods

void
nsSVGUseElement::CharacterDataChanged(nsIDocument *aDocument,
                                      nsIContent *aContent,
                                      CharacterDataChangeInfo* aInfo)
{
  TriggerReclone();
}

void
nsSVGUseElement::AttributeChanged(nsIDocument *aDocument,
                                  nsIContent *aContent,
                                  PRInt32 aNameSpaceID,
                                  nsIAtom *aAttribute,
                                  PRInt32 aModType)
{
  TriggerReclone();
}

void
nsSVGUseElement::ContentAppended(nsIDocument *aDocument,
                                 nsIContent *aContainer,
                                 PRInt32 aNewIndexInContainer)
{
  TriggerReclone();
}

void
nsSVGUseElement::ContentInserted(nsIDocument *aDocument,
                                 nsIContent *aContainer,
                                 nsIContent *aChild,
                                 PRInt32 aIndexInContainer)
{
  TriggerReclone();
}

void
nsSVGUseElement::ContentRemoved(nsIDocument *aDocument,
                                nsIContent *aContainer,
                                nsIContent *aChild,
                                PRInt32 aIndexInContainer)
{
  TriggerReclone();
}

void
nsSVGUseElement::NodeWillBeDestroyed(const nsINode *aNode)
{
  RemoveListener();
}

//----------------------------------------------------------------------

nsIContent*
nsSVGUseElement::CreateAnonymousContent()
{
#ifdef DEBUG_tor
  nsAutoString href;
  mHref->GetAnimVal(href);
  fprintf(stderr, "<svg:use> reclone of \"%s\"\n", ToNewCString(href));
#endif

  mClone = nsnull;

  nsCOMPtr<nsIContent> targetContent = LookupHref();

  if (!targetContent)
    return nsnull;

  if (mSourceContent != targetContent) {
    RemoveListener();
    targetContent->AddMutationObserver(this);
  }
  mSourceContent = targetContent;

  // make sure target is valid type for <use>
  // QIable nsSVGGraphicsElement would eliminate enumerating all elements
  nsIAtom *tag = targetContent->Tag();
  if (tag != nsGkAtoms::svg &&
      tag != nsGkAtoms::symbol &&
      tag != nsGkAtoms::g &&
      tag != nsGkAtoms::path &&
      tag != nsGkAtoms::text &&
      tag != nsGkAtoms::rect &&
      tag != nsGkAtoms::circle &&
      tag != nsGkAtoms::ellipse &&
      tag != nsGkAtoms::line &&
      tag != nsGkAtoms::polyline &&
      tag != nsGkAtoms::polygon &&
      tag != nsGkAtoms::image &&
      tag != nsGkAtoms::use)
    return nsnull;

  // circular loop detection

  // check 1 - check if we're a document descendent of the target
  if (nsContentUtils::ContentIsDescendantOf(this, targetContent))
    return nsnull;

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
          return nsnull;
      }
    }
  }

  nsCOMPtr<nsIDOMNode> newnode;
  nsCOMArray<nsINode> unused;
  nsNodeUtils::Clone(targetContent, PR_TRUE, nsnull, unused,
                     getter_AddRefs(newnode));

  nsCOMPtr<nsIContent> newcontent = do_QueryInterface(newnode);

  if (!newcontent)
    return nsnull;

  nsCOMPtr<nsIDOMSVGSymbolElement> symbol     = do_QueryInterface(newcontent);
  nsCOMPtr<nsIDOMSVGSVGElement>    svg        = do_QueryInterface(newcontent);

  if (symbol) {
    nsIDocument *document = GetCurrentDoc();
    if (!document)
      return nsnull;

    nsNodeInfoManager *nodeInfoManager = document->NodeInfoManager();
    if (!nodeInfoManager)
      return nsnull;

    nsCOMPtr<nsINodeInfo> nodeInfo;
    nodeInfoManager->GetNodeInfo(nsGkAtoms::svg, nsnull, kNameSpaceID_SVG,
                                 getter_AddRefs(nodeInfo));
    if (!nodeInfo)
      return nsnull;

    nsCOMPtr<nsIContent> svgNode;
    NS_NewSVGSVGElement(getter_AddRefs(svgNode), nodeInfo);

    if (!svgNode)
      return nsnull;
    
    if (newcontent->HasAttr(kNameSpaceID_None, nsGkAtoms::viewBox)) {
      nsAutoString viewbox;
      newcontent->GetAttr(kNameSpaceID_None, nsGkAtoms::viewBox, viewbox);
      svgNode->SetAttr(kNameSpaceID_None, nsGkAtoms::viewBox, viewbox, PR_FALSE);
    }

    // copy attributes
    const nsAttrName* name;
    PRUint32 i;
    for (i = 0; (name = newcontent->GetAttrNameAt(i)); i++) {
      nsAutoString value;
      PRInt32 nsID = name->NamespaceID();
      nsIAtom* lname = name->LocalName();

      newcontent->GetAttr(nsID, lname, value);
      svgNode->SetAttr(nsID, lname, name->GetPrefix(), value, PR_FALSE);
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
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::width)) {
      nsAutoString width;
      GetAttr(kNameSpaceID_None, nsGkAtoms::width, width);
      newcontent->SetAttr(kNameSpaceID_None, nsGkAtoms::width, width, PR_FALSE);
    }

    if (HasAttr(kNameSpaceID_None, nsGkAtoms::height)) {
      nsAutoString height;
      GetAttr(kNameSpaceID_None, nsGkAtoms::height, height);
      newcontent->SetAttr(kNameSpaceID_None, nsGkAtoms::height, height, PR_FALSE);
    }
  }

  mClone = newcontent;
  return mClone;
}

void
nsSVGUseElement::DestroyAnonymousContent()
{
  nsContentUtils::DestroyAnonymousContent(&mClone);
}

//----------------------------------------------------------------------
// implementation helpers

void
nsSVGUseElement::SyncWidthHeight(PRUint8 aAttrEnum)
{
  if (mClone && (aAttrEnum == WIDTH || aAttrEnum == HEIGHT)) {
    nsCOMPtr<nsIDOMSVGSymbolElement> symbol = do_QueryInterface(mClone);
    nsCOMPtr<nsIDOMSVGSVGElement>    svg    = do_QueryInterface(mClone);

    if (symbol || svg) {
      if (aAttrEnum == WIDTH) {
        nsAutoString width;
        GetAttr(kNameSpaceID_None, nsGkAtoms::width, width);
        mClone->SetAttr(kNameSpaceID_None, nsGkAtoms::width, width, PR_FALSE);
      } else if (aAttrEnum == HEIGHT) {
        nsAutoString height;
        GetAttr(kNameSpaceID_None, nsGkAtoms::height, height);
        mClone->SetAttr(kNameSpaceID_None, nsGkAtoms::height, height, PR_FALSE);
      }
    }
  }
}

nsIContent *
nsSVGUseElement::LookupHref()
{
  nsAutoString href;
  mHref->GetAnimVal(href);
  if (href.IsEmpty())
    return nsnull;

  nsCOMPtr<nsIURI> targetURI, baseURI = GetBaseURI();
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                            GetCurrentDoc(), baseURI);

  return nsContentUtils::GetReferencedElement(targetURI, this);
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
nsSVGUseElement::RemoveListener()
{
  if (mSourceContent) {
    mSourceContent->RemoveMutationObserver(this);
    mSourceContent = nsnull;
  }
}

//----------------------------------------------------------------------
// nsSVGElement methods

void
nsSVGUseElement::DidChangeLength(PRUint8 aAttrEnum, PRBool aDoSetAttr)
{
  nsSVGUseElementBase::DidChangeLength(aAttrEnum, aDoSetAttr);

  SyncWidthHeight(aAttrEnum);
}

nsSVGElement::LengthAttributesInfo
nsSVGUseElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGUseElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
  };

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGUseElementBase::IsAttributeMapped(name);
}
