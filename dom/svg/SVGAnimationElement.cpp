/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAnimationElement.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/SMILAnimationController.h"
#include "mozilla/SMILAnimationFunction.h"
#include "mozilla/SMILTimeContainer.h"
#include "nsContentUtils.h"
#include "nsIContentInlines.h"

namespace mozilla::dom {

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGAnimationElement, SVGAnimationElementBase)
NS_IMPL_RELEASE_INHERITED(SVGAnimationElement, SVGAnimationElementBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGAnimationElement)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::SVGTests)
NS_INTERFACE_MAP_END_INHERITING(SVGAnimationElementBase)

NS_IMPL_CYCLE_COLLECTION_INHERITED(SVGAnimationElement, SVGAnimationElementBase,
                                   mHrefTarget, mTimedElement)

//----------------------------------------------------------------------
// Implementation

SVGAnimationElement::SVGAnimationElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGAnimationElementBase(std::move(aNodeInfo)), mHrefTarget(this) {}

nsresult SVGAnimationElement::Init() {
  nsresult rv = SVGAnimationElementBase::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mTimedElement.SetAnimationElement(this);
  AnimationFunction().SetAnimationElement(this);
  mTimedElement.SetTimeClient(&AnimationFunction());

  return NS_OK;
}

//----------------------------------------------------------------------

Element* SVGAnimationElement::GetTargetElementContent() {
  if (HasAttr(kNameSpaceID_XLink, nsGkAtoms::href) ||
      HasAttr(nsGkAtoms::href)) {
    return mHrefTarget.get();
  }
  MOZ_ASSERT(!mHrefTarget.get(),
             "We shouldn't have a href target "
             "if we don't have an xlink:href or href attribute");

  // No "href" or "xlink:href" attribute --> I should target my parent.
  //
  // Note that we want to use GetParentElement instead of the flattened tree to
  // allow <use><animate>, for example.
  return GetParentElement();
}

bool SVGAnimationElement::GetTargetAttributeName(int32_t* aNamespaceID,
                                                 nsAtom** aLocalName) const {
  const nsAttrValue* nameAttr = mAttrs.GetAttr(nsGkAtoms::attributeName);

  if (!nameAttr) return false;

  NS_ASSERTION(nameAttr->Type() == nsAttrValue::eAtom,
               "attributeName should have been parsed as an atom");

  return NS_SUCCEEDED(nsContentUtils::SplitQName(
      this, nsDependentAtomString(nameAttr->GetAtomValue()), aNamespaceID,
      aLocalName));
}

SMILTimedElement& SVGAnimationElement::TimedElement() { return mTimedElement; }

SVGElement* SVGAnimationElement::GetTargetElement() {
  FlushAnimations();

  // We'll just call the other GetTargetElement method, and QI to the right type
  return SVGElement::FromNodeOrNull(GetTargetElementContent());
}

float SVGAnimationElement::GetStartTime(ErrorResult& rv) {
  FlushAnimations();

  SMILTimeValue startTime = mTimedElement.GetStartTime();
  if (!startTime.IsDefinite()) {
    rv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return 0.f;
  }

  return float(double(startTime.GetMillis()) / PR_MSEC_PER_SEC);
}

float SVGAnimationElement::GetCurrentTimeAsFloat() {
  // Not necessary to call FlushAnimations() for this

  SMILTimeContainer* root = GetTimeContainer();
  if (root) {
    return float(double(root->GetCurrentTimeAsSMILTime()) / PR_MSEC_PER_SEC);
  }

  return 0.0f;
}

float SVGAnimationElement::GetSimpleDuration(ErrorResult& rv) {
  // Not necessary to call FlushAnimations() for this

  SMILTimeValue simpleDur = mTimedElement.GetSimpleDuration();
  if (!simpleDur.IsDefinite()) {
    rv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return 0.f;
  }

  return float(double(simpleDur.GetMillis()) / PR_MSEC_PER_SEC);
}

//----------------------------------------------------------------------
// nsIContent methods

