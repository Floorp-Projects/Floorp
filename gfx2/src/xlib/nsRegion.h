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
 * The Initial Developer of the Original Code is Stuart Parmenter.
 * Portions created by Stuart Parmenter are Copyright (C) 1998-2000
 * Stuart Parmenter.  All Rights Reserved.  
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef nsRegion_h___
#define nsRegion_h___

#include "nsIRegion.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define NS_REGION_CID \
{ /* 8c8d6e7e-1dd2-11b2-b6ba-efe851e28e39 */         \
     0x8c8d6e7e,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0xb6, 0xba, 0xef, 0xe8, 0x51, 0xe2, 0x8e, 0x39} \
}

class nsRegion : public nsIRegion
{
public:
  nsRegion();
  virtual ~nsRegion();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREGION

  friend class nsDrawable;
  friend class nsWindow;

protected:
  Region mRegion;
  static Region copyRegion;

private:
  void XCopyRegion(Region src, Region dr_return);
  void XUnionRegionWithRect(Region srca,
                            gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight,
                            Region dr_return);

  Region CreateRectRegion(gfx_coord aX, gfx_coord aY, gfx_dimension aWidth, gfx_dimension aHeight);
  inline Region GetCopyRegion();

};

#endif  // nsRegion_h___ 
