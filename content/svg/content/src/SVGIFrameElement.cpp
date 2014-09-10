/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGIFrameElement.h"

#include "GeckoProfiler.h"
#include "mozilla/ArrayUtils.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/SVGDocumentBinding.h"
#include "mozilla/dom/SVGIFrameElementBinding.h"
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/Preferences.h"
#include "nsStyleConsts.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT_CHECK_PARSER(IFrame)

namespace mozilla {
namespace dom {

JSObject*
SVGIFrameElement::WrapNode(JSContext *aCx)
{
  return SVGIFrameElementBinding::Wrap(aCx, this);
}
  
//--------------------- IFrame ------------------------

nsSVGElement::LengthInfo SVGIFrameElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y }
};

//----------------------------------------------------------------------
// nsISupports methods
NS_IMPL_ISUPPORTS_INHERITED(SVGIFrameElement, SVGIFrameElementBase,
                            nsIFrameLoaderOwner,
                            nsIDOMNode, nsIDOMElement,
                            nsIDOMSVGElement)
//----------------------------------------------------------------------
// Implementation

SVGIFrameElement::SVGIFrameElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                                   FromParser aFromParser)
  : SVGIFrameElementBase(aNodeInfo)
  , nsElementFrameLoaderOwner(aFromParser)
{
}

SVGIFrameElement::~SVGIFrameElement()
{
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ gfxMatrix
SVGIFrameElement::PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                                           TransformTypes aWhich) const
{
  // 'transform' attribute:
  gfxMatrix fromUserSpace =
    SVGGraphicsElement::PrependLocalTransformsTo(aMatrix, aWhich);
  if (aWhich == eUserSpaceToParent) {
    return fromUserSpace;
  }
  // our 'x' and 'y' attributes:
  float x, y;
  const_cast<SVGIFrameElement*>(this)->
    GetAnimatedLengthValues(&x, &y, nullptr);
  gfxMatrix toUserSpace = gfxMatrix::Translation(x, y);
  if (aWhich == eChildToUserSpace) {
    return toUserSpace;
  }
  NS_ABORT_IF_FALSE(aWhich == eAllTransforms, "Unknown TransformTypes");
  return toUserSpace * fromUserSpace;
}
  
  
//----------------------------------------------------------------------
// nsIDOMNode methods

nsresult
SVGIFrameElement::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nullptr;
  already_AddRefed<mozilla::dom::NodeInfo> ni = nsRefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();
  SVGIFrameElement *it = new SVGIFrameElement(ni, NOT_FROM_PARSER);

  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv1 = it->Init();
  nsresult rv2 = const_cast<SVGIFrameElement*>(this)->CopyInnerTo(it);
  if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2)) {
    kungFuDeathGrip.swap(*aResult);
  }

  return NS_FAILED(rv1) ? rv1 : rv2;
}

//----------------------------------------------------------------------
// nsSVGElement methods
  
nsSVGElement::LengthAttributesInfo
SVGIFrameElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

SVGAnimatedPreserveAspectRatio *
SVGIFrameElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}

//----------------------------------------------------------------------
// nsIDOMSVGIFrameElement methods:

already_AddRefed<SVGAnimatedLength>
SVGIFrameElement::X()
{
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGIFrameElement::Y()
{
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGIFrameElement::Width()
{
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGIFrameElement::Height()
{
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGIFrameElement::PreserveAspectRatio()
{
  nsRefPtr<DOMSVGAnimatedPreserveAspectRatio> ratio;
  mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(getter_AddRefs(ratio),
                                                        this);
  return ratio.forget();
}

void
SVGIFrameElement::GetName(DOMString& name)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
}

void
SVGIFrameElement::GetSrc(DOMString& src)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::src, src);
}

void
SVGIFrameElement::GetSrcdoc(DOMString& srcdoc)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::srcdoc, srcdoc);
}

nsDOMSettableTokenList*
SVGIFrameElement::Sandbox()
{
  return GetTokenList(nsGkAtoms::sandbox);
}