nsresult SVGAnimationElement::BindToTree(BindContext& aContext,
                                         nsINode& aParent) {
  MOZ_ASSERT(!mHrefTarget.get(),
             "Shouldn't have href-target yet (or it should've been cleared)");
  nsresult rv = SVGAnimationElementBase::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add myself to the animation controller's master set of animation elements.
  if (Document* doc = aContext.GetComposedDoc()) {
    if (SMILAnimationController* controller = doc->GetAnimationController()) {
      controller->RegisterAnimationElement(this);
    }
    const nsAttrValue* href =
        HasAttr(nsGkAtoms::href)
            ? mAttrs.GetAttr(nsGkAtoms::href, kNameSpaceID_None)
            : mAttrs.GetAttr(nsGkAtoms::href, kNameSpaceID_XLink);
    if (href) {
      nsAutoString hrefStr;
      href->ToString(hrefStr);

      UpdateHrefTarget(hrefStr);
    }

    mTimedElement.BindToTree(*this);
  }

  AnimationNeedsResample();

  return NS_OK;
}

void SVGAnimationElement::UnbindFromTree(bool aNullParent) {
  SMILAnimationController* controller = OwnerDoc()->GetAnimationController();
  if (controller) {
    controller->UnregisterAnimationElement(this);
  }

  mHrefTarget.Unlink();
  mTimedElement.DissolveReferences();

  AnimationNeedsResample();

  SVGAnimationElementBase::UnbindFromTree(aNullParent);
}

bool SVGAnimationElement::ParseAttribute(int32_t aNamespaceID,
                                         nsAtom* aAttribute,
                                         const nsAString& aValue,
                                         nsIPrincipal* aMaybeScriptedPrincipal,
                                         nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    // Deal with target-related attributes here
    if (aAttribute == nsGkAtoms::attributeName) {
      aResult.ParseAtom(aValue);
      AnimationNeedsResample();
      return true;
    }

    nsresult rv = NS_ERROR_FAILURE;

    // First let the animation function try to parse it...
    bool foundMatch =
        AnimationFunction().SetAttr(aAttribute, aValue, aResult, &rv);

    // ... and if that didn't recognize the attribute, let the timed element
    // try to parse it.
    if (!foundMatch) {
      foundMatch =
          mTimedElement.SetAttr(aAttribute, aValue, aResult, *this, &rv);
    }

    if (foundMatch) {
      AnimationNeedsResample();
      if (NS_FAILED(rv)) {
        ReportAttributeParseFailure(OwnerDoc(), aAttribute, aValue);
        return false;
      }
      return true;
    }
  }

  return SVGAnimationElementBase::ParseAttribute(
      aNamespaceID, aAttribute, aValue, aMaybeScriptedPrincipal, aResult);
}

void SVGAnimationElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                       const nsAttrValue* aValue,
                                       const nsAttrValue* aOldValue,
                                       nsIPrincipal* aSubjectPrincipal,
                                       bool aNotify) {
  if (!aValue && aNamespaceID == kNameSpaceID_None) {
    // Attribute is being removed.
    if (AnimationFunction().UnsetAttr(aName) ||
        mTimedElement.UnsetAttr(aName)) {
      AnimationNeedsResample();
    }
  }

  SVGAnimationElementBase::AfterSetAttr(aNamespaceID, aName, aValue, aOldValue,
                                        aSubjectPrincipal, aNotify);

  if (SVGTests::IsConditionalProcessingAttribute(aName)) {
    bool isDisabled = !SVGTests::PassesConditionalProcessingTests();
    if (mTimedElement.SetIsDisabled(isDisabled)) {
      AnimationNeedsResample();
    }
  }

  if (!IsInComposedDoc()) {
    return;
  }

  if (!((aNamespaceID == kNameSpaceID_None ||
         aNamespaceID == kNameSpaceID_XLink) &&
        aName == nsGkAtoms::href)) {
    return;
  }

  if (!aValue) {
    if (aNamespaceID == kNameSpaceID_None) {
      mHrefTarget.Unlink();
      AnimationTargetChanged();

      // After unsetting href, we may still have xlink:href, so we
      // should try to add it back.
      const nsAttrValue* xlinkHref =
          mAttrs.GetAttr(nsGkAtoms::href, kNameSpaceID_XLink);
      if (xlinkHref) {
        UpdateHrefTarget(xlinkHref->GetStringValue());
      }
    } else if (!HasAttr(nsGkAtoms::href)) {
      mHrefTarget.Unlink();
      AnimationTargetChanged();
    }  // else: we unset xlink:href, but we still have href attribute, so keep
       // mHrefTarget linking to href.
  } else if (!(aNamespaceID == kNameSpaceID_XLink &&
               HasAttr(nsGkAtoms::href))) {
    // Note: "href" takes priority over xlink:href. So if "xlink:href" is being
    // set here, we only let that update our target if "href" is *unset*.
    MOZ_ASSERT(aValue->Type() == nsAttrValue::eString,
               "Expected href attribute to be string type");
    UpdateHrefTarget(aValue->GetStringValue());
  }  // else: we're not yet in a document -- we'll update the target on
     // next BindToTree call.
}

