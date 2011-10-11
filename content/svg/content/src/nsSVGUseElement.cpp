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

#include "mozilla/Util.h"

#include "nsSVGUseElement.h"
#include "nsIDOMSVGGElement.h"
#include "nsGkAtoms.h"
#include "nsIDOMDocument.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGSymbolElement.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "mozilla/dom/Element.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

////////////////////////////////////////////////////////////////////////
// implementation

nsSVGElement::LengthInfo nsSVGUseElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
};

nsSVGElement::StringInfo nsSVGUseElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Use)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_CYCLE_COLLECTION_CLASS(nsSVGUseElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsSVGUseElement,
                                                nsSVGUseElementBase)
  nsAutoScriptBlocker scriptBlocker;
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOriginal)
  tmp->DestroyAnonymousContent();
  tmp->UnlinkSource();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsSVGUseElement,
                                                  nsSVGUseElementBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOriginal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mClone)
  tmp->mSource.Traverse(&cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsSVGUseElement,nsSVGUseElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGUseElement,nsSVGUseElementBase)

DOMCI_NODE_DATA(SVGUseElement, nsSVGUseElement)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsSVGUseElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGUseElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGURIReference,
                           nsIDOMSVGUseElement, nsIMutationObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGUseElement)
  if (aIID.Equals(NS_GET_IID(nsSVGUseElement)))
    foundInterface = reinterpret_cast<nsISupports*>(this);
  else
NS_INTERFACE_MAP_END_INHERITING(nsSVGUseElementBase)

//----------------------------------------------------------------------
// Implementation

#ifdef _MSC_VER
// Disable "warning C4355: 'this' : used in base member initializer list".
// We can ignore that warning because we know that mSource's constructor 
// doesn't dereference the pointer passed to it.
#pragma warning(push)
#pragma warning(disable:4355)
#endif
nsSVGUseElement::nsSVGUseElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGUseElementBase(aNodeInfo), mSource(this)
#ifdef _MSC_VER
#pragma warning(pop)
#endif
{
}

nsSVGUseElement::~nsSVGUseElement()
{
  UnlinkSource();
}

//----------------------------------------------------------------------
// nsIDOMNode methods

nsresult
nsSVGUseElement::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nsnull;
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  nsSVGUseElement *it = new nsSVGUseElement(ni.forget());
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsINode> kungFuDeathGrip(it);
  nsresult rv = it->Init();
  rv |= CopyInnerTo(it);

  // nsSVGUseElement specific portion - record who we cloned from
  it->mOriginal = const_cast<nsSVGUseElement*>(this);

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
  return mStringAttributes[HREF].ToDOMAnimatedString(aHref, this);
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
// nsIMutationObserver methods

void
nsSVGUseElement::CharacterDataChanged(nsIDocument *aDocument,
                                      nsIContent *aContent,
                                      CharacterDataChangeInfo* aInfo)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aContent)) {
    TriggerReclone();
  }
}

void
nsSVGUseElement::AttributeChanged(nsIDocument* aDocument,
                                  Element* aElement,
                                  PRInt32 aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  PRInt32 aModType)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aElement)) {
    TriggerReclone();
  }
}

void
nsSVGUseElement::ContentAppended(nsIDocument *aDocument,
                                 nsIContent *aContainer,
                                 nsIContent *aFirstNewContent,
                                 PRInt32 aNewIndexInContainer)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aContainer)) {
    TriggerReclone();
  }
}

void
nsSVGUseElement::ContentInserted(nsIDocument *aDocument,
                                 nsIContent *aContainer,
                                 nsIContent *aChild,
                                 PRInt32 aIndexInContainer)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aChild)) {
    TriggerReclone();
  }
}

void
nsSVGUseElement::ContentRemoved(nsIDocument *aDocument,
                                nsIContent *aContainer,
                                nsIContent *aChild,
                                PRInt32 aIndexInContainer,
                                nsIContent *aPreviousSibling)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aChild)) {
    TriggerReclone();
  }
}

void
nsSVGUseElement::NodeWillBeDestroyed(const nsINode *aNode)
{
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  UnlinkSource();
}

//----------------------------------------------------------------------

