/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "mozilla/dom/SVGUseElement.h"
#include "mozilla/dom/SVGUseElementBinding.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "mozilla/dom/Element.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "mozilla/URLExtraData.h"
#include "nsSVGEffects.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Use)

namespace mozilla {
namespace dom {

JSObject*
SVGUseElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGUseElementBinding::Wrap(aCx, this, aGivenProto);
}

////////////////////////////////////////////////////////////////////////
// implementation

nsSVGElement::LengthInfo SVGUseElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
};

nsSVGElement::StringInfo SVGUseElement::sStringInfo[2] =
{
  { &nsGkAtoms::href, kNameSpaceID_None, true },
  { &nsGkAtoms::href, kNameSpaceID_XLink, true }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_CYCLE_COLLECTION_CLASS(SVGUseElement)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SVGUseElement,
                                                SVGUseElementBase)
  nsAutoScriptBlocker scriptBlocker;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOriginal)
  tmp->DestroyAnonymousContent();
  tmp->UnlinkSource();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SVGUseElement,
                                                  SVGUseElementBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOriginal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mClone)
  tmp->mSource.Traverse(&cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(SVGUseElement,SVGUseElementBase)
NS_IMPL_RELEASE_INHERITED(SVGUseElement,SVGUseElementBase)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(SVGUseElement)
  NS_INTERFACE_TABLE_INHERITED(SVGUseElement, nsIMutationObserver)
NS_INTERFACE_TABLE_TAIL_INHERITING(SVGUseElementBase)

//----------------------------------------------------------------------
// Implementation

SVGUseElement::SVGUseElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGUseElementBase(aNodeInfo), mSource(this)
{
}

SVGUseElement::~SVGUseElement()
{
  UnlinkSource();
}

//----------------------------------------------------------------------
// nsIDOMNode methods

nsresult
SVGUseElement::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                     bool aPreallocateChildren) const
{
  *aResult = nullptr;
  already_AddRefed<mozilla::dom::NodeInfo> ni = RefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();
  SVGUseElement *it = new SVGUseElement(ni);

  nsCOMPtr<nsINode> kungFuDeathGrip(it);
  nsresult rv1 = it->Init();
  nsresult rv2 = const_cast<SVGUseElement*>(this)->CopyInnerTo(it, aPreallocateChildren);

  // SVGUseElement specific portion - record who we cloned from
  it->mOriginal = const_cast<SVGUseElement*>(this);

  if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2)) {
    kungFuDeathGrip.swap(*aResult);
  }

  return NS_FAILED(rv1) ? rv1 : rv2;
}

already_AddRefed<SVGAnimatedString>
SVGUseElement::Href()
{
  return mStringAttributes[HREF].IsExplicitlySet()
         ? mStringAttributes[HREF].ToDOMAnimatedString(this)
         : mStringAttributes[XLINK_HREF].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedLength>
SVGUseElement::X()
{
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGUseElement::Y()
{
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGUseElement::Width()
{
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGUseElement::Height()
{
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// nsIMutationObserver methods

void
SVGUseElement::CharacterDataChanged(nsIDocument *aDocument,
                                    nsIContent *aContent,
                                    CharacterDataChangeInfo* aInfo)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aContent)) {
    TriggerReclone();
  }
}

void
SVGUseElement::AttributeChanged(nsIDocument* aDocument,
                                Element* aElement,
                                int32_t aNameSpaceID,
                                nsIAtom* aAttribute,
                                int32_t aModType,
                                const nsAttrValue* aOldValue)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aElement)) {
    TriggerReclone();
  }
}

void
SVGUseElement::ContentAppended(nsIDocument *aDocument,
                               nsIContent *aContainer,
                               nsIContent *aFirstNewContent,
                               int32_t aNewIndexInContainer)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aContainer)) {
    TriggerReclone();
  }
}

void
SVGUseElement::ContentInserted(nsIDocument *aDocument,
                               nsIContent *aContainer,
                               nsIContent *aChild,
                               int32_t aIndexInContainer)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aChild)) {
    TriggerReclone();
  }
}

void
SVGUseElement::ContentRemoved(nsIDocument *aDocument,
                              nsIContent *aContainer,
                              nsIContent *aChild,
                              int32_t aIndexInContainer,
                              nsIContent *aPreviousSibling)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aChild)) {
    TriggerReclone();
  }
}