//----------------------------------------------------------------------
// SVG utility methods

void SVGAnimationElement::ActivateByHyperlink() {
  FlushAnimations();

  // The behavior for when the target is an animation element is defined in
  // SMIL Animation:
  //   http://www.w3.org/TR/smil-animation/#HyperlinkSemantics
  SMILTimeValue seekTime = mTimedElement.GetHyperlinkTime();
  if (seekTime.IsDefinite()) {
    SMILTimeContainer* timeContainer = GetTimeContainer();
    if (timeContainer) {
      timeContainer->SetCurrentTime(seekTime.GetMillis());
      AnimationNeedsResample();
      // As with SVGSVGElement::SetCurrentTime, we need to trigger
      // a synchronous sample now.
      FlushAnimations();
    }
    // else, silently fail. We mustn't be part of an SVG document fragment that
    // is attached to the document tree so there's nothing we can do here
  } else {
    BeginElement(IgnoreErrors());
  }
}

//----------------------------------------------------------------------
// Implementation helpers

SMILTimeContainer* SVGAnimationElement::GetTimeContainer() {
  SVGSVGElement* element = SVGContentUtils::GetOuterSVGElement(this);

  if (element) {
    return element->GetTimedDocumentRoot();
  }

  return nullptr;
}

void SVGAnimationElement::BeginElementAt(float offset, ErrorResult& rv) {
  // Make sure the timegraph is up-to-date
  FlushAnimations();

  // This will fail if we're not attached to a time container (SVG document
  // fragment).
  rv = mTimedElement.BeginElementAt(offset);
  if (rv.Failed()) return;

  AnimationNeedsResample();
  // Force synchronous sample so that events resulting from this call arrive in
  // the expected order and we get an up-to-date paint.
  FlushAnimations();
}

void SVGAnimationElement::EndElementAt(float offset, ErrorResult& rv) {
  // Make sure the timegraph is up-to-date
  FlushAnimations();

  rv = mTimedElement.EndElementAt(offset);
  if (rv.Failed()) return;

  AnimationNeedsResample();
  // Force synchronous sample
  FlushAnimations();
}

bool SVGAnimationElement::IsEventAttributeNameInternal(nsAtom* aName) {
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SMIL);
}

void SVGAnimationElement::UpdateHrefTarget(const nsAString& aHrefStr) {
  if (nsContentUtils::IsLocalRefURL(aHrefStr)) {
    mHrefTarget.ResetWithLocalRef(*this, aHrefStr);
  } else {
    mHrefTarget.Unlink();
  }
  AnimationTargetChanged();
}

void SVGAnimationElement::AnimationTargetChanged() {
  mTimedElement.HandleTargetElementChange(GetTargetElementContent());
  AnimationNeedsResample();
}

}  // namespace mozilla::dom
