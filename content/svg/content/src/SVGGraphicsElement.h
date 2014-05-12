/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGGraphicsElement_h
#define mozilla_dom_SVGGraphicsElement_h

#include "mozilla/dom/SVGTests.h"
#include "mozilla/dom/SVGTransformableElement.h"

namespace mozilla {
namespace dom {

typedef SVGTransformableElement SVGGraphicsElementBase;

class SVGGraphicsElement : public SVGGraphicsElementBase,
                           public SVGTests
{
protected:
  SVGGraphicsElement(already_AddRefed<nsINodeInfo>& aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGGraphicsElement_h
