/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGSwitchElement.h"
#include "DOMSVGTests.h"
#include "nsIFrame.h"
#include "nsSVGUtils.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

////////////////////////////////////////////////////////////////////////
// implementation


NS_IMPL_NS_NEW_SVG_ELEMENT(Switch)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_CYCLE_COLLECTION_CLASS(nsSVGSwitchElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsSVGSwitchElement,
                                                  nsSVGSwitchElementBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mActiveChild)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsSVGSwitchElement,
                                                nsSVGSwitchElementBase)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mActiveChild)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(nsSVGSwitchElement,nsSVGSwitchElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGSwitchElement,nsSVGSwitchElementBase)

DOMCI_NODE_DATA(SVGSwitchElement, nsSVGSwitchElement)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsSVGSwitchElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGSwitchElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGSwitchElement,
                           nsIDOMSVGTests)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGSwitchElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGSwitchElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGSwitchElement::nsSVGSwitchElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGSwitchElementBase(aNodeInfo)
{
}

void
nsSVGSwitchElement::MaybeInvalidate()
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
    nsSVGUtils::InvalidateAndScheduleBoundsUpdate(frame);
  }

  mActiveChild = newActiveChild;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGSwitchElement)

//----------------------------------------------------------------------
// nsINode methods

nsresult
nsSVGSwitchElement::InsertChildAt(nsIContent* aKid,
                                  PRUint32 aIndex,
                                  bool aNotify)
{
  nsresult rv = nsSVGSwitchElementBase::InsertChildAt(aKid, aIndex, aNotify);
  if (NS_SUCCEEDED(rv)) {
    MaybeInvalidate();
  }
  return rv;
}

void
nsSVGSwitchElement::RemoveChildAt(PRUint32 aIndex, bool aNotify)
{
  nsSVGSwitchElementBase::RemoveChildAt(aIndex, aNotify);
  MaybeInvalidate();
}
 
//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGSwitchElement::IsAttributeMapped(const nsIAtom* name) const
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
    nsSVGSwitchElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// Implementation Helpers:

nsIContent *
nsSVGSwitchElement::FindActiveChild() const
{
  bool allowReorder = AttrValueIs(kNameSpaceID_None,
                                    nsGkAtoms::allowReorder,
                                    nsGkAtoms::yes, eCaseMatters);

  const nsAdoptingString& acceptLangs =
    Preferences::GetLocalizedString("intl.accept_languages");

  if (allowReorder && !acceptLangs.IsEmpty()) {
    PRInt32 bestLanguagePreferenceRank = -1;
    nsIContent *bestChild = nsnull;
    for (nsIContent* child = nsINode::GetFirstChild();
         child;
         child = child->GetNextSibling()) {

      if (!child->IsElement()) {
        continue;
      }
      nsCOMPtr<DOMSVGTests> tests(do_QueryInterface(child));
      if (tests) {
        if (tests->PassesConditionalProcessingTests(
                            DOMSVGTests::kIgnoreSystemLanguage)) {
          PRInt32 languagePreferenceRank =
              tests->GetBestLanguagePreferenceRank(acceptLangs);
          switch (languagePreferenceRank) {
          case 0:
            // best possible match
            return child;
          case -1:
            // not found
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
    return bestChild;
  }

  for (nsIContent* child = nsINode::GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (!child->IsElement()) {
      continue;
    }
    nsCOMPtr<DOMSVGTests> tests(do_QueryInterface(child));
    if (!tests || tests->PassesConditionalProcessingTests(&acceptLangs)) {
      return child;
    }
  }
  return nsnull;
}
