/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsStdColorPicker.h"
#include "nsColor.h"
#include "nsCRT.h"

#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"


static char* StandardPalette[] = {
  "#FFFFFF","#FFCCCC","#FFCC99","#FFFF99","#FFFFCC","#99FF99","#99FFFF","#CCFFFF","#CCCCFF","#FFCCFF",
  "#CCCCCC","#FF6666","#FFCC33","#FFFF66","#FFFF99","#66FF99","#33FFFF","#66FFFF","#9999FF","#FF99FF",
  "#CCCCCC","#FF0000","#FF9900","#FFCC66","#FFFF00","#33FF33","#66CCCC","#33CCFF","#6666CC","#CC66CC",
  "#999999","#CC0000","#FF6600","#FFCC33","#FFCC00","#33CC00","#00CCCC","#3366FF","#6633FF","#CC33CC",
  "#666666","#990000","#CC6600","#CC9933","#999900","#009900","#339999","#3333FF","#6600CC","#993399",
  "#333333","#660000","#993300","#996633","#666600","#006600","#336666","#000099","#333399","#663366",
  "#000000","#330000","#663300","#663333","#333300","#003300","#003333","#000066","#330099","#330033"
};
static int nColors = sizeof(StandardPalette) / sizeof(char *);

NS_IMPL_ISUPPORTS1(nsStdColorPicker, nsIColorPicker)

nsStdColorPicker::nsStdColorPicker()
{
  mNumCols = 10; // NSToIntRound(sqrt(nColors));
  mNumRows = NSToIntRound(nColors/mNumCols);

  mFrameWidth = 0;
  mFrameHeight = 0;

  mBlockWidth = 20;
  mBlockHeight = 20;
}

nsStdColorPicker::~nsStdColorPicker()
{

}

NS_IMETHODIMP nsStdColorPicker::Paint(nsIPresContext * aPresContext, nsIRenderingContext * aRenderingContext)
{
  int i = 0;
  int row = 0;
  int col = 0;
  nscolor color = 0;
  float p2t;
  PRInt32 width, height;

  aPresContext->GetScaledPixelsToTwips(&p2t);

  width = NSToIntRound(mBlockWidth * p2t);
  height = NSToIntRound(mBlockHeight * p2t);

  //  aRenderingContext->SetColor(0);
  //  aRenderingContext->FillRect(0, 0, (mNumCols)*width, mNumRows*height);

  for (i=0;i<nColors;i++)
  {
    NS_LooseHexToRGB(StandardPalette[i], &color);

    aRenderingContext->SetColor(color);
    aRenderingContext->FillRect(col*width, row*height, width, height);

    if (col+1 == mNumCols)
    {
      col = 0;
      row++;
    }
    else
      col++;
  }


  return NS_OK;
}

NS_IMETHODIMP nsStdColorPicker::GetColor(PRInt32 aX, PRInt32 aY, char **aColor)
{
  int cur_col = aX / mBlockWidth;
  int cur_row = aY / mBlockHeight;

  int f = mNumCols * cur_row + cur_col;

  if (f >= nColors)
  {
    return NS_ERROR_FAILURE;
  }

  *aColor = nsCRT::strdup(StandardPalette[(int)f]);

  return NS_OK;
}

NS_IMETHODIMP nsStdColorPicker::SetSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mFrameWidth = aWidth;
  mFrameHeight = aHeight;

  if (aWidth != -1)
    mBlockWidth = NSToIntRound(mFrameWidth / mNumCols);

  if (aWidth != -1)
    mBlockHeight = NSToIntRound(mFrameHeight / mNumRows);

  return NS_OK;
}

NS_IMETHODIMP nsStdColorPicker::GetSize(PRInt32 *aWidth, PRInt32 *aHeight)
{
  mFrameWidth = NSToIntRound((mNumCols) * mBlockWidth);
  mFrameHeight = NSToIntRound((mNumRows) * mBlockHeight);

  *aWidth = mFrameWidth;
  *aHeight = mFrameHeight;

  return NS_OK;
}
