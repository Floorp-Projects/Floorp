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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsIImageContainer.h"

#include "nsSize2.h"

#include "nsSupportsArray.h"

#define NS_IMAGECONTAINER_CID \
{ /* 284f7652-1dd2-11b2-b0b4-d40aab841150 */         \
     0x284f7652,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0xb0, 0xb4, 0xd4, 0x0a, 0xab, 0x84, 0x11, 0x50} \
}

class nsImageContainer : public nsIImageContainer
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMAGECONTAINER

  nsImageContainer();
  virtual ~nsImageContainer();

private:
  /* additional members */
  nsSupportsArray mFrames;
  nsSize2 mSize;
  PRUint32 mCurrentFrame;
};

