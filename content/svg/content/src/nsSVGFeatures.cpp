/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains code to help implement the Conditional Processing
 * section of the SVG specification (i.e. the <switch> element and the
 * requiredFeatures, requiredExtensions and systemLanguage attributes).
 *
 *   http://www.w3.org/TR/SVG11/struct.html#ConditionalProcessing
 */

#include "nsSVGFeatures.h"
#include "nsIContent.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

/*static*/ bool
nsSVGFeatures::HasFeature(nsISupports* aObject, const nsAString& aFeature)
{
  if (aFeature.EqualsLiteral("http://www.w3.org/TR/SVG11/feature#Script")) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aObject));
    if (content) {
      nsIDocument *doc = content->GetCurrentDoc();
      if (doc && doc->IsResourceDoc()) {
        // no scripting in SVG images or external resource documents
        return false;
      }
    }
    return Preferences::GetBool("javascript.enabled", false);
  }
#define SVG_SUPPORTED_FEATURE(str) if (aFeature.EqualsLiteral(str)) return true;
#define SVG_UNSUPPORTED_FEATURE(str)
#include "nsSVGFeaturesList.h"
#undef SVG_SUPPORTED_FEATURE
#undef SVG_UNSUPPORTED_FEATURE
  return false;
}

/*static*/ bool
nsSVGFeatures::HasExtension(const nsAString& aExtension)
{
#define SVG_SUPPORTED_EXTENSION(str) if (aExtension.EqualsLiteral(str)) return true;
  SVG_SUPPORTED_EXTENSION("http://www.w3.org/1999/xhtml")
  SVG_SUPPORTED_EXTENSION("http://www.w3.org/1998/Math/MathML")
#undef SVG_SUPPORTED_EXTENSION

  return false;
}
