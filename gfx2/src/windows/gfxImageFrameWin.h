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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000-2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "gfxIImageFrame.h"
#include "gfxIImageFrameWin.h"

#include "nsPoint.h"
#include "nsSize.h"

#include "nsCOMPtr.h"

#include <windows.h>


#define GFX_IMAGEFRAMEWIN_CID \
{ /* 004299f4-1dd2-11b2-a860-9839e7ab9f93 */         \
     0x004299f4,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0xa8, 0x60, 0x98, 0x39, 0xe7, 0xab, 0x9f, 0x93} \
}


struct WinBitmap
{
public:
  WinBitmap() : mDIB(nsnull), mDIBBits(nsnull), mBMI(nsnull),
                mDDB(nsnull) { }

  ~WinBitmap() {
    if (mDIB)
      ::DeleteObject(mDIB);
    // mDIBBits will get cleaned up automatically
    if (mBMI)
      delete mBMI;
    if (mDDB)
      ::DeleteObject(mDDB);
  }

  HBITMAP mDIB;
  void *mDIBBits;
  LPBITMAPINFOHEADER mBMI;

  HBITMAP mDDB;
};

class gfxImageFrameWin : public gfxIImageFrame,
                         public gfxIImageFrameWin
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_GFXIIMAGEFRAME
  NS_DECL_GFXIIMAGEFRAMEWIN

  gfxImageFrameWin();
  virtual ~gfxImageFrameWin();

protected:
  nsSize mSize;

private:
  /* private members */
  struct WinBitmap mImage;
  struct WinBitmap mAlphaImage;

  PRUint32 mImageRowSpan;
  PRUint32 mAlphaRowSpan;

  PRPackedBool mInitalized;
  PRPackedBool mMutable;
  PRPackedBool mHasBackgroundColor;
  PRPackedBool mHasTransparentColor;
  gfx_format   mFormat;


  gfx_depth    mAlphaDepth;


  PRInt32 mTimeout; // -1 means display forever
  nsPoint mOffset;

  gfx_color mBackgroundColor;
  gfx_color mTransparentColor;

  PRInt32   mDisposalMethod;
};
