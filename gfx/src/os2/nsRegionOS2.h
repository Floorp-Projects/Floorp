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
 * The Initial Developer of the Original Code is Dainis Jonitis,
 * <Dainis_Jonitis@swh-t.lv>.  Portions created by Dainis Jonitis are
 * Copyright (C) 2001 Dainis Jonitis. All Rights Reserved.
 * 
 * Contributor(s): 
 */


#ifndef _nsRegionOS2_h
#define _nsRegionOS2_h

#include "nsRegionImpl.h"


class nsRegionOS2 : public nsRegionImpl
{
  PRUint32 NumOfRects (HPS aPS, HRGN aRegion) const;

public:
  nsRegionOS2 ();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetNativeRegion(void *&aRegion) const;    // Don't use this on OS/2 - Use GetHRGN () instead
 
  // OS/2 specific
  // get region in widget's coord space for given device
  HRGN GetHRGN (PRUint32 ulHeight, HPS hps);

  // copy from another region defined in aWidget's space for a given device
  nsresult InitWithHRGN (HRGN copy, PRUint32 ulHeight, HPS hps);
};

#endif
