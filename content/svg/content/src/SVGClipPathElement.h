/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGClipPathElement_h
#define mozilla_dom_SVGClipPathElement_h

#include "nsIDOMSVGUnitTypes.h"
#include "nsSVGEnum.h"
#include "mozilla/dom/SVGTransformableElement.h"

class nsSVGClipPathFrame;

nsresult NS_NewSVGClipPathElement(nsIContent **aResult,
                                  already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGTransformableElement SVGClipPathElementBase;

class SVGClipPathElement MOZ_FINAL : public SVGClipPathElementBase,
                                     public nsIDOMSVGElement,
                                     public nsIDOMSVGUnitTypes
{
  friend class ::nsSVGClipPathFrame;

protected:
  friend nsresult (::NS_NewSVGClipPathElement(nsIContent **aResult,
                                              already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGClipPathElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JSObject *scope) MOZ_OVERRIDE;

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_FORWARD_NSIDOMSVGELEMENT(SVGClipPathElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> ClipPathUnits();

protected:

  enum { CLIPPATHUNITS };
  nsSVGEnum mEnumAttributes[1];
  static EnumInfo sEnumInfo[1];

  virtual EnumAttributesInfo GetEnumInfo();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGClipPathElement_h
