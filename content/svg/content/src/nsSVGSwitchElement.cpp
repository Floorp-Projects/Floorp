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
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#include "nsSVGSwitchElement.h"
#include "nsIFrame.h"
#include "nsISVGChildFrame.h"
#include "nsSVGUtils.h"

PRBool 
NS_SVG_PassesConditionalProcessingTests(nsIContent *aContent);

////////////////////////////////////////////////////////////////////////
// implementation


NS_IMPL_NS_NEW_SVG_ELEMENT(Switch)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGSwitchElement,nsSVGSwitchElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGSwitchElement,nsSVGSwitchElementBase)

NS_INTERFACE_TABLE_HEAD(nsSVGSwitchElement)
  NS_NODE_INTERFACE_TABLE4(nsSVGSwitchElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGSwitchElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGSwitchElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGSwitchElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGSwitchElement::nsSVGSwitchElement(nsINodeInfo *aNodeInfo)
  : nsSVGSwitchElementBase(aNodeInfo)
{
}

void
nsSVGSwitchElement::MaybeInvalidate()
{
  // We don't reuse UpdateActiveChild() and check if mActiveChild has changed
  // to determine if we should call nsSVGUtils::UpdateGraphic. If we did that,
  // nsSVGUtils::UpdateGraphic would not invalidate the old mActiveChild area!

  PRUint32 count = GetChildCount();
  for (PRUint32 i = 0; i < count; i++) {
    nsIContent * child = GetChildAt(i);
    if (NS_SVG_PassesConditionalProcessingTests(child)) {

      if (mActiveChild == child) {
        return;
      }

      nsIFrame *frame = GetPrimaryFrame();
      if (frame) {
        nsISVGChildFrame* svgFrame = nsnull;

        CallQueryInterface(frame, &svgFrame);
        if (svgFrame) {
          nsSVGUtils::UpdateGraphic(svgFrame);
        }
      }
      return;
    }
  }
}

void
nsSVGSwitchElement::UpdateActiveChild()
{
  PRUint32 count = GetChildCount();
  for (PRUint32 i = 0; i < count; i++) {
    nsIContent * child = GetChildAt(i);
    if (NS_SVG_PassesConditionalProcessingTests(child)) {
      mActiveChild = child;
      return;
    }
  }
  mActiveChild = nsnull;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGSwitchElement)

//----------------------------------------------------------------------
// nsINode methods

nsresult
nsSVGSwitchElement::InsertChildAt(nsIContent* aKid,
                                  PRUint32 aIndex,
                                  PRBool aNotify)
{
  nsresult rv = nsSVGSwitchElementBase::InsertChildAt(aKid, aIndex, aNotify);
  if (NS_SUCCEEDED(rv)) {
    MaybeInvalidate();
  }
  return rv;
}

nsresult
nsSVGSwitchElement::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  nsresult rv = nsSVGSwitchElementBase::RemoveChildAt(aIndex, aNotify);
  if (NS_SUCCEEDED(rv)) {
    MaybeInvalidate();
  }
  return rv;
}
 
//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
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

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGSwitchElementBase::IsAttributeMapped(name);
}
