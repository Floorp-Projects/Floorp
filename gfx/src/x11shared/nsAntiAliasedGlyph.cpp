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
