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
 * The Initial Developer of the Original Code is
 * Robert Longson <longsonr@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2012
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

#include "SVGFragmentIdentifier.h"
#include "mozilla/CharTokenizer.h"
#include "nsIDOMSVGDocument.h"
#include "nsSVGSVGElement.h"
#include "nsSVGViewElement.h"

using namespace mozilla;

static nsSVGEnumMapping sZoomAndPanMap[] = {
  {&nsGkAtoms::disable, nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_DISABLE},
  {&nsGkAtoms::magnify, nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY},
  {nsnull, 0}
};

static bool
IsMatchingParameter(const nsAString &aString, const nsAString &aParameterName)
{
  // The first two tests ensure aString.Length() > aParameterName.Length()
  // so it's then safe to do the third test
  return StringBeginsWith(aString, aParameterName) &&
         aString.Last() == ')' &&
         aString.CharAt(aParameterName.Length()) == '(';
}

static nsSVGViewElement*
GetViewElement(nsIDocument *aDocument, const nsAString &aId)
{
  dom::Element* element = aDocument->GetElementById(aId);
  return (element && element->IsSVG(nsGkAtoms::view)) ?
            static_cast<nsSVGViewElement*>(element) : nsnull;
}

void 
SVGFragmentIdentifier::SaveOldPreserveAspectRatio(nsSVGSVGElement *root)
{
  const SVGPreserveAspectRatio *oldPARPtr = root->GetPreserveAspectRatioProperty();
  if (!oldPARPtr) {
    root->SetPreserveAspectRatioProperty(root->mPreserveAspectRatio.GetBaseValue());
  }
}

void 
SVGFragmentIdentifier::RestoreOldPreserveAspectRatio(nsSVGSVGElement *root)
{
  const SVGPreserveAspectRatio *oldPARPtr = root->GetPreserveAspectRatioProperty();
  if (oldPARPtr) {
    root->mPreserveAspectRatio.SetBaseValue(*oldPARPtr, root);
    root->ClearPreserveAspectRatioProperty();
  }
}

void 
SVGFragmentIdentifier::SaveOldViewBox(nsSVGSVGElement *root)
{
  const nsSVGViewBoxRect *oldViewBoxPtr = root->GetViewBoxProperty();
  if (!oldViewBoxPtr) {
    root->SetViewBoxProperty(root->mViewBox.GetBaseValue());
  }
}

void 
SVGFragmentIdentifier::RestoreOldViewBox(nsSVGSVGElement *root)
{
  const nsSVGViewBoxRect *oldViewBoxPtr = root->GetViewBoxProperty();
  if (oldViewBoxPtr) {
    root->mViewBox.SetBaseValue(*oldViewBoxPtr, root);
    root->ClearViewBoxProperty();
  }
}

void 
SVGFragmentIdentifier::SaveOldZoomAndPan(nsSVGSVGElement *root)
{
  const PRUint16 *oldZoomAndPanPtr = root->GetZoomAndPanProperty();
  if (!oldZoomAndPanPtr) {
    root->SetZoomAndPanProperty(root->mEnumAttributes[nsSVGSVGElement::ZOOMANDPAN].GetBaseValue());
  }
}

void 
SVGFragmentIdentifier::RestoreOldZoomAndPan(nsSVGSVGElement *root)
{
  const PRUint16 *oldZoomAndPanPtr = root->GetZoomAndPanProperty();
  if (oldZoomAndPanPtr) {
    root->mEnumAttributes[nsSVGSVGElement::ZOOMANDPAN].SetBaseValue(*oldZoomAndPanPtr, root);
    root->ClearZoomAndPanProperty();
  }
}