nsIContent*
nsSVGUseElement::CreateAnonymousContent()
{
  mClone = nsnull;

  if (mSource.get()) {
    mSource.get()->RemoveMutationObserver(this);
  }

  LookupHref();
  nsIContent* targetContent = mSource.get();
  if (!targetContent)
    return nsnull;

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

  // Make sure the use attributes are valid
  if (!HasValidDimensions())
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
  nsNodeInfoManager* nodeInfoManager =
    targetContent->GetOwnerDoc() == GetOwnerDoc() ?
      nsnull : GetOwnerDoc()->NodeInfoManager();
  nsNodeUtils::Clone(targetContent, PR_TRUE, nodeInfoManager, unused,
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
    nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::svg, nsnull,
                                            kNameSpaceID_SVG,
                                            nsIDOMNode::ELEMENT_NODE);
    if (!nodeInfo)
      return nsnull;

    nsCOMPtr<nsIContent> svgNode;
    NS_NewSVGSVGElement(getter_AddRefs(svgNode), nodeInfo.forget(),
                        NOT_FROM_PARSER);

    if (!svgNode)
      return nsnull;
    
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
      nsCOMPtr<nsIContent> child = newcontent->GetFirstChild();
      newcontent->RemoveChildAt(0, PR_FALSE);
      svgNode->InsertChildAt(child, i, PR_TRUE);
    }

    newcontent = svgNode;
  }

  if (symbol || svg) {
    nsSVGElement *newElement = static_cast<nsSVGElement*>(newcontent.get());

    if (mLengthAttributes[WIDTH].IsExplicitlySet())
      newElement->SetLength(nsGkAtoms::width, mLengthAttributes[WIDTH]);
    if (mLengthAttributes[HEIGHT].IsExplicitlySet())
      newElement->SetLength(nsGkAtoms::height, mLengthAttributes[HEIGHT]);
  }

  // Set up its base URI correctly
  nsCOMPtr<nsIURI> baseURI = targetContent->GetBaseURI();
  if (!baseURI)
    return nsnull;
  nsCAutoString spec;
  baseURI->GetSpec(spec);
  newcontent->SetAttr(kNameSpaceID_XML, nsGkAtoms::base,
                      NS_ConvertUTF8toUTF16(spec), PR_FALSE);

  targetContent->AddMutationObserver(this);
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

bool nsSVGUseElement::HasValidDimensions()
{
  nsSVGSVGElement *ctx = GetCtx();

  return (!mLengthAttributes[WIDTH].IsExplicitlySet() ||
           mLengthAttributes[WIDTH].GetAnimValue(ctx) > 0) &&
         (!mLengthAttributes[HEIGHT].IsExplicitlySet() || 
           mLengthAttributes[HEIGHT].GetAnimValue(ctx) > 0);
}

void
nsSVGUseElement::SyncWidthHeight(nsIAtom* aName)
{
  NS_ASSERTION(aName == nsGkAtoms::width || aName == nsGkAtoms::height,
               "The clue is in the function name");

  if (HasValidDimensions() == !mClone) {
    TriggerReclone();
    return;
  }

  if (mClone) {
    nsCOMPtr<nsIDOMSVGSymbolElement> symbol = do_QueryInterface(mClone);
    nsCOMPtr<nsIDOMSVGSVGElement>    svg    = do_QueryInterface(mClone);

    if (symbol || svg) {
      PRUint32 index = *sLengthInfo[WIDTH].mName == aName ? WIDTH : HEIGHT;
      if (mLengthAttributes[index].IsExplicitlySet()) {
        static_cast<nsSVGElement*>(mClone.get())->
          SetLength(aName, mLengthAttributes[index]);
      } else {
        // Our width/height attribute is now no longer explicitly set, so we
        // need to revert the clone's width/height to the width/height of the
        // content that's being cloned.
        TriggerReclone();
      }
    }
  }
}

void
nsSVGUseElement::LookupHref()
{
  nsAutoString href;
  mStringAttributes[HREF].GetAnimValue(href, this);
  if (href.IsEmpty())
    return;

  nsCOMPtr<nsIURI> targetURI;
  nsCOMPtr<nsIURI> baseURI = mOriginal ? mOriginal->GetBaseURI() : GetBaseURI();
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                            GetCurrentDoc(), baseURI);

  mSource.Reset(this, targetURI);
}

void
nsSVGUseElement::TriggerReclone()
{
  nsIDocument *doc = GetCurrentDoc();
  if (!doc)
    return;
  nsIPresShell *presShell = doc->GetShell();
  if (!presShell)
    return;
  presShell->PostRecreateFramesFor(this);
}

void
nsSVGUseElement::UnlinkSource()
{
  if (mSource.get()) {
    mSource.get()->RemoveMutationObserver(this);
  }
  mSource.Unlink();
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ gfxMatrix
nsSVGUseElement::PrependLocalTransformTo(const gfxMatrix &aMatrix) const
{
  // 'transform' attribute:
  gfxMatrix matrix = nsSVGUseElementBase::PrependLocalTransformTo(aMatrix);

  // now translate by our 'x' and 'y':
  float x, y;
  const_cast<nsSVGUseElement*>(this)->GetAnimatedLengthValues(&x, &y, nsnull);
  return matrix.PreMultiply(gfxMatrix().Translate(gfxPoint(x, y)));
}

nsSVGElement::LengthAttributesInfo
nsSVGUseElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGUseElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGUseElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sLightingEffectsMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
  };

  return FindAttributeDependence(name, map, ArrayLength(map)) ||
    nsSVGUseElementBase::IsAttributeMapped(name);
}

