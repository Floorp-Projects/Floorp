/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsTransform2D_h___
#define nsTransform2D_h___

#include "nscore.h"
#include "nsCoord.h"
#include "nsUnitConversion.h"

#define MG_2DIDENTITY     0
#define MG_2DTRANSLATION  1
#define MG_2DSCALE        2
#define MG_2DGENERAL      4

class NS_GFX nsTransform2D
{
private:
  //accelerators

  float     m00, m01, m10, m11, m20, m21;
  PRUint16  type;

public:
  //constructors

  nsTransform2D(void)                         { SetToIdentity(); }
  nsTransform2D(nsTransform2D *aTransform2D)  { SetMatrix(aTransform2D); }

  //destructor

  ~nsTransform2D(void)                        { }

 /**
  * get the type of this transform
  *
  * @param
  * @return     type from above set
  * @exception
  * @author     michaelp   09-25-97 1:56pm
  **/

  PRUint16 GetType(void)                      { return type; }

 /**
  * set this transform to identity
  *
  * @param
  * @exception
  * @author     michaelp   09-25-97 1:56pm
  **/

  void SetToIdentity(void)                    { m01 = m10 = m20 = m21 = 0.0f; m00 = m11 = 1.0f; type = MG_2DIDENTITY; }

 /**
  * set this transform to a scale
  *
  * @param      sx, x scale
  * @param      sy, y scale
  * @exception
  * @author     michaelp   09-25-97 1:56pm
  **/

  void SetToScale(float sx, float sy);

 /**
  * set this transform to a translation
  *
  * @param      tx, x translation
  * @param      ty, y translation
  * @exception
  * @author     michaelp   09-25-97 1:56pm
  **/

  void SetToTranslate(float tx, float ty);

 /**
  * get the translation portion of this transform
  *
  * @param      pt, Point to return translation values in
  * @exception
  * @author     michaelp   09-25-97 1:56pm
  **/

  void GetTranslation(float *ptX, float *ptY) { *ptX = m20; *ptY = m21; }
  void GetTranslationCoord(nscoord *ptX, nscoord *ptY) { *ptX = NSToCoordRound(m20); *ptY = NSToCoordRound(m21); }

 /**
  * get the X translation portion of this transform
  *
  * @param
  * @returns x component of translation
  * @exception
  **/

  float GetXTranslation(void)                 { return m20; }
  nscoord GetXTranslationCoord(void)          { return NSToCoordRound(m20); }

 /**
  * get the Y translation portion of this transform
  *
  * @param
  * @returns y component of translation
  * @exception
  **/

  float GetYTranslation(void)               { return m21; }
  nscoord GetYTranslationCoord(void)        { return NSToCoordRound(m21); }

 /**
  * set this matrix and type from another Transform2D
  *
  * @param    aTransform2D is the Transform2D to be copied from
  * @exception
  * @author   michaelp   09-25-97 1:56pm
  **/

  void SetMatrix(nsTransform2D *aTransform2D);

 /**
  * post-multiply a new Transform
  *
  * @param    newxform new Transform2D
  * @exception
  * @author   michaelp   09-25-97 1:56pm
  **/

  void Concatenate(nsTransform2D *newxform);

 /**
  * pre-multiply a new Transform
  *
  * @param    newxform new Transform2D
  * @exception
  * @author   michaelp   09-25-97 1:56pm
  **/

  void PreConcatenate(nsTransform2D *newxform);

 /**
  * apply nontranslation portion of matrix to vector
  *
  * @param    pt  Point to transform
  * @exception
  * @author   michaelp   09-25-97 1:56pm
  **/

  void TransformNoXLate(float *ptX, float *ptY);
  void TransformNoXLateCoord(nscoord *ptX, nscoord *ptY);

 /**
  * apply matrix to vector
  *
  * @param    pt Point to transform
  * @exception
  * @author   michaelp   09-25-97 1:56pm
  **/

  void Transform(float *ptX, float *ptY);
  void TransformCoord(nscoord *ptX, nscoord *ptY);

 /**
  * apply matrix to rect
  *
  * @param    rect Rect to transform
  * @exception
  * @author   michaelp   09-25-97 1:56pm
  **/

  void Transform(float *aX, float *aY, float *aWidth, float *aHeight);
  void TransformCoord(nscoord *aX, nscoord *aY, nscoord *aWidth, nscoord *aHeight);

  /**
   * Scale an array of X/Y coordinates by the X/Y scale factor in the
   * matrix. The scale is done as if the other coordinate were zero.
   *
   * @param aSrc Base of coordinate input array
   * @param aDst Base of coordinate output array
   * @param aNumCoords Number of coordinates to scale
   */
  void ScaleXCoords(const nscoord* aSrc, PRUint32 aNumCoords, PRIntn* aDst);
  void ScaleYCoords(const nscoord* aSrc, PRUint32 aNumCoords, PRIntn* aDst);

 /**
  * add a translation to a Transform via x, y pair
  *
  * @param    ptX x value to add as x translation
  * @param    ptY y value to add as y translation
  * @exception
  * @author   michaelp   09-25-97 1:56pm
  **/

  void AddTranslation(float ptX, float ptY);

 /**
  * add a scale to a Transform via x, y pair
  *
  * @param    ptX x value to add as x scale
  * @param    ptY y value to add as y scale
  * @exception
  * @author   michaelp   09-25-97 1:56pm
  **/

  void AddScale(float ptX, float ptY);
};

#endif
