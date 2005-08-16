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


#include "nsTransform2D.h"

void nsTransform2D :: SetMatrix(nsTransform2D *aTransform2D)
{
  m00 = aTransform2D->m00;
  m11 = aTransform2D->m11;
  m20 = aTransform2D->m20;
  m21 = aTransform2D->m21;
  type = aTransform2D->type;
}

void nsTransform2D :: Concatenate(nsTransform2D *newxform)
{
  PRUint16  newtype = newxform->type;

  if (newtype == MG_2DIDENTITY)
  {
    return;
  }
  else if (type == MG_2DIDENTITY)
  {
    SetMatrix(newxform);
    return;
  }
  else if ((type & MG_2DSCALE) != 0)
  {
    //current matrix is at least scale

    if ((newtype & MG_2DSCALE) != 0)
    {
      //new matrix is scale

      if ((newtype & MG_2DTRANSLATION) != 0)
      {
        m20 += newxform->m20 * m00;
        m21 += newxform->m21 * m11;
      }

      m00 *= newxform->m00;
      m11 *= newxform->m11;
    }
    else
    {
      //new matrix must be translation only

      m20 += newxform->m20 * m00;
      m21 += newxform->m21 * m11;
    }
  }
  else
  {
    //current matrix is translation only

    if ((newtype & MG_2DSCALE) != 0)
    {
      //new matrix is scale

      if ((newtype & MG_2DTRANSLATION) != 0)
      {
        m20 += newxform->m20;
        m21 += newxform->m21;
      }

      m00 = newxform->m00;
      m11 = newxform->m11;
    }
    else
    {
        //new matrix must be translation only

        m20 += newxform->m20;
        m21 += newxform->m21;
    }
  }

  type |= newtype;
}

void nsTransform2D :: PreConcatenate(nsTransform2D *newxform)
{
  float new00 = newxform->m00;
  float new11 = newxform->m11;

  m00 *= new00;
  m11 *= new11;
  m20 = m20 * new00 + newxform->m20;
  m21 = m21 * new11 + newxform->m21;

  type |= newxform->type;
}

void nsTransform2D :: TransformNoXLate(float *ptX, float *ptY) const
{
  if ((type & MG_2DSCALE) != 0) {
    *ptX *= m00;
    *ptY *= m11;
  }
}

void nsTransform2D :: TransformNoXLateCoord(nscoord *ptX, nscoord *ptY) const
{
  if ((type & MG_2DSCALE) != 0) {
    *ptX = NSToCoordRound(*ptX * m00);
    *ptY = NSToCoordRound(*ptY * m11);
  }
}

inline PRIntn NSToIntNFloor(float aValue)
{
  return ((0.0f <= aValue) ? PRIntn(aValue) : PRIntn(aValue - CEIL_CONST_FLOAT));
}

void nsTransform2D :: ScaleXCoords(const nscoord* aSrc,
                                   PRUint32 aNumCoords,
                                   PRIntn* aDst) const
{
const nscoord* end = aSrc + aNumCoords;

  if (type == MG_2DIDENTITY){
    while (aSrc < end ) {
      *aDst++ = PRIntn(*aSrc++);
    }
  } else {
    float scale = m00;
    while (aSrc < end) {
      nscoord c = *aSrc++;
      *aDst++ = NSToIntNFloor(c * scale);
    }
  }
}

void nsTransform2D :: ScaleYCoords(const nscoord* aSrc,
                                   PRUint32 aNumCoords,
                                   PRIntn* aDst) const
{
const nscoord* end = aSrc + aNumCoords;

  if (type == MG_2DIDENTITY){
    while (aSrc < end ) {
      *aDst++ = PRIntn(*aSrc++); 
    } 
  } else {
    float scale = m11;
    while (aSrc < end) {
      nscoord c = *aSrc++;
      *aDst++ = NSToIntNFloor(c * scale);
    }
  }
}
  

void nsTransform2D :: Transform(float *ptX, float *ptY) const
{
  switch (type)
  {
    case MG_2DIDENTITY:
      break;

    case MG_2DTRANSLATION:
      *ptX += m20;
      *ptY += m21;
      break;

    case MG_2DSCALE:
      *ptX *= m00;
      *ptY *= m11;
      break;

    case MG_2DSCALE | MG_2DTRANSLATION:
      *ptX = *ptX * m00 + m20;
      *ptY = *ptY * m11 + m21;
      break;

    default:
      NS_ASSERTION(0, "illegal type");
      break;
  }
}

void nsTransform2D :: TransformCoord(nscoord *ptX, nscoord *ptY) const
{
  switch (type)
  {
    case MG_2DIDENTITY:
      break;

    case MG_2DTRANSLATION:
      *ptX += NSToCoordRound(m20);
      *ptY += NSToCoordRound(m21);
      break;

    case MG_2DSCALE:
      *ptX = NSToCoordRound(*ptX * m00);
      *ptY = NSToCoordRound(*ptY * m11);
      break;

    case MG_2DSCALE | MG_2DTRANSLATION:
      *ptX = NSToCoordRound(*ptX * m00 + m20);
      *ptY = NSToCoordRound(*ptY * m11 + m21);
      break;

    default:
      NS_ASSERTION(0, "illegal type");
      break;
  }
}

void nsTransform2D :: Transform(float *aX, float *aY, float *aWidth, float *aHeight) const
{
  switch (type)
  {
    case MG_2DIDENTITY:
      break;

    case MG_2DTRANSLATION:
      *aX += m20;
      *aY += m21;
      break;

    case MG_2DSCALE:
      *aX *= m00;
      *aY *= m11;
      *aWidth *= m00;
      *aHeight *= m11;
      break;

    case MG_2DSCALE | MG_2DTRANSLATION:
      *aX = *aX * m00 + m20;
      *aY = *aY * m11 + m21;
      *aWidth *= m00;
      *aHeight *= m11;
      break;

    default:
      NS_ASSERTION(0, "illegal type");
      break;
  }
}

void nsTransform2D :: TransformCoord(nscoord *aX, nscoord *aY, nscoord *aWidth, nscoord *aHeight) const
{
  nscoord x2 = *aX + *aWidth;
  nscoord y2 = *aY + *aHeight;
  TransformCoord(aX, aY);
  TransformCoord(&x2, &y2);
  *aWidth = x2 - *aX;
  *aHeight = y2 - *aY;
}

void nsTransform2D :: AddTranslation(float ptX, float ptY)
{
  if (type == MG_2DIDENTITY)
  {
    m20 = ptX;
    m21 = ptY;
  }
  else if ((type & MG_2DSCALE) != 0)
  {
    m20 += ptX * m00;
    m21 += ptY * m11;
  }
  else
  {
    m20 += ptX;
    m21 += ptY;
  }

  type |= MG_2DTRANSLATION;
}

void nsTransform2D :: AddScale(float ptX, float ptY)
{
  if ((type == MG_2DIDENTITY) || (type == MG_2DTRANSLATION))
  {
    m00 = ptX;
    m11 = ptY;
  }
  else
  {
    m00 *= ptX;
    m11 *= ptY;
  }

  type |= MG_2DSCALE;
}
