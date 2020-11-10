/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedClassOrString.h"

#include "DOMSVGAnimatedString.h"
#include "SVGAttrTearoffTable.h"

namespace mozilla {

static SVGAttrTearoffTable<SVGAnimatedClassOrString, dom::DOMSVGAnimatedString>
    sSVGAnimatedClassOrStringTearoffTable;

already_AddRefed<dom::DOMSVGAnimatedString>
SVGAnimatedClassOrString::ToDOMAnimatedString(SVGElement* aSVGElement) {
  RefPtr<dom::DOMSVGAnimatedString> domAnimatedString =
      sSVGAnimatedClassOrStringTearoffTable.GetTearoff(this);
  if (!domAnimatedString) {
    domAnimatedString = new dom::DOMSVGAnimatedString(this, aSVGElement);
    sSVGAnimatedClassOrStringTearoffTable.AddTearoff(this, domAnimatedString);
  }

  return domAnimatedString.forget();
}

void SVGAnimatedClassOrString::RemoveTearoff() {
  sSVGAnimatedClassOrStringTearoffTable.RemoveTearoff(this);
}

}  // namespace mozilla
