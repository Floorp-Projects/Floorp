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

#include "nsXULAtoms.h"
#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"

static char *NosePalette[] = {
  "#00FF00","#00ED00","#00DB00","#00C900","#00B700","#00A500","#009300","#008100","#006F00","#005D00",
  "#FFFF00","#EDED00","#DBDB00","#C9C900","#B7B700","#A5A500","#939300","#818100","#6F6F00","#5D5D00",
  "#FFFF00","#F5ED00","#EBDB00","#E1C900","#D7B700","#CDA500","#C39300","#B98100","#AF6F00","#A55D00"
};

static char* StandardPalette[] = {
  "#FFFFFF","#FFCCCC","#FFCC99","#FFFF99","#FFFFCC","#99FF99","#99FFFF","#CCFFFF","#CCCCFF","#FFCCFF",
  "#CCCCCC","#FF6666","#FFCC33","#FFFF66","#FFFF99","#66FF99","#33FFFF","#66FFFF","#9999FF","#FF99FF",
  "#C0C0C0","#FF0000","#FF9900","#FFCC66","#FFFF00","#33FF33","#66CCCC","#33CCFF","#6666CC","#CC66CC",
  "#999999","#CC0000","#FF6600","#FFCC33","#FFCC00","#33CC00","#00CCCC","#3366FF","#6633FF","#CC33CC",
  "#666666","#990000","#CC6600","#CC9933","#999900","#009900","#339999","#3333FF","#6600CC","#993399",
  "#333333","#660000","#993300","#996633","#666600","#006600","#336666","#000099","#333399","#663366",
  "#000000","#330000","#663300","#663333","#333300","#003300","#003333","#000066","#330099","#330033"
};

static char *WebPalette[] = {
  "#FFFFFF", "#FFFFCC", "#FFFF99", "#FFFF66", "#FFFF33", "#FFFF00", "#FFCCFF", "#FFCCCC",
  "#FFCC99", "#FFCC66", "#FFCC33", "#FFCC00", "#FF99FF", "#FF99CC", "#FF9999", "#FF9966",
  "#FF9933", "#FF9900", "#FF66FF", "#FF66CC", "#FF6699", "#FF6666", "#FF6633", "#FF6600",
  "#FF33FF", "#FF33CC", "#FF3399", "#FF3366", "#FF3333", "#FF3300", "#FF00FF", "#FF00CC",
  "#FF0099", "#FF0066", "#FF0033", "#FF0000", "#CCFFFF", "#CCFFCC", "#CCFF99", "#CCFF66",
  "#CCFF33", "#CCFF00", "#CCCCFF", "#CCCCCC", "#CCCC99", "#CCCC66", "#CCCC33", "#CCCC00",
  "#CC99FF", "#CC99CC", "#CC9999", "#CC9966", "#CC9933", "#CC9900", "#CC66FF", "#CC66CC",
  "#CC6699", "#CC6666", "#CC6633", "#CC6600", "#CC33FF", "#CC33CC", "#CC3399", "#CC3366",
  "#CC3333", "#CC3300", "#CC00FF", "#CC00CC", "#CC0099", "#CC0066", "#CC0033", "#CC0000",
  "#99FFFF", "#99FFCC", "#99FF99", "#99FF66", "#99FF33", "#99FF00", "#99CCFF", "#99CCCC",
  "#99CC99", "#99CC66", "#99CC33", "#99CC00", "#9999FF", "#9999CC", "#999999", "#999966",
  "#999933", "#999900", "#9966FF", "#9966CC", "#996699", "#996666", "#996633", "#996600",
  "#9933FF", "#9933CC", "#993399", "#993366", "#993333", "#993300", "#9900FF", "#9900CC",
  "#990099", "#990066", "#990033", "#990000", "#66FFFF", "#66FFCC", "#66FF99", "#66FF66",
  "#66FF33", "#66FF00", "#66CCFF", "#66CCCC", "#66CC99", "#66CC66", "#66CC33", "#66CC00",
  "#6699FF", "#6699CC", "#669999", "#669966", "#669933", "#669900", "#6666FF", "#6666CC",
  "#666699", "#666666", "#666633", "#666600", "#6633FF", "#6633CC", "#663399", "#663366",
  "#663333", "#663300", "#6600FF", "#6600CC", "#660099", "#660066", "#660033", "#660000",
  "#33FFFF", "#33FFCC", "#33FF99", "#33FF66", "#33FF33", "#33FF00", "#33CCFF", "#33CCCC",
  "#33CC99", "#33CC66", "#33CC33", "#33CC00", "#3399FF", "#3399CC", "#339999", "#339966",
  "#339933", "#339900", "#3366FF", "#3366CC", "#336699", "#336666", "#336633", "#336600",
  "#3333FF", "#3333CC", "#333399", "#333366", "#333333", "#333300", "#3300FF", "#3300CC",
  "#330099", "#330066", "#330033", "#330000", "#00FFFF", "#00FFCC", "#00FF99", "#00FF66",
  "#00FF33", "#00FF00", "#00CCFF", "#00CCCC", "#00CC99", "#00CC66", "#00CC33", "#00CC00",
  "#0099FF", "#0099CC", "#009999", "#009966", "#009933", "#009900", "#0066FF", "#0066CC",
  "#006699", "#006666", "#006633", "#006600", "#0033FF", "#0033CC", "#003399", "#003366",
  "#003333", "#003300", "#0000FF", "#0000CC", "#000099", "#000066", "#000033", "#000000"
};

