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
{ /* 27d55516-1dd2-11b2-9b33-d9a6328f49bd */         \
     0x27d55516,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x9b, 0x33, 0xd9, 0xa6, 0x32, 0x8f, 0x49, 0xbd} \
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

