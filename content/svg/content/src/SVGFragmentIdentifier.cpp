/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGFragmentIdentifier.h"
#include "mozilla/CharTokenizer.h"
#include "nsIDOMSVGDocument.h"
#include "nsSVGSVGElement.h"
#include "nsSVGViewElement.h"

using namespace mozilla;

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
            static_cast<nsSVGViewElement*>(element) : nullptr;
}

void 
SVGFragmentIdentifier::SaveOldPreserveAspectRatio(nsSVGSVGElement *root)
{
  if (root->mPreserveAspectRatio.IsExplicitlySet()) {
    root->SetPreserveAspectRatioProperty(root->mPreserveAspectRatio.GetBaseValue());
  }
}

void 
SVGFragmentIdentifier::RestoreOldPreserveAspectRatio(nsSVGSVGElement *root)
{
  const SVGPreserveAspectRatio *oldPARPtr = root->GetPreserveAspectRatioProperty();
  if (oldPARPtr) {
    root->mPreserveAspectRatio.SetBaseValue(*oldPARPtr, root);
  } else if (root->mPreserveAspectRatio.IsExplicitlySet()) {
    root->RemoveAttribute(NS_LITERAL_STRING("preserveAspectRatio"));
  }
}

void 
SVGFragmentIdentifier::SaveOldViewBox(nsSVGSVGElement *root)
{
  if (root->mViewBox.IsExplicitlySet()) {
    root->SetViewBoxProperty(root->mViewBox.GetBaseValue());
  }
}

void 
SVGFragmentIdentifier::RestoreOldViewBox(nsSVGSVGElement *root)
{
  const nsSVGViewBoxRect *oldViewBoxPtr = root->GetViewBoxProperty();
  if (oldViewBoxPtr) {
    root->mViewBox.SetBaseValue(*oldViewBoxPtr, root);
  } else if (root->mViewBox.IsExplicitlySet()) {
    root->RemoveAttribute(NS_LITERAL_STRING("viewBox"));
  }
}

void 
SVGFragmentIdentifier::SaveOldZoomAndPan(nsSVGSVGElement *root)
{
  if (root->mEnumAttributes[nsSVGSVGElement::ZOOMANDPAN].IsExplicitlySet()) {
    root->SetZoomAndPanProperty(root->mEnumAttributes[nsSVGSVGElement::ZOOMANDPAN].GetBaseValue());
  }
}

void 
SVGFragmentIdentifier::RestoreOldZoomAndPan(nsSVGSVGElement *root)
{
  uint16_t oldZoomAndPan = root->GetZoomAndPanProperty();
  if (oldZoomAndPan != nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_UNKNOWN) {
    root->mEnumAttributes[nsSVGSVGElement::ZOOMANDPAN].SetBaseValue(oldZoomAndPan, root);
  } else if (root->mEnumAttributes[nsSVGSVGElement::ZOOMANDPAN].IsExplicitlySet()) {
    root->RemoveAttribute(NS_LITERAL_STRING("zoomAndPan"));
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

  const nsAString *viewBoxParams = nullptr;
  const nsAString *preserveAspectRatioParams = nullptr;
  const nsAString *zoomAndPanParams = nullptr;

  // Each token is a SVGViewAttribute
  int32_t bracketPos = aViewSpec.FindChar('(');
  CharTokenizer<';'>tokenizer(
    Substring(aViewSpec, bracketPos + 1, aViewSpec.Length() - bracketPos - 2));

  if (!tokenizer.hasMoreTokens()) {
    return false;
  }
  do {

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
  } while (tokenizer.hasMoreTokens());

  if (viewBoxParams) {
    root->mViewBox.SetBaseValueString(*viewBoxParams, root);
  } else {
    RestoreOldViewBox(root);
  }

  if (preserveAspectRatioParams) {
    root->mPreserveAspectRatio.SetBaseValueString(*preserveAspectRatioParams, root);
  } else {
    RestoreOldPreserveAspectRatio(root);
  }

  if (zoomAndPanParams) {
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

  if (!rootElement->mUseCurrentView) {
    SaveOldViewBox(rootElement);
    SaveOldPreserveAspectRatio(rootElement);
    SaveOldZoomAndPan(rootElement);
  }

  const nsSVGViewElement *viewElement = GetViewElement(aDocument, aAnchorName);

  if (viewElement) {
    if (viewElement->mViewBox.IsExplicitlySet()) {
      rootElement->mViewBox.SetBaseValue(
        viewElement->mViewBox.GetBaseValue(), rootElement);
    } else {
      RestoreOldViewBox(rootElement);
    }
    if (viewElement->mPreserveAspectRatio.IsExplicitlySet()) {
      rootElement->mPreserveAspectRatio.SetBaseValue(
        viewElement->mPreserveAspectRatio.GetBaseValue(), rootElement);
    } else {
      RestoreOldPreserveAspectRatio(rootElement);
    }
    if (viewElement->mEnumAttributes[nsSVGViewElement::ZOOMANDPAN].IsExplicitlySet()) {
      rootElement->mEnumAttributes[nsSVGSVGElement::ZOOMANDPAN].SetBaseValue(
        viewElement->mEnumAttributes[nsSVGViewElement::ZOOMANDPAN].GetBaseValue(), rootElement);
    } else {
      RestoreOldZoomAndPan(rootElement);
    }
    rootElement->mUseCurrentView = true;
    return true;
  }

  rootElement->mUseCurrentView = ProcessSVGViewSpec(aAnchorName, rootElement);
  if (rootElement->mUseCurrentView) {
    return true;
  }
  RestoreOldViewBox(rootElement);
  rootElement->ClearViewBoxProperty();
  RestoreOldPreserveAspectRatio(rootElement);
  rootElement->ClearPreserveAspectRatioProperty();
  RestoreOldZoomAndPan(rootElement);
  rootElement->ClearZoomAndPanProperty();
  return false;
}
