/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/ErrorResult.h"

#include "mozilla/dom/SVGUseElement.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGUseElementBinding.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "mozilla/dom/Element.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "mozilla/URLExtraData.h"
#include "SVGObserverUtils.h"
#include "nsSVGUseFrame.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Use)

namespace mozilla {
namespace dom {

JSObject*
SVGUseElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGUseElement_Binding::Wrap(aCx, this, aGivenProto);
}

////////////////////////////////////////////////////////////////////////
// implementation

nsSVGElement::LengthInfo SVGUseElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::width, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::height, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
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
  tmp->UnlinkSource();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SVGUseElement,
                                                  SVGUseElementBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOriginal)
  tmp->mReferencedElementTracker.Traverse(&cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(SVGUseElement,
                                             SVGUseElementBase,
                                             nsIMutationObserver)

//----------------------------------------------------------------------
// Implementation

SVGUseElement::SVGUseElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGUseElementBase(aNodeInfo)
  , mReferencedElementTracker(this)
{
}

SVGUseElement::~SVGUseElement()
{
  UnlinkSource();
}

//----------------------------------------------------------------------
// nsINode methods

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
SVGUseElement::CharacterDataChanged(nsIContent* aContent,
                                    const CharacterDataChangeInfo&)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aContent)) {
    TriggerReclone();
  }
}

void
SVGUseElement::AttributeChanged(Element* aElement,
                                int32_t aNameSpaceID,
                                nsAtom* aAttribute,
                                int32_t aModType,
                                const nsAttrValue* aOldValue)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aElement)) {
    TriggerReclone();
  }
}

void
SVGUseElement::ContentAppended(nsIContent* aFirstNewContent)
{
  // FIXME(emilio, bug 1442336): Why does this check the parent but
  // ContentInserted the child?
  if (nsContentUtils::IsInSameAnonymousTree(this, aFirstNewContent->GetParent())) {
    TriggerReclone();
  }
}

void
SVGUseElement::ContentInserted(nsIContent* aChild)
{
  // FIXME(emilio, bug 1442336): Why does this check the child but
  // ContentAppended the parent?
  if (nsContentUtils::IsInSameAnonymousTree(this, aChild)) {
    TriggerReclone();
  }
}

void
SVGUseElement::ContentRemoved(nsIContent* aChild, nsIContent* aPreviousSibling)
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

already_AddRefed<nsIContent>
SVGUseElement::CreateAnonymousContent()
{
  if (mReferencedElementTracker.get()) {
    mReferencedElementTracker.get()->RemoveMutationObserver(this);
  }

  LookupHref();
  nsIContent* targetContent = mReferencedElementTracker.get();
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

  nsNodeInfoManager* nodeInfoManager =
    targetContent->OwnerDoc() == OwnerDoc() ?
      nullptr : OwnerDoc()->NodeInfoManager();
  nsCOMPtr<nsINode> newnode =
    nsNodeUtils::Clone(targetContent, true, nodeInfoManager, nullptr,
                       IgnoreErrors());
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

#ifdef DEBUG
  // Our anonymous clone can get restyled by various things
  // (e.g. SMIL).  Reconstructing its frame is OK, though, because
  // it's going to be our _only_ child in the frame tree, so can't get
  // mis-ordered with anything.
  newcontent->SetProperty(nsGkAtoms::restylableAnonymousNode,
                          reinterpret_cast<void*>(true));
#endif // DEBUG

  return newcontent.forget();
}

nsIURI*
SVGUseElement::GetSourceDocURI()
{
  nsIContent* targetContent = mReferencedElementTracker.get();
  if (!targetContent)
    return nullptr;

  return targetContent->OwnerDoc()->GetDocumentURI();
}

bool
SVGUseElement::OurWidthAndHeightAreUsed() const
{
  auto* frame = GetFrame();
  if (!frame || !frame->GetContentClone()) {
    return false;
  }
  return frame->GetContentClone()->IsAnyOfSVGElements(nsGkAtoms::svg, nsGkAtoms::symbol);
}

//----------------------------------------------------------------------
// implementation helpers

void
SVGUseElement::SyncWidthOrHeight(nsAtom* aName)
{
  NS_ASSERTION(aName == nsGkAtoms::width || aName == nsGkAtoms::height,
               "The clue is in the function name");
  NS_ASSERTION(OurWidthAndHeightAreUsed(), "Don't call this");

  auto* frame = GetFrame();
  nsIContent* clone = frame ? frame->GetContentClone() : nullptr;

  if (OurWidthAndHeightAreUsed()) {
    auto* target = static_cast<nsSVGElement*>(clone);
    uint32_t index = *sLengthInfo[ATTR_WIDTH].mName == aName ? ATTR_WIDTH : ATTR_HEIGHT;

    if (mLengthAttributes[index].IsExplicitlySet()) {
      target->SetLength(aName, mLengthAttributes[index]);
      return;
    }
    if (clone->IsSVGElement(nsGkAtoms::svg)) {
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
                100, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE);
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
    ? SVGObserverUtils::GetBaseURLForLocalRef(this, originURI)
    : originURI;

  nsCOMPtr<nsIURI> targetURI;
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                            GetComposedDoc(), baseURI);
  mReferencedElementTracker.Reset(this, targetURI);
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
  if (mReferencedElementTracker.get()) {
    mReferencedElementTracker.get()->RemoveMutationObserver(this);
  }
  mReferencedElementTracker.Unlink();
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

nsSVGUseFrame*
SVGUseElement::GetFrame() const
{
  nsIFrame* frame = GetPrimaryFrame();
  MOZ_ASSERT_IF(frame, frame->IsSVGUseFrame());
  return static_cast<nsSVGUseFrame*>(frame);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGUseElement::IsAttributeMapped(const nsAtom* name) const
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
