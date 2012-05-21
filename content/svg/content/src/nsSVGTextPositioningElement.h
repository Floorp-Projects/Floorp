/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGTEXTPOSITIONINGELEMENTBASE_H__
#define __NS_SVGTEXTPOSITIONINGELEMENTBASE_H__

#include "nsIDOMSVGTextPositionElem.h"
#include "nsSVGTextContentElement.h"
#include "SVGAnimatedLengthList.h"
#include "SVGAnimatedNumberList.h"

class nsSVGElement;

namespace mozilla {
class SVGAnimatedLengthList;
}

typedef nsSVGTextContentElement nsSVGTextPositioningElementBase;

/**
 * Note that nsSVGTextElement does not inherit this class - it reimplements it
 * instead (see its documenting comment). The upshot is that any changes to
 * this class also need to be made in nsSVGTextElement.
 */
class nsSVGTextPositioningElement : public nsSVGTextPositioningElementBase
{
public:
  NS_DECL_NSIDOMSVGTEXTPOSITIONINGELEMENT

protected:

  nsSVGTextPositioningElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGTextPositioningElementBase(aNodeInfo)
  {}

  virtual LengthListAttributesInfo GetLengthListInfo();
  virtual NumberListAttributesInfo GetNumberListInfo();

  // nsIDOMSVGTextPositioning properties:

  enum { X, Y, DX, DY };
  SVGAnimatedLengthList mLengthListAttributes[4];
  static LengthListInfo sLengthListInfo[4];

  enum { ROTATE };
  SVGAnimatedNumberList mNumberListAttributes[1];
  static NumberListInfo sNumberListInfo[1];
};

#endif
