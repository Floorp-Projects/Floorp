/*
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com>
 */

#ifndef __inBitmapDecoder_h__
#define __inBitmapDecoder_h__

#include "imgIDecoder.h"

#include "nsCOMPtr.h"

#include "imgIContainer.h"
#include "imgIDecoderObserver.h"
#include "gfxIImageFrame.h"
#include "imgILoad.h"

class inBitmapDecoder : public imgIDecoder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODER
  NS_DECL_NSIOUTPUTSTREAM

  inBitmapDecoder();
  virtual ~inBitmapDecoder();

private:
  nsCOMPtr<imgIContainer> mImage;
  nsCOMPtr<gfxIImageFrame> mFrame;
  nsCOMPtr<imgIDecoderObserver> mObserver; // this is just qi'd from mRequest for speed
};

#endif // __inBitmapDecoder_h__
