/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "nsTransform2D.h"

void nsTransform2D :: SetToScale(float sx, float sy)
{
  m01 = m10 = m20 = m21 = 0.0f;
  m00 = sx;
  m11 = sy;
  type = MG_2DSCALE;
}

void nsTransform2D :: SetToTranslate(float tx, float ty)
{
  m01 = m10 = 0.0f;
  m00 = m11 = 1.0f;
  m20 = tx;
  m21 = ty;
  type = MG_2DTRANSLATION;
}

void nsTransform2D :: SetMatrix(nsTransform2D *aTransform2D)
{
  m00 = aTransform2D->m00;
  m01 = aTransform2D->m01;
  m10 = aTransform2D->m10;
  m11 = aTransform2D->m11;
  m20 = aTransform2D->m20;
  m21 = aTransform2D->m21;
  type = aTransform2D->type;
}

void nsTransform2D :: Concatenate(nsTransform2D *newxform)
{
  float     temp00, temp01, temp10, temp11;
  float     new00, new01, new10, new11, new20, new21;
  PRUint16  newtype = newxform->type;

  if (type == MG_2DIDENTITY)
  {
    //current matrix is identity

    if (newtype != MG_2DIDENTITY)
      SetMatrix(newxform);

    return;
  }
  else if (newtype == MG_2DIDENTITY)
    return;
  else if ((type & MG_2DSCALE) != 0)
  {
    //current matrix is at least scale

    if ((newtype & (MG_2DGENERAL | MG_2DSCALE)) != 0)
    {
      //new matrix is general or scale

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
  else if ((type & MG_2DGENERAL) != 0)
  {
    //current matrix is at least general

    if ((newtype & MG_2DGENERAL) != 0)
    {
      //new matrix is general - worst case

      temp00 = m00;
      temp01 = m01;
      temp10 = m10;
      temp11 = m11;

      new00 = newxform->m00;
      new01 = newxform->m01;
      new10 = newxform->m10;
      new11 = newxform->m11;

      if ((newtype & MG_2DTRANSLATION) != 0)
      {
        new20 = newxform->m20;
        new21 = newxform->m21;

        m20 += new20 * temp00 + new21 * temp10;
        m21 += new20 * temp01 + new21 * temp11;
      }

      m00 = new00 * temp00 + new01 * temp10;
      m01 = new00 * temp01 + new01 * temp11;
      m10 = new10 * temp00 + new11 * temp10;
      m11 = new10 * temp01 + new11 * temp11;
    }
    else if ((newtype & MG_2DSCALE) != 0)
    {
      //new matrix is at least scale

      temp00 = m00;
      temp01 = m01;
      temp10 = m10;
      temp11 = m11;

      new00 = newxform->m00;
      new11 = newxform->m11;

      if ((newtype & MG_2DTRANSLATION) != 0)
      {
        new20 = newxform->m20;
        new21 = newxform->m21;

        m20 += new20 * temp00 + new21 * temp10;
        m21 += new20 * temp01 + new21 * temp11;
      }

      m00 = new00 * temp00;
      m01 = new00 * temp01;
      m10 = new11 * temp10;
      m11 = new11 * temp11;
    }
    else
    {
        //new matrix must be translation only

        new20 = newxform->m20;
        new21 = newxform->m21;

        m20 += new20 * m00 + new21 * m10;
        m21 += new20 * m01 + new21 * m11;
    }
  }
  else
  {
    //current matrix is translation only

    if ((newtype & (MG_2DGENERAL | MG_2DSCALE)) != 0)
    {
      //new matrix is general or scale

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

/*    temp00 = m00;
  temp01 = m01;
  temp10 = m10;
  temp11 = m11;
  temp20 = m20;
  temp21 = m21;

  new00 = newxform.m00;
  new01 = newxform.m01;
  new10 = newxform.m10;
  new11 = newxform.m11;
  new20 = newxform.m20;
  new21 = newxform.m21;

  m00 = new00 * temp00 + new01 * temp10; // + new02 * temp20 == 0
  m01 = new00 * temp01 + new01 * temp11; // + new02 * temp21 == 0
//    m02 += new00 * temp02 + new01 * temp12; // + new02 * temp22 == 0
  m10 = new10 * temp00 + new11 * temp10; // + new12 * temp20 == 0
  m11 = new10 * temp01 + new11 * temp11; // + new12 * temp21 == 0
//    m12 += new10 * temp02 + new11 * temp12; // + new12 * temp22 == 0
  m20 = new20 * temp00 + new21 * temp10 + temp20; // + new22 * temp20 == temp20
  m21 = new20 * temp01 + new21 * temp11 + temp21; // + new22 * temp21 == temp21
//    m22 += new20 * temp02 + new21 * temp12; // + new22 * temp22 == 1
*/
  type |= newtype;
}

void nsTransform2D :: PreConcatenate(nsTransform2D *newxform)
{
  float temp00, temp01, temp10, temp11, temp20, temp21;
  float new00, new01, new10, new11, new20, new21;

  //this is totally unoptimized MMP

  temp00 = m00;
  temp01 = m01;
  temp10 = m10;
  temp11 = m11;
  temp20 = m20;
  temp21 = m21;

  new00 = newxform->m00;
  new01 = newxform->m01;
  new10 = newxform->m10;
  new11 = newxform->m11;
  new20 = newxform->m20;
  new21 = newxform->m21;

  m00 = temp00 * new00 + temp01 * new10; // + temp02 * new20 == 0
  m01 = temp00 * new01 + temp01 * new11; // + temp02 * new21 == 0
//    m02 += temp00 * new02 + temp01 * new12; // + temp02 * new22 == 0
  m10 = temp10 * new00 + temp11 * new10; // + temp12 * new20 == 0
  m11 = temp10 * new01 + temp11 * new11; // + temp12 * new21 == 0
//    m12 += temp10 * new02 + temp11 * new12; // + temp12 * new22 == 0
  m20 = temp20 * new00 + temp21 * temp10 + temp20; // + temp22 * new20 == new20
  m21 = temp20 * new01 + temp21 * new11 + temp21; // + temp22 * new21 == new21
//    m22 += temp20 * new02 + temp21 * new12; // + temp22 * new22 == 1

  type |= newxform->type;
}

void nsTransform2D :: TransformNoXLate(float *ptX, float *ptY)
{
  float x, y;

  switch (type)
  {
    case MG_2DIDENTITY:
      break;

    case MG_2DSCALE:
      *ptX *= m00;
      *ptY *= m11;
      break;

    default:
    case MG_2DGENERAL:
      x = *ptX;
      y = *ptY;

      *ptX = x * m00 + y * m10;
      *ptY = x * m01 + y * m11;

      break;
  }
}

void nsTransform2D :: TransformNoXLateCoord(nscoord *ptX, nscoord *ptY)
{
  float x, y;

  switch (type)
  {
    case MG_2DIDENTITY:
      break;

    case MG_2DSCALE:
      *ptX = NS_TO_INT_ROUND(*ptX * m00);
      *ptY = NS_TO_INT_ROUND(*ptY * m11);
      break;

    default:
    case MG_2DGENERAL:
      x = (float)*ptX;
      y = (float)*ptY;

      *ptX = NS_TO_INT_ROUND(x * m00 + y * m10);
      *ptY = NS_TO_INT_ROUND(x * m01 + y * m11);

      break;
  }
}

void nsTransform2D :: Transform(float *ptX, float *ptY)
{
  float x, y;

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

    case MG_2DGENERAL:
      x = *ptX;
      y = *ptY;

      *ptX = x * m00 + y * m10;
      *ptY = x * m01 + y * m11;

      break;

    case MG_2DSCALE | MG_2DTRANSLATION:
      *ptX = *ptX * m00 + m20;
      *ptY = *ptY * m11 + m21;
      break;

    default:
    case MG_2DGENERAL | MG_2DTRANSLATION:
      x = *ptX;
      y = *ptY;

      *ptX = x * m00 + y * m10 + m20;
      *ptY = x * m01 + y * m11 + m21;

      break;
  }
}

void nsTransform2D :: TransformCoord(nscoord *ptX, nscoord *ptY)
{
  float x, y;

  switch (type)
  {
    case MG_2DIDENTITY:
      break;

    case MG_2DTRANSLATION:
      *ptX += NS_TO_INT_ROUND(m20);
      *ptY += NS_TO_INT_ROUND(m21);
      break;

    case MG_2DSCALE:
      *ptX = NS_TO_INT_ROUND(*ptX * m00);
      *ptY = NS_TO_INT_ROUND(*ptY * m11);
      break;

    case MG_2DGENERAL:
      x = (float)*ptX;
      y = (float)*ptY;

      *ptX = NS_TO_INT_ROUND(x * m00 + y * m10);
      *ptY = NS_TO_INT_ROUND(x * m01 + y * m11);

      break;

    case MG_2DSCALE | MG_2DTRANSLATION:
      *ptX = NS_TO_INT_ROUND(*ptX * m00 + m20);
      *ptY = NS_TO_INT_ROUND(*ptY * m11 + m21);
      break;

    default:
    case MG_2DGENERAL | MG_2DTRANSLATION:
      x = (float)*ptX;
      y = (float)*ptY;

      *ptX = NS_TO_INT_ROUND(x * m00 + y * m10 + m20);
      *ptY = NS_TO_INT_ROUND(x * m01 + y * m11 + m21);

      break;
  }
}

void nsTransform2D :: Transform(float *aX, float *aY, float *aWidth, float *aHeight)
{
  float x, y;

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

    case MG_2DGENERAL:
      x = *aX;
      y = *aY;

      *aX = x * m00 + y * m10;
      *aY = x * m01 + y * m11;

      x = *aWidth;
      y = *aHeight;

      *aHeight = x * m00 + y * m10;
      *aHeight = x * m01 + y * m11;

      break;

    case MG_2DSCALE | MG_2DTRANSLATION:
      *aX = *aX * m00 + m20;
      *aY = *aY * m11 + m21;
      *aWidth *= m00;
      *aHeight *= m11;
      break;

    default:
    case MG_2DGENERAL | MG_2DTRANSLATION:
      x = *aX;
      y = *aY;

      *aX = x * m00 + y * m10 + m20;
      *aY = x * m01 + y * m11 + m21;

      x = *aWidth;
      y = *aHeight;

      *aWidth = x * m00 + y * m10;
      *aHeight = x * m01 + y * m11;

      break;
  }
}

void nsTransform2D :: TransformCoord(nscoord *aX, nscoord *aY, nscoord *aWidth, nscoord *aHeight)
{
  float x, y;

  switch (type)
  {
    case MG_2DIDENTITY:
      break;

    case MG_2DTRANSLATION:
      *aX += NS_TO_INT_ROUND(m20);
      *aY += NS_TO_INT_ROUND(m21);
      break;

    case MG_2DSCALE:
      *aX = NS_TO_INT_ROUND(*aX * m00);
      *aY = NS_TO_INT_ROUND(*aY * m11);
      *aWidth = NS_TO_INT_ROUND(*aWidth * m00);
      *aHeight = NS_TO_INT_ROUND(*aHeight * m11);
      break;

    case MG_2DGENERAL:
      x = (float)*aX;
      y = (float)*aY;

      *aX = NS_TO_INT_ROUND(x * m00 + y * m10);
      *aY = NS_TO_INT_ROUND(x * m01 + y * m11);

      x = (float)*aWidth;
      y = (float)*aHeight;

      *aHeight = NS_TO_INT_ROUND(x * m00 + y * m10);
      *aHeight = NS_TO_INT_ROUND(x * m01 + y * m11);

      break;

    case MG_2DSCALE | MG_2DTRANSLATION:
      *aX = NS_TO_INT_ROUND(*aX * m00 + m20);
      *aY = NS_TO_INT_ROUND(*aY * m11 + m21);
      *aWidth = NS_TO_INT_ROUND(*aWidth * m00);
      *aHeight = NS_TO_INT_ROUND(*aHeight * m11);
      break;

    default:
    case MG_2DGENERAL | MG_2DTRANSLATION:
      x = (float)*aX;
      y = (float)*aY;

      *aX = NS_TO_INT_ROUND(x * m00 + y * m10 + m20);
      *aY = NS_TO_INT_ROUND(x * m01 + y * m11 + m21);

      x = (float)*aWidth;
      y = (float)*aHeight;

      *aWidth = NS_TO_INT_ROUND(x * m00 + y * m10);
      *aHeight = NS_TO_INT_ROUND(x * m01 + y * m11);

      break;
  }
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
    //current matrix is at least scale

    m20 += ptX * m00;
    m21 += ptY * m11;
  }
  else if ((type & MG_2DGENERAL) != 0)
  {
    //current matrix is at least general

    m20 += ptX * m00 + ptY * m10;
    m21 += ptX * m01 + ptY * m11;
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
  else if ((type & MG_2DSCALE) != 0)
  {
    //current matrix is at least scale

    m00 *= ptX;
    m11 *= ptY;
  }
  else if ((type & MG_2DGENERAL) != 0)
  {
    //current matrix is at least general

    m00 *= ptX;
    m01 *= ptX;
    m10 *= ptY;
    m11 *= ptY;
  }

  type |= MG_2DSCALE;
}
