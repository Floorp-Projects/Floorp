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

#include "nsIImageFrame.h"

#include "nsRect.h"

#include <X11/Xlib.h>

#include <gdk/gdk.h>

#define NS_IMAGEFRAME_CID \
{ /* 27d55516-1dd2-11b2-9b33-d9a6328f49bd */         \
     0x27d55516,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x9b, 0x33, 0xd9, 0xa6, 0x32, 0x8f, 0x49, 0xbd} \
}

struct ImageData
{
  ImageData() : bytesPerRow(0), data(nsnull), length(0), depth(0) {}
  ~ImageData() {
    delete[] data;
  }
  PRUint32 bytesPerRow; // bytes per row
  PRUint8 *data;
  PRUint32 length; // length of the data in bytes
  gfx_depth depth;
};

class nsImageFrame : public nsIImageFrame
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_GFXIIMAGEFRAME

  nsImageFrame();
  virtual ~nsImageFrame();

  //  nsresult DrawImage(Display * display, Drawable dest, const GC gc, const nsRect * aSrcRect, const nsPoint * aDestPoint);
  nsresult DrawImage(GdkDrawable *dest, const GdkGC *gc, const nsRect * aSrcRect, const nsPoint * aDestPoint);

  nsresult DrawScaledImage(GdkDrawable *dest, const GdkGC *gc, const nsRect * aSrcRect, const nsRect * aDestRect);

private:
  /* additional members */
  nsRect mRect;

  ImageData mImageData;       // ... (ends with 16 bits)
  gfx_format mFormat;         // 16 bits

  ImageData *mAlphaData;

  PRBool mInitalized;
};
