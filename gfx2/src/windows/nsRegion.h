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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):
 */

#ifndef nsRegion_h___
#define nsRegion_h___

#include "nsIRegion.h"

#include "windows.h"

#define NS_REGION_CID \
{ /* 9cb3d640-1dd1-11b2-9518-d76af15f2e31 */         \
     0x9cb3d640,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0x95, 0x18, 0xd7, 0x6a, 0xf1, 0x5f, 0x2e, 0x31} \
}

class nsRegion : public nsIRegion
{
public:
  nsRegion();
  virtual ~nsRegion();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREGION

protected:
  LPRGNDATA mData;
  PRUint32  mDataSize;

  HRGN      mRegion;
  int       mRegionType;
};

#endif  // nsRegion_h___ 
