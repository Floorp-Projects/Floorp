/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

#include "inBitmap.h"

#include "nsCOMPtr.h"
#include "nsString.h"

///////////////////////////////////////////////////////////////////////////////

inBitmap::inBitmap()
{
  NS_INIT_REFCNT();
}

inBitmap::~inBitmap()
{
  delete mBits;
}

NS_IMPL_ISUPPORTS1(inBitmap, inIBitmap);

///////////////////////////////////////////////////////////////////////////////
// inIBitmap

NS_IMETHODIMP
inBitmap::Init(PRUint32 aWidth, PRUint32 aHeight, PRUint8 aBytesPerPixel)
{
  mWidth = aWidth;
  mHeight = aHeight;
  
  if (aBytesPerPixel == 8) {
    mBits = new PRUint8[aWidth*aHeight];
  } else if (aBytesPerPixel == 16) {
    mBits = new PRUint8[aWidth*aHeight*2];
  } else if (aBytesPerPixel == 32 || aBytesPerPixel == 24) {
    mBits = new PRUint8[aWidth*aHeight*3];
  }

  return NS_OK;
}

NS_IMETHODIMP
inBitmap::GetWidth(PRUint32* aWidth)
{
  *aWidth = mWidth;
  return NS_OK;
}

NS_IMETHODIMP 
inBitmap::GetHeight(PRUint32* aHeight)
{
  *aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP
inBitmap::GetBits(PRUint8** aBits)
{
  *aBits = mBits;
  return NS_OK;
}

NS_IMETHODIMP
inBitmap::GetPixelHex(PRUint32 aX, PRUint32 aY, PRUnichar **_retval)
{
  if (aX < 0 || aX > mWidth || aY < 0 || aY > mHeight)
    return NS_ERROR_FAILURE;
    
  PRUint8* c = mBits + ((aX+(mWidth*aY))*3);
  PRUint8 b = c[0];
  PRUint8 g = c[1];
  PRUint8 r = c[2];

  char* s = new char[7];
  sprintf(s, "#%2X%2X%2X", r, g, b);
  // sprintf won't 0-pad my hex values, so I have to space-pad it
  // and then replace space characters with zero characters
  for (PRUint8 i = 0; i < 6; ++i)
    if (s[i] == 32)
      s[i] = 48;
      
  nsAutoString str;
  str.AssignWithConversion(s);
  delete s;
  
  *_retval = str.ToNewUnicode();

  return NS_OK;
}

NS_IMETHODIMP
inBitmap::PutPixel(PRUint32 aX, PRUint32 aY, PRUint8 aR, PRUint8 aG, PRUint8 aB, PRUnichar **_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inBitmap::PutPixelHex(PRUint32 aX, PRUint32 aY, const PRUnichar *aColor, PRUnichar **_retval)
{
  return NS_OK;
}

