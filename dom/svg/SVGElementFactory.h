/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGElementFactory_h
#define mozilla_dom_SVGElementFactory_h

class nsIAtom;

namespace mozilla {
namespace dom {

class SVGElementFactory {
public:
  static void Init();
  static void Shutdown();
};

typedef nsresult (*SVGContentCreatorFunction)(
  nsIContent** aResult,
  already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
  mozilla::dom::FromParser aFromParser);

} // namespace dom
} // namespace mozilla

#define SVG_TAG(_tag, _classname)                                              \
  nsresult NS_NewSVG##_classname##Element(                                     \
    nsIContent** aResult,                                                      \
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,                      \
    mozilla::dom::FromParser aFromParser);

#define SVG_FROM_PARSER_TAG(_tag, _classname)                                  \
  nsresult NS_NewSVG##_classname##Element(                                     \
    nsIContent** aResult,                                                      \
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,                      \
    mozilla::dom::FromParser aFromParser);
#include "SVGTagList.h"
#undef SVG_TAG
#undef SVG_FROM_PARSER_TAG

nsresult
NS_NewSVGUnknownElement(nsIContent** aResult,
                        already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                        mozilla::dom::FromParser aFromParser);

#endif /* mozilla_dom_SVGElementFactory_h */
