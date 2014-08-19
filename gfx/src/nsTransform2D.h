/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTransform2D_h___
#define nsTransform2D_h___

#include "gfxCore.h"
#include "nsCoord.h"

class NS_GFX nsTransform2D
{
private:
 /**
  * This represents the following matrix (note that the order of row/column
  * indices is opposite to usual notation)
  *
  *      / m00   0   m20  \
  * M =  |  0   m11  m21  |
  *      \  0    0    1   /
  *
  * Transformation of a coordinate (x, y) is obtained by setting
  * v = (x, y, 1)^T and evaluating  M . v
  **/

  float     m00, m11, m20, m21;

public:
  nsTransform2D(void)                         { m20 = m21 = 0.0f; m00 = m11 = 1.0f; }

  ~nsTransform2D(void)                        { }

 /**
  * set this transform to a translation
  *
  * @param      tx, x translation
  * @param      ty, y translation
  **/

  void SetToTranslate(float tx, float ty)    { m00 = m11 = 1.0f; m20 = tx; m21 = ty; }
  
 /**
  * get the translation portion of this transform
  *
  * @param      pt, Point to return translation values in
  **/

  void GetTranslationCoord(nscoord *ptX, nscoord *ptY) const { *ptX = NSToCoordRound(m20); *ptY = NSToCoordRound(m21); }

 /**
  * apply matrix to vector
  *
  * @param    pt Point to transform
  **/

  void TransformCoord(nscoord *ptX, nscoord *ptY) const;

 /**
  * apply matrix to rect
  *
  * @param    rect Rect to transform
  **/

  void TransformCoord(nscoord *aX, nscoord *aY, nscoord *aWidth, nscoord *aHeight) const;

 /**
  * add a scale to a Transform via x, y pair
  *
  * @param    ptX x value to add as x scale
  * @param    ptY y value to add as y scale
  **/

  void AddScale(float ptX, float ptY) { m00 *= ptX; m11 *= ptY; }

 /**
  * Set the scale (overriding any previous calls to AddScale, but leaving
  * any existing translation).
  *
  * @param    ptX x value to add as x scale
  * @param    ptY y value to add as y scale
  **/

  void SetScale(float ptX, float ptY) { m00 = ptX; m11 = ptY; }
};

#endif
