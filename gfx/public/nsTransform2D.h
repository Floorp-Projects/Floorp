/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      michaelp   09-25-97 1:56pm
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  nsTransform2D(nsTransform2D *aTransform2D)  {
    m00 = aTransform2D->m00;
    m11 = aTransform2D->m11;
    m20 = aTransform2D->m20;
    m21 = aTransform2D->m21;
  }

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