static char *GrayPalette[] = {
  "#000000", "#010101", "#020202", "#030303", "#040404", "#050505", "#060606", "#070707", 
  "#080808", "#090909", "#0A0A0A", "#0B0B0B", "#0C0C0C", "#0D0D0D", "#0E0E0E", "#0F0F0F", 
  "#101010", "#111111", "#121212", "#131313", "#141414", "#151515", "#161616", "#171717", 
  "#181818", "#191919", "#1A1A1A", "#1B1B1B", "#1C1C1C", "#1D1D1D", "#1E1E1E", "#1F1F1F", 
  "#202020", "#212121", "#222222", "#232323", "#242424", "#252525", "#262626", "#272727", 
  "#282828", "#292929", "#2A2A2A", "#2B2B2B", "#2C2C2C", "#2D2D2D", "#2E2E2E", "#2F2F2F", 
  "#303030", "#313131", "#323232", "#333333", "#343434", "#353535", "#363636", "#373737", 
  "#383838", "#393939", "#3A3A3A", "#3B3B3B", "#3C3C3C", "#3D3D3D", "#3E3E3E", "#3F3F3F", 
  "#404040", "#414141", "#424242", "#434343", "#444444", "#454545", "#464646", "#474747", 
  "#484848", "#494949", "#4A4A4A", "#4B4B4B", "#4C4C4C", "#4D4D4D", "#4E3E3E", "#4F4F4F", 
  "#505050", "#515151", "#525252", "#534343", "#545454", "#555555", "#565656", "#575757", 
  "#585858", "#595959", "#5A5A5A", "#5B5B5B", "#5C5C5C", "#5D5D5D", "#5E5E5E", "#5F5F5F", 
  "#606060", "#616161", "#626262", "#636363", "#646464", "#656565", "#666666", "#676767", 
  "#686868", "#696969", "#6A6A6A", "#6B6B6B", "#6C6C6C", "#6D6D6D", "#6E6E6E", "#6F6F6F", 
  "#707070", "#717171", "#727272", "#737373", "#747474", "#757575", "#767676", "#777777", 
  "#787878", "#797979", "#7A7A7A", "#7B7B7B", "#7C7C7C", "#7D7D7D", "#7E7E7E", "#7F7F7F", 
  "#808080", "#818181", "#828282", "#838383", "#848484", "#858585", "#868686", "#878787", 
  "#888888", "#898989", "#8A8A8A", "#8B8B8B", "#8C8C8C", "#8D8D8D", "#8E8E8E", "#8F8F8F", 
  "#909090", "#919191", "#929292", "#939393", "#949494", "#959595", "#969696", "#979797", 
  "#989898", "#999999", "#9A9A9A", "#9B9B9B", "#9C9C9C", "#9D9D9D", "#9E9E9E", "#9F9F9F", 
  "#A0A0A0", "#A1A1A1", "#A2A2A2", "#A3A3A3", "#A4A4A4", "#A5A5A5", "#A6A6A6", "#A7A7A7", 
  "#A8A8A8", "#A9A9A9", "#AAAAAA", "#ABABAB", "#ACACAC", "#ADADAD", "#AEAEAE", "#AFAFAF", 
  "#B0B0B0", "#B1B1B1", "#B2B2B2", "#B3B3B3", "#B4B4B4", "#B5B5B5", "#B6B6B6", "#B7B7B7", 
  "#B8B8B8", "#B9B9B9", "#BABABA", "#BBBBBB", "#BCBCBC", "#BDBDBD", "#BEBEBE", "#BFBFBF", 
  "#C0C0C0", "#C1C1C1", "#C2C2C2", "#C3C3C3", "#C4C4C4", "#C5C5C5", "#C6C6C6", "#C7C7C7", 
  "#C8C8C8", "#C9C9C9", "#CACACA", "#CBCBCB", "#CCCCCC", "#CDCDCD", "#CECECE", "#CFCFCF", 
  "#D0D0D0", "#D1D1D1", "#D2D2D2", "#D3D3D3", "#D4D4D4", "#D5D5D5", "#D6D6D6", "#D7D7D7", 
  "#D8D8D8", "#D9D9D9", "#DADADA", "#DBDBDB", "#DCDCDC", "#DDDDDD", "#DEDEDE", "#DFDFDF", 
  "#E0E0E0", "#E1E1E1", "#E2E2E2", "#E3E3E3", "#E4E4E4", "#E5E5E5", "#E6E6E6", "#E7E7E7", 
  "#E8E8E8", "#E9E9E9", "#EAEAEA", "#EBEBEB", "#ECECEC", "#EDEDED", "#EEEEEE", "#EFEFEF", 
  "#F0F0F0", "#F1F1F1", "#F2F2F2", "#F3F3F3", "#F4F4F4", "#F5F5F5", "#F6F6F6", "#F7F7F7", 
  "#F8F8F8", "#F9F9F9", "#FAFAFA", "#FBFBFB", "#FCFCFC", "#FDFDFD", "#FEFEFE", "#FFFFFF"
};

