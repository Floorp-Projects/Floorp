/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab: */
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
 *   Brian Stell <bstell@netscape.com>
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

#include "gfx-config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include "nsCRT.h"

#include "nsAntiAliasedGlyph.h"

nsAntiAliasedGlyph::nsAntiAliasedGlyph(PRUint32 aMaxWidth, PRUint32 aMaxHeight,
                         PRUint32 aBorder)
{
  mMaxWidth = aMaxWidth;
  mMaxHeight = aMaxHeight;
  mBorder = aBorder;
  mBufferWidth  = mBorder + mMaxWidth  + mBorder;
  mBufferHeight = mBorder + mMaxHeight + mBorder;

  mAscent    = 0;
  mDescent   = 0;
  mLBearing  = 0;
  mRBearing  = 0;
  mWidth     = 0;
  mHeight    = 0;
  mAdvance   = 0;
  mOwnBuffer = PR_FALSE;
  mBuffer    = nsnull;
  mBufferLen = 0;
}

nsAntiAliasedGlyph::~nsAntiAliasedGlyph()
{
  if (mOwnBuffer)
    nsMemory::Free(mBuffer);
}

PRBool
nsAntiAliasedGlyph::Init()
{
  return Init(nsnull, 0);
}

PRBool
nsAntiAliasedGlyph::Init(PRUint8 *aBuffer, PRUint32 aBufferLen)
{
  mBufferLen = mBufferWidth * mBufferHeight;
  if (aBufferLen >= mBufferLen) {
    mBuffer = aBuffer;
    mOwnBuffer = PR_FALSE;
  }
  else {
    mBuffer = (PRUint8 *)nsMemory::Alloc(mBufferLen);
    if (!mBuffer) {
      mBufferLen = 0;
      return PR_FALSE;
    }
    mOwnBuffer = PR_TRUE;
  }
  memset(mBuffer, 0, mBufferLen);
  return PR_TRUE;
}

#ifdef MOZ_ENABLE_FREETYPE2
PRBool
nsAntiAliasedGlyph::WrapFreeType(FT_BBox *aBbox, FT_BitmapGlyph aSlot,
                                 PRUint8 *aBuffer, PRUint32 aBufferLen)
{
  mAscent       = aBbox->yMax;
  mDescent      = aBbox->yMin;
  mLBearing     = aBbox->xMin;
  mRBearing     = aBbox->xMax;
  mAdvance      = aSlot->root.advance.x>>16;
  mWidth        = aSlot->bitmap.width;
  mHeight       = aSlot->bitmap.rows;


  if (aSlot->bitmap.pixel_mode == ft_pixel_mode_grays) {
    mBufferWidth  = aSlot->bitmap.pitch;
    mBufferHeight = aSlot->bitmap.rows;
    mBufferLen = mBufferWidth * mBufferHeight;
    mBuffer = aSlot->bitmap.buffer;
    mOwnBuffer = PR_FALSE;
    return PR_TRUE;
  }
  else {
    // expand the data from 1 bit to 8 bit
    mBufferWidth  = aSlot->bitmap.width;
    mBufferHeight = aSlot->bitmap.rows;
    if (!Init(aBuffer, aBufferLen))
      return PR_FALSE;
    int pitch = aSlot->bitmap.pitch;
    for (int row=0; row<aSlot->bitmap.rows; row++) {
      for (int j=0; j<aSlot->bitmap.width; j++) {
        int byte = aSlot->bitmap.buffer[(j>>3) + (row*pitch)];
        if (!((byte<<(j&0x7)) & 0x80))
          continue;
        mBuffer[j+(row*mBufferWidth)] = 255;
      }
    }
  }
  return PR_TRUE;
}
#endif

PRBool
nsAntiAliasedGlyph::SetImage(XCharStruct *aCharStruct, XImage *aXImage)
{
  NS_ASSERTION(mBuffer, "null buffer (was Init called?)");
  if (!mBuffer)
    return PR_FALSE;
  PRUint32 src_width = GLYPH_RIGHT_EDGE(aCharStruct)
                       - GLYPH_LEFT_EDGE(aCharStruct);
  PRUint32 src_height = aXImage->height;
  if ((src_width > mMaxWidth) || (src_height > mMaxHeight)) {
    NS_ASSERTION(src_width<=mMaxWidth,"unexpected width");
    NS_ASSERTION(src_height<=mMaxHeight,"unexpected height");
    return PR_FALSE;
  }

  mAscent   = aCharStruct->ascent;
  mDescent  = aCharStruct->descent;
  mLBearing = aCharStruct->lbearing;
  mRBearing = aCharStruct->rbearing;
  mWidth    = src_width;
  mHeight   = src_height;
  mAdvance  = aCharStruct->width;

  NS_ASSERTION(aXImage->format==ZPixmap,"unexpected image format");
  if (aXImage->format != ZPixmap)
    return PR_FALSE;

  int bits_per_pixel = aXImage->bits_per_pixel;
  memset((char*)mBuffer, 0, mBufferLen);

  PRUint32 x, y;
  PRUint32 src_index = 0;
  PRUint32 dst_index =  mBorder + (mBorder*mBufferWidth);
  PRInt32 delta_dst_row = -src_width + mBufferWidth;
  PRUint8 *pSrcLineStart = (PRUint8 *)aXImage->data;
  if (bits_per_pixel == 16) {
    for (y=0; y<src_height; y++) {
      PRUint16 *src = (PRUint16*)pSrcLineStart;
      for (x=0; x<src_width; x++,src++,dst_index++) {
        if (*src & 0x1)
          mBuffer[dst_index] = 0xFF;
      }
      // move to the next row
      dst_index += delta_dst_row;
      pSrcLineStart += aXImage->bytes_per_line;
    }
    return PR_TRUE;
  }
  else if (bits_per_pixel == 24) {
    PRUint8 *src = (PRUint8*)aXImage->data;
    for (y=0; y<src_height; y++) {
      for (x=0; x<src_width; x++,src_index+=3,dst_index++) {
        if (src[src_index] & 0x1)
          mBuffer[dst_index] = 0xFF;
      }
      // move to the next row
      dst_index += delta_dst_row;
      src_index += -3*src_width + aXImage->bytes_per_line;
    }
    return PR_TRUE;
  }
  else if (bits_per_pixel == 32) {
    for (y=0; y<src_height; y++) {
      PRUint32 *src = (PRUint32*)pSrcLineStart;
      for (x=0; x<src_width; x++,src++,dst_index++) {
        if (*src & 0x100)
          mBuffer[dst_index] = 0xFF;
      }
      // move to the next row
      dst_index += delta_dst_row;
      pSrcLineStart += aXImage->bytes_per_line;
    }
    return PR_TRUE;
  }
  else {
    NS_ASSERTION(0, "do not support current bits_per_pixel");
    return PR_FALSE;
  }
}

PRBool
nsAntiAliasedGlyph::SetSize(GlyphMetrics *aGlyphMetrics)
{
  mAscent   = aGlyphMetrics->ascent;
  mDescent  = aGlyphMetrics->descent;
  mLBearing = aGlyphMetrics->lbearing;
  mRBearing = aGlyphMetrics->rbearing;
  mWidth    = aGlyphMetrics->width;
  mHeight   = aGlyphMetrics->height;
  mAdvance  = aGlyphMetrics->advance;
  return PR_TRUE;
}
