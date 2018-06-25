/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGSwitchElement.h"

#include "nsLayoutUtils.h"
#include "nsSVGUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/SVGSwitchElementBinding.h"

class nsIFrame;

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Switch)

namespace mozilla {
namespace dom {

JSObject*
SVGSwitchElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGSwitchElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_CYCLE_COLLECTION_INHERITED(SVGSwitchElement, SVGSwitchElementBase,
                                   mActiveChild)

NS_IMPL_ADDREF_INHERITED(SVGSwitchElement,SVGSwitchElementBase)
NS_IMPL_RELEASE_INHERITED(SVGSwitchElement,SVGSwitchElementBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGSwitchElement)
NS_INTERFACE_MAP_END_INHERITING(SVGSwitchElementBase)

//----------------------------------------------------------------------
// Implementation

SVGSwitchElement::SVGSwitchElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGSwitchElementBase(aNodeInfo)
{
}

SVGSwitchElement::~SVGSwitchElement()
{
}

void
SVGSwitchElement::MaybeInvalidate()
{
  // We must not change mActiveChild until after
  // InvalidateAndScheduleBoundsUpdate has been called, otherwise
  // it will not correctly invalidate the old mActiveChild area.

  nsIContent *newActiveChild = FindActiveChild();

  if (newActiveChild == mActiveChild) {
    return;
  }

  nsIFrame *frame = GetPrimaryFrame();
  if (frame) {
    nsLayoutUtils::PostRestyleEvent(
      this, nsRestyleHint(0),
      nsChangeHint_InvalidateRenderingObservers);
    nsSVGUtils::ScheduleReflowSVG(frame);
  }

  mActiveChild = newActiveChild;
}

//----------------------------------------------------------------------
// nsINode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGSwitchElement)

//----------------------------------------------------------------------
// nsINode methods

nsresult
SVGSwitchElement::InsertChildBefore(nsIContent* aKid, nsIContent* aBeforeThis,
                                    bool aNotify)
{
  nsresult rv =
    SVGSwitchElementBase::InsertChildBefore(aKid, aBeforeThis, aNotify);
  if (NS_SUCCEEDED(rv)) {
    MaybeInvalidate();
  }
  return rv;
}

void
SVGSwitchElement::RemoveChildNode(nsIContent* aKid, bool aNotify)
{
  SVGSwitchElementBase::RemoveChildNode(aKid, aNotify);
  MaybeInvalidate();
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGSwitchElement::IsAttributeMapped(const nsAtom* name) const
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
    SVGSwitchElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// Implementation Helpers:

nsIContent *
SVGSwitchElement::FindActiveChild() const
{
  nsAutoString acceptLangs;
  Preferences::GetLocalizedString("intl.accept_languages", acceptLangs);

  if (!acceptLangs.IsEmpty()) {
    int32_t bestLanguagePreferenceRank = -1;
    nsIContent *bestChild = nullptr;
    nsIContent *defaultChild = nullptr;
    for (nsIContent* child = nsINode::GetFirstChild();
         child;
         child = child->GetNextSibling()) {

      if (!child->IsElement()) {
        continue;
      }
      nsCOMPtr<SVGTests> tests(do_QueryInterface(child));
      if (tests) {
        if (tests->PassesConditionalProcessingTests(
                            SVGTests::kIgnoreSystemLanguage)) {
          int32_t languagePreferenceRank =
              tests->GetBestLanguagePreferenceRank(acceptLangs);
          switch (languagePreferenceRank) {
          case 0:
            // best possible match
            return child;
          case -1:
            // no match
            break;
          case -2:
            // no systemLanguage attribute. If there's nothing better
            // we'll use the first such child.
            if (!defaultChild) {
              defaultChild = child;
            }
            break;
          default:
            if (bestLanguagePreferenceRank == -1 ||
                languagePreferenceRank < bestLanguagePreferenceRank) {
              bestLanguagePreferenceRank = languagePreferenceRank;
              bestChild = child;
            }
            break;
          }
        }
      } else if (!bestChild) {
         bestChild = child;
      }
    }
    return bestChild ? bestChild : defaultChild;
  }

  for (nsIContent* child = nsINode::GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (!child->IsElement()) {
      continue;
    }
    nsCOMPtr<SVGTests> tests(do_QueryInterface(child));
    if (!tests || tests->PassesConditionalProcessingTests(&acceptLangs)) {
      return child;
    }
  }
  return nullptr;
}

} // namespace dom
} // namespace mozilla

