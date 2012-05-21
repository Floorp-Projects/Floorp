/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGPATHGEOMETRYELEMENT_H__
#define __NS_SVGPATHGEOMETRYELEMENT_H__

#include "DOMSVGTests.h"
#include "gfxMatrix.h"
#include "nsSVGGraphicElement.h"
#include "nsTArray.h"

struct nsSVGMark {
  float x, y, angle;
  nsSVGMark(float aX, float aY, float aAngle) :
    x(aX), y(aY), angle(aAngle) {}
};

class gfxContext;

typedef nsSVGGraphicElement nsSVGPathGeometryElementBase;

class nsSVGPathGeometryElement : public nsSVGPathGeometryElementBase,
                                 public DOMSVGTests
{
public:
  nsSVGPathGeometryElement(already_AddRefed<nsINodeInfo> aNodeInfo);

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  virtual bool AttributeDefinesGeometry(const nsIAtom *aName);
  virtual bool IsMarkable();
  virtual void GetMarkPoints(nsTArray<nsSVGMark> *aMarks);
  virtual void ConstructPath(gfxContext *aCtx) = 0;
  virtual already_AddRefed<gfxFlattenedPath> GetFlattenedPath(const gfxMatrix &aMatrix);
};

#endif