NS_IMPL_ISUPPORTS1(nsStdColorPicker, nsIColorPicker)

nsStdColorPicker::nsStdColorPicker()
{
  mColors = 0;

  mNumCols = 0;
  mNumRows = 0;

  mFrameWidth = 0;
  mFrameHeight = 0;

  mBlockWidth = 20;
  mBlockHeight = 20;
}

nsStdColorPicker::~nsStdColorPicker()
{

}

NS_IMETHODIMP nsStdColorPicker::Init(nsIContent *aContent)
{
  nsAutoString palette;
  aContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::palettename, palette);

  if (palette.EqualsIgnoreCase("web"))
  {
    printf("web picked\n");
    mPalette = WebPalette;
    mNumCols = 12;
    mColors = sizeof(WebPalette) / sizeof(char *);
  }
  else if (palette.EqualsIgnoreCase("nose"))
  {
    printf("nose picked\n");
    mPalette = NosePalette;
    mNumCols = 10;
    mColors = sizeof(NosePalette) / sizeof(char *);
  }
  else if (palette.EqualsIgnoreCase("gray"))
  {
    printf("gray picked\n");
    mPalette = GrayPalette;
    mNumCols = 10;
    mColors = sizeof(GrayPalette) / sizeof(char *);
  }
  else
  {
    printf("standard picked\n");
    mPalette = StandardPalette;
    mNumCols = 10;
    mColors = sizeof(StandardPalette) / sizeof(char *);
  }

  mNumRows = NSToIntCeil(nscoord(mColors/mNumCols));

  return NS_OK;
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

  for (i=0;i<mColors;i++)
  {
    NS_LooseHexToRGB(mPalette[i], &color);

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

  if (f >= mColors)
  {
    return NS_ERROR_FAILURE;
  }

  *aColor = nsCRT::strdup(mPalette[f]);

  return NS_OK;
}

NS_IMETHODIMP nsStdColorPicker::SetSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mFrameWidth = aWidth;
  mFrameHeight = aHeight;

  if (aWidth != -1)
    mBlockWidth = NSToIntRound(nscoord(aWidth / mNumCols));

  if (aWidth != -1)
    mBlockHeight = NSToIntRound(nscoord(aHeight / mNumRows));

  mFrameWidth = NSToIntRound(nscoord((mNumCols) * mBlockWidth));
  mFrameHeight = NSToIntRound(nscoord((mNumRows) * mBlockHeight));

  return NS_OK;
}

NS_IMETHODIMP nsStdColorPicker::GetSize(PRInt32 *aWidth, PRInt32 *aHeight)
{
  *aWidth = mFrameWidth;
  *aHeight = mFrameHeight;

  return NS_OK;
}