bool
SVGIFrameElement::ParseAttribute(int32_t aNamespaceID,
                                 nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::sandbox) {
      aResult.ParseAtomArray(aValue);
      return true;
    }
  }
  return SVGIFrameElementBase::ParseAttribute(aNamespaceID, aAttribute,
                                              aValue, aResult);
}

nsresult
SVGIFrameElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                          nsIAtom* aPrefix, const nsAString& aValue,
                          bool aNotify)
{
  nsresult rv = nsSVGElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                      aValue, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::src &&
        !HasAttr(kNameSpaceID_None,nsGkAtoms::srcdoc)) {
      // Don't propagate error here. The attribute was successfully set, that's
      // what we should reflect.
      LoadSrc();
    }
    if (aName == nsGkAtoms::srcdoc) {
      // Don't propagate error here. The attribute was successfully set, that's
      // what we should reflect.
      LoadSrc();
    }
  }
  return NS_OK;
}

nsresult
SVGIFrameElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                               const nsAttrValue* aValue,
                               bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::sandbox && mFrameLoader) {
      // If we have an nsFrameLoader, apply the new sandbox flags.
      // Since this is called after the setter, the sandbox flags have
      // alreay been updated.
      mFrameLoader->ApplySandboxFlags(GetSandboxFlags());
    }
  }
  return nsSVGElement::AfterSetAttr(aNameSpaceID, aName, aValue, aNotify);
}

nsresult
SVGIFrameElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                            bool aNotify)
{
  // Invoke on the superclass.
  nsresult rv = nsSVGElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::srcdoc) {
      // Fall back to the src attribute, if any
      LoadSrc();
    }
  }

  return NS_OK;
}

uint32_t
SVGIFrameElement::GetSandboxFlags()
{
  const nsAttrValue* sandboxAttr = GetParsedAttr(nsGkAtoms::sandbox);
  return nsContentUtils::ParseSandboxAttributeToFlags(sandboxAttr);
}

nsresult
SVGIFrameElement::BindToTree(nsIDocument* aDocument,
                             nsIContent* aParent,
                             nsIContent* aBindingParent,
                             bool aCompileEventHandlers)
{
  nsresult rv = nsSVGElement::BindToTree(aDocument, aParent,
                                         aBindingParent,
                                         aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
                 "Missing a script blocker!");

    PROFILER_LABEL("SVGIFrameElement", "BindToTree",
      js::ProfileEntry::Category::OTHER);

    // We're in a document now.  Kick off the frame load.
    LoadSrc();

    if (HasAttr(kNameSpaceID_None, nsGkAtoms::sandbox)) {
      if (mFrameLoader) {
        mFrameLoader->ApplySandboxFlags(GetSandboxFlags());
      }
    }
  }

  // We're now in document and scripts may move us, so clear
  // the mNetworkCreated flag.
  mNetworkCreated = false;
  return rv;
}

void
SVGIFrameElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (mFrameLoader) {
    // This iframe is being taken out of the document, destroy the
    // iframe's frame loader (doing that will tear down the window in
    // this iframe).
    // XXXbz we really want to only partially destroy the frame
    // loader... we don't want to tear down the docshell.  Food for
    // later bug.
    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  nsSVGElement::UnbindFromTree(aDeep, aNullParent);
}

void
SVGIFrameElement::DestroyContent()
{
  if (mFrameLoader) {
    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  nsSVGElement::DestroyContent();
}

nsresult
SVGIFrameElement::CopyInnerTo(Element* aDest)
{
  nsresult rv = nsSVGElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDocument* doc = aDest->OwnerDoc();
  if (doc->IsStaticDocument() && mFrameLoader) {
    SVGIFrameElement* dest =
      static_cast<SVGIFrameElement*>(aDest);
    nsFrameLoader* fl = nsFrameLoader::Create(dest, false);
    NS_ENSURE_STATE(fl);
    dest->mFrameLoader = fl;
    static_cast<nsFrameLoader*>(mFrameLoader.get())->CreateStaticClone(fl);
  }

  return rv;
}

} // namespace dom
} // namespace mozilla
