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

#include "nsRect.h"

#include <X11/Xlib.h>

#include <gdk/gdk.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#define NS_IMAGEFRAME_CID \
{ /* 3d25862e-1dd2-11b2-9d51-be9a25210304 */         \
     0x3d25862e,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x9d, 0x51, 0xbe, 0x9a, 0x25, 0x21, 0x03, 0x04} \
}

struct ImageData
{
  ImageData() : bytesPerRow(0), data(nsnull), length(0), depth(0), dataChanged(PR_FALSE),
    pixmap(nsnull), pixbuf(nsnull) {}
  ~ImageData() {
    delete[] data;
    if (pixmap) gdk_pixmap_unref(pixmap);
    if (pixbuf) gdk_pixbuf_unref(pixbuf);
  }
  PRUint32 bytesPerRow; // bytes per row
  PRUint8 *data;
  PRUint32 length; // length of the data in bytes
  gfx_depth depth;
  PRPackedBool dataChanged;

  /* union? */
  GdkPixmap *pixmap;
  GdkPixbuf *pixbuf;
};

class nsImageFrame : public gfxIImageFrame
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
  GdkBitmap *GetAlphaBitmap();


  /* additional members */
  nsRect mRect;

  ImageData mImageData;       // ... (ends with 16 bits)
  gfx_format mFormat;         // 16 bits

  ImageData *mAlphaData;

  PRBool mInitalized;
  PRInt32 mTimeout;
};
