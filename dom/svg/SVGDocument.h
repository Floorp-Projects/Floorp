/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGDOCUMENT_H_
#define DOM_SVG_SVGDOCUMENT_H_

#include "mozilla/Attributes.h"
#include "mozilla/dom/XMLDocument.h"

namespace mozilla {

namespace dom {

class SVGElement;
class SVGForeignObjectElement;

class SVGDocument final : public XMLDocument {
 public:
  SVGDocument() : XMLDocument("image/svg+xml") { mType = eSVG; }

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
};

inline SVGDocument* Document::AsSVGDocument() {
  MOZ_ASSERT(IsSVGDocument());
  return static_cast<SVGDocument*>(this);
}

inline const SVGDocument* Document::AsSVGDocument() const {
  MOZ_ASSERT(IsSVGDocument());
  return static_cast<const SVGDocument*>(this);
}

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGDOCUMENT_H_
