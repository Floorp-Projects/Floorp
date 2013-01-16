/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGSymbolElement_h
#define mozilla_dom_SVGSymbolElement_h

#include "nsIDOMSVGSymbolElement.h"
#include "DOMSVGTests.h"
#include "nsSVGElement.h"
#include "nsSVGViewBox.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsGkAtoms.h"

nsresult NS_NewSVGSymbolElement(nsIContent **aResult,
                                already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGElement SVGSymbolElementBase;

class SVGSymbolElement MOZ_FINAL : public SVGSymbolElementBase,
                                   public nsIDOMSVGSymbolElement,
                                   public DOMSVGTests,
                                   public nsIDOMSVGFitToViewBox
{
protected:
  friend nsresult (::NS_NewSVGSymbolElement(nsIContent **aResult,
                                            already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGSymbolElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual JSObject* WrapNode(JSContext *cx, JSObject *scope, bool *triedToWrap) MOZ_OVERRIDE;

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSYMBOLELEMENT
  NS_DECL_NSIDOMSVGFITTOVIEWBOX

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGElement::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedRect> ViewBox();
  already_AddRefed<DOMSVGAnimatedPreserveAspectRatio> PreserveAspectRatio();

protected:
  virtual nsSVGViewBox *GetViewBox();
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio();

  nsSVGViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGSymbolElement_h