void
SVGUseElement::NodeWillBeDestroyed(const nsINode *aNode)
{
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  UnlinkSource();
}

//----------------------------------------------------------------------

nsIContent*
SVGUseElement::CreateAnonymousContent()
{
  mClone = nullptr;

  if (mSource.get()) {
    mSource.get()->RemoveMutationObserver(this);
  }

  LookupHref();
  nsIContent* targetContent = mSource.get();
  if (!targetContent)
    return nullptr;

  // make sure target is valid type for <use>
  // QIable nsSVGGraphicsElement would eliminate enumerating all elements
  if (!targetContent->IsAnyOfSVGElements(nsGkAtoms::svg,
                                         nsGkAtoms::symbol,
                                         nsGkAtoms::g,
                                         nsGkAtoms::path,
                                         nsGkAtoms::text,
                                         nsGkAtoms::rect,
                                         nsGkAtoms::circle,
                                         nsGkAtoms::ellipse,
                                         nsGkAtoms::line,
                                         nsGkAtoms::polyline,
                                         nsGkAtoms::polygon,
                                         nsGkAtoms::image,
                                         nsGkAtoms::use))
    return nullptr;

  // circular loop detection

  // check 1 - check if we're a document descendent of the target
  if (nsContentUtils::ContentIsDescendantOf(this, targetContent))
    return nullptr;

  // check 2 - check if we're a clone, and if we already exist in the hierarchy
  if (GetParent() && mOriginal) {
    for (nsCOMPtr<nsIContent> content = GetParent();
         content;
         content = content->GetParent()) {
      if (content->IsSVGElement(nsGkAtoms::use) &&
          static_cast<SVGUseElement*>(content.get())->mOriginal == mOriginal) {
        return nullptr;
      }
    }
  }

  nsCOMPtr<nsINode> newnode;
  nsNodeInfoManager* nodeInfoManager =
    targetContent->OwnerDoc() == OwnerDoc() ?
      nullptr : OwnerDoc()->NodeInfoManager();
  nsNodeUtils::Clone(targetContent, true, nodeInfoManager, nullptr,
                     getter_AddRefs(newnode));

  nsCOMPtr<nsIContent> newcontent = do_QueryInterface(newnode);

  if (!newcontent)
    return nullptr;

  if (newcontent->IsAnyOfSVGElements(nsGkAtoms::svg, nsGkAtoms::symbol)) {
    nsSVGElement *newElement = static_cast<nsSVGElement*>(newcontent.get());

    if (mLengthAttributes[ATTR_WIDTH].IsExplicitlySet())
      newElement->SetLength(nsGkAtoms::width, mLengthAttributes[ATTR_WIDTH]);
    if (mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet())
      newElement->SetLength(nsGkAtoms::height, mLengthAttributes[ATTR_HEIGHT]);
  }

  // Store the base URI
  nsCOMPtr<nsIURI> baseURI = targetContent->GetBaseURI();
  if (!baseURI) {
    return nullptr;
  }
  mContentURLData = new URLExtraData(baseURI.forget(),
                                     do_AddRef(OwnerDoc()->GetDocumentURI()),
                                     do_AddRef(NodePrincipal()));

  targetContent->AddMutationObserver(this);
  mClone = newcontent;

#ifdef DEBUG
  // Our anonymous clone can get restyled by various things
  // (e.g. SMIL).  Reconstructing its frame is OK, though, because
  // it's going to be our _only_ child in the frame tree, so can't get
  // mis-ordered with anything.
  mClone->SetProperty(nsGkAtoms::restylableAnonymousNode,
                      reinterpret_cast<void*>(true));
#endif // DEBUG

  return mClone;
}

nsIURI*
SVGUseElement::GetSourceDocURI()
{
  nsIContent* targetContent = mSource.get();
  if (!targetContent)
    return nullptr;

  return targetContent->OwnerDoc()->GetDocumentURI();
}

void
SVGUseElement::DestroyAnonymousContent()
{
  nsContentUtils::DestroyAnonymousContent(&mClone);
}

bool
SVGUseElement::OurWidthAndHeightAreUsed() const
{
  return mClone && mClone->IsAnyOfSVGElements(nsGkAtoms::svg, nsGkAtoms::symbol);
}

//----------------------------------------------------------------------
// implementation helpers

