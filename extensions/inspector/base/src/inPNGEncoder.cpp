/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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


#include "inPNGEncoder.h"

#include "nsISupportsArray.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIFrame.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindowInternal.h"
#include "png.h"

///////////////////////////////////////////////////////////////////////////////

inPNGEncoder::inPNGEncoder()
{
  NS_INIT_REFCNT();
}

inPNGEncoder::~inPNGEncoder()
{
}

NS_IMPL_ISUPPORTS1(inPNGEncoder, inIPNGEncoder);

///////////////////////////////////////////////////////////////////////////////

PR_STATIC_CALLBACK(void) gPNGErrorHandler(png_structp aPNGStruct, png_const_charp aMsg);

///////////////////////////////////////////////////////////////////////////////
/// inIPNGEncoder

NS_IMETHODIMP 
inPNGEncoder::WritePNG(inIBitmap *aBitmap, const PRUnichar *aURL, PRInt16 aType)
{
  PRUint8* bits;
  aBitmap->GetBits(&bits);
  PRUint32 width;
  aBitmap->GetWidth(&width);
  PRUint32 height;
  aBitmap->GetHeight(&height);

  png_structp  pngStruct;
  png_infop  infoStruct;

  nsAutoString str;
  str.Assign(aURL);
  FILE *file = fopen(ToNewCString(str), "wb");
  if (file) {
    pngStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, gPNGErrorHandler, NULL);
    infoStruct = png_create_info_struct(pngStruct);
    png_init_io(pngStruct, file);
    png_set_compression_level(pngStruct, Z_BEST_COMPRESSION);

    png_set_IHDR(pngStruct, infoStruct, width, height,
        8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(pngStruct, infoStruct);

    ReverseRGB(width, height, bits);
    PRUint8* rowPtr = bits;
    for (PRUint32 row = 0; row < height; ++row) {
      png_write_row(pngStruct, rowPtr);
      rowPtr += width*3;
    }
    ReverseRGB(width, height, bits);
    
    png_write_end(pngStruct, NULL);

    fclose(file); 
  } else {
    return NS_ERROR_NULL_POINTER;
  }

  return NS_OK;
}

void
inPNGEncoder::ReverseRGB(PRUint32 aWidth, PRUint32 aHeight, PRUint8* aBits)
{
  PRUint8 temp;
  PRUint32 row, col;
  for (row = 0; row < aHeight; ++row) {
    for (col = 0; col < aWidth; ++col) {
      temp = aBits[0];
      aBits[0] = aBits[2];
      aBits[2] = temp;
      aBits += 3;
    }
  }
}

void gPNGErrorHandler(png_structp aPNGStruct, png_const_charp aMsg)
{
  printf("ERROR ENCODING PNG IMAGE\n");
}