bool
SVGFragmentIdentifier::ProcessSVGViewSpec(const nsAString &aViewSpec,
                                          nsSVGSVGElement *root)
{
  if (!IsMatchingParameter(aViewSpec, NS_LITERAL_STRING("svgView"))) {
    return false;
  }

  // SVGViewAttribute may occur in any order, but each type may only occur at most one time
  // in a correctly formed SVGViewSpec.
  // If we encounter any element more than once or get any syntax errors we're going to
  // return without updating the root element

  const nsAString *viewBoxParams = nsnull;
  const nsAString *preserveAspectRatioParams = nsnull;
  const nsAString *zoomAndPanParams = nsnull;

  // Each token is a SVGViewAttribute
  PRInt32 bracketPos = aViewSpec.FindChar('(');
  CharTokenizer<';'>tokenizer(
    Substring(aViewSpec, bracketPos + 1, aViewSpec.Length() - bracketPos - 2));

  while (tokenizer.hasMoreTokens()) {

    nsAutoString token(tokenizer.nextToken());

    bracketPos = token.FindChar('(');
    if (bracketPos < 1 || token.Last() != ')') {
      // invalid SVGViewAttribute syntax
      return false;
    }

    const nsAString &params =
      Substring(token, bracketPos + 1, token.Length() - bracketPos - 2);

    if (IsMatchingParameter(token, NS_LITERAL_STRING("viewBox"))) {
      if (viewBoxParams) {
        return false;
      }
      viewBoxParams = &params;
    } else if (IsMatchingParameter(token, NS_LITERAL_STRING("preserveAspectRatio"))) {
      if (preserveAspectRatioParams) {
        return false;
      }
      preserveAspectRatioParams = &params;
    } else if (IsMatchingParameter(token, NS_LITERAL_STRING("zoomAndPan"))) {
      if (zoomAndPanParams) {
        return false;
      }
      zoomAndPanParams = &params;
    } else {
      // We don't support transform or viewTarget currently
      return false;
    }
  }

  const nsSVGViewBoxRect *oldViewBoxPtr = root->GetViewBoxProperty();
  if (viewBoxParams) {
    SaveOldViewBox(root);
    root->mViewBox.SetBaseValueString(*viewBoxParams, root);
  } else {
    RestoreOldViewBox(root);
  }

  const SVGPreserveAspectRatio *oldPARPtr = root->GetPreserveAspectRatioProperty();
  if (preserveAspectRatioParams) {
    SaveOldPreserveAspectRatio(root);
    root->mPreserveAspectRatio.SetBaseValueString(*preserveAspectRatioParams, root);
  } else {
    RestoreOldPreserveAspectRatio(root);
  }

  const PRUint16 *oldZoomAndPanPtr = root->GetZoomAndPanProperty();
  if (zoomAndPanParams) {
    SaveOldZoomAndPan(root);
    nsCOMPtr<nsIAtom> valAtom = do_GetAtom(*zoomAndPanParams);
    const nsSVGEnumMapping *mapping = root->sZoomAndPanMap;
    while (mapping->mKey) {
      if (valAtom == *(mapping->mKey)) {
        root->mEnumAttributes[nsSVGSVGElement::ZOOMANDPAN].SetBaseValue(mapping->mVal, root);
        break;
      }
      mapping++;
    }
  } else {
    RestoreOldZoomAndPan(root);
  }

  return true;
}

bool
SVGFragmentIdentifier::ProcessFragmentIdentifier(nsIDocument *aDocument,
                                                 const nsAString &aAnchorName)
{
  NS_ABORT_IF_FALSE(aDocument->GetRootElement()->IsSVG(nsGkAtoms::svg),
                    "expecting an SVG root element");

  nsSVGSVGElement *rootElement =
    static_cast<nsSVGSVGElement*>(aDocument->GetRootElement());

  const nsSVGViewElement *viewElement = GetViewElement(aDocument, aAnchorName);

  if (viewElement) {
    if (viewElement->mViewBox.IsExplicitlySet()) {
      SaveOldViewBox(rootElement);
      rootElement->mViewBox.SetBaseValue(
        viewElement->mViewBox.GetBaseValue(), rootElement);
    } else {
      RestoreOldViewBox(rootElement);
    }
    if (viewElement->mPreserveAspectRatio.IsExplicitlySet()) {
      SaveOldPreserveAspectRatio(rootElement);
      rootElement->mPreserveAspectRatio.SetBaseValue(
        viewElement->mPreserveAspectRatio.GetBaseValue(), rootElement);
    } else {
      RestoreOldPreserveAspectRatio(rootElement);
    }
    if (viewElement->mEnumAttributes[nsSVGViewElement::ZOOMANDPAN].IsExplicitlySet()) {
      SaveOldZoomAndPan(rootElement);
      rootElement->mEnumAttributes[nsSVGSVGElement::ZOOMANDPAN].SetBaseValue(
        viewElement->mEnumAttributes[nsSVGViewElement::ZOOMANDPAN].GetBaseValue(), rootElement);
    } else {
      RestoreOldZoomAndPan(rootElement);
    }
    return true;
  }

  if (ProcessSVGViewSpec(aAnchorName, rootElement)) {
    return true;
  }
  RestoreOldViewBox(rootElement);
  RestoreOldPreserveAspectRatio(rootElement);
  RestoreOldZoomAndPan(rootElement);
  return false;
}