void
SVGUseElement::SyncWidthOrHeight(nsIAtom* aName)
{
  NS_ASSERTION(aName == nsGkAtoms::width || aName == nsGkAtoms::height,
               "The clue is in the function name");
  NS_ASSERTION(OurWidthAndHeightAreUsed(), "Don't call this");

  if (OurWidthAndHeightAreUsed()) {
    nsSVGElement *target = static_cast<nsSVGElement*>(mClone.get());
    uint32_t index = *sLengthInfo[ATTR_WIDTH].mName == aName ? ATTR_WIDTH : ATTR_HEIGHT;

    if (mLengthAttributes[index].IsExplicitlySet()) {
      target->SetLength(aName, mLengthAttributes[index]);
      return;
    }
    if (mClone->IsSVGElement(nsGkAtoms::svg)) {
      // Our width/height attribute is now no longer explicitly set, so we
      // need to revert the clone's width/height to the width/height of the
      // content that's being cloned.
      TriggerReclone();
      return;
    }
    // Our width/height attribute is now no longer explicitly set, so we
    // need to set the value to 100%
    nsSVGLength2 length;
    length.Init(SVGContentUtils::XY, 0xff,
                100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    target->SetLength(aName, length);
    return;
  }
}

void
SVGUseElement::LookupHref()
{
  nsAutoString href;
  if (mStringAttributes[HREF].IsExplicitlySet()) {
    mStringAttributes[HREF].GetAnimValue(href, this);
  } else {
    mStringAttributes[XLINK_HREF].GetAnimValue(href, this);
  }

  if (href.IsEmpty()) {
    return;
  }

  nsCOMPtr<nsIURI> originURI =
    mOriginal ? mOriginal->GetBaseURI() : GetBaseURI();
  nsCOMPtr<nsIURI> baseURI = nsContentUtils::IsLocalRefURL(href)
    ? nsSVGEffects::GetBaseURLForLocalRef(this, originURI)
    : originURI;

  nsCOMPtr<nsIURI> targetURI;
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                            GetComposedDoc(), baseURI);
  mSource.Reset(this, targetURI);
}

void
SVGUseElement::TriggerReclone()
{
  nsIDocument *doc = GetComposedDoc();
  if (!doc)
    return;
  nsIPresShell *presShell = doc->GetShell();
  if (!presShell)
    return;
  presShell->PostRecreateFramesFor(this);
}

void
SVGUseElement::UnlinkSource()
{
  if (mSource.get()) {
    mSource.get()->RemoveMutationObserver(this);
  }
  mSource.Unlink();
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ gfxMatrix
SVGUseElement::PrependLocalTransformsTo(
  const gfxMatrix &aMatrix, SVGTransformTypes aWhich) const
{
  // 'transform' attribute:
  gfxMatrix userToParent;

  if (aWhich == eUserSpaceToParent || aWhich == eAllTransforms) {
    userToParent = GetUserToParentTransform(mAnimateMotionTransform,
                                            mTransforms);
    if (aWhich == eUserSpaceToParent) {
      return userToParent * aMatrix;
    }
  }

  // our 'x' and 'y' attributes:
  float x, y;
  const_cast<SVGUseElement*>(this)->GetAnimatedLengthValues(&x, &y, nullptr);

  gfxMatrix childToUser = gfxMatrix::Translation(x, y);

  if (aWhich == eAllTransforms) {
    return childToUser * userToParent * aMatrix;
  }

  MOZ_ASSERT(aWhich == eChildToUserSpace, "Unknown TransformTypes");

  // The following may look broken because pre-multiplying our eChildToUserSpace
  // transform with another matrix without including our eUserSpaceToParent
  // transform between the two wouldn't make sense.  We don't expect that to
  // ever happen though.  We get here either when the identity matrix has been
  // passed because our caller just wants our eChildToUserSpace transform, or
  // when our eUserSpaceToParent transform has already been multiplied into the
  // matrix that our caller passes (such as when we're called from PaintSVG).
  return childToUser * aMatrix;
}

/* virtual */ bool
SVGUseElement::HasValidDimensions() const
{
  return (!mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() ||
           mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0) &&
         (!mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() ||
           mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0);
}

nsSVGElement::LengthAttributesInfo
SVGUseElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsSVGElement::StringAttributesInfo
SVGUseElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGUseElement::IsAttributeMapped(const nsIAtom* name) const
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

  return FindAttributeDependence(name, map) ||
    SVGUseElementBase::IsAttributeMapped(name);
}

} // namespace dom
} // namespace mozilla
