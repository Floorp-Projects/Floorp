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

#include "nsRect2.h"

#define NS_IMAGEFRAME_CID \
{ /* 99b219ea-1dd1-11b2-aa87-cd48e7d50227 */         \
     0x99b219ea,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0xaa, 0x87, 0xcd, 0x48, 0xe7, 0xd5, 0x02, 0x27} \
}

class nsImageFrame : public nsIImageFrame
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMAGEFRAME

  nsImageFrame();
  virtual ~nsImageFrame();

private:
  /* additional members */
  PRUint32 mBytesPerRow;
  nsRect2 mRect;
  gfx_format mFormat;

  PRUint32 mBitsLength;

  gfx_depth mDepth;
  PRUint8 *mBits;
};

