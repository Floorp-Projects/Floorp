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
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef nsPixmap_h___
#define nsPixmap_h___

#include "nsIPixmap.h"
#include "nsDrawable.h"

#ifndef __QDOFFSCREEN__
#include <QDOffscreen.h>
#endif

#define NS_PIXMAP_CID \
{ /* 51d24afe-1dd2-11b2-bb2f-c50415059b04 */         \
     0x51d24afe,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0xbb, 0x2f, 0xc5, 0x04, 0x15, 0x05, 0x9b, 0x04} \
}

class nsPixmap : public nsIPixmap,
                 public nsDrawable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPIXMAP

  nsPixmap();
  virtual ~nsPixmap();

private:
  
  GWorldPtr mGWorld;
};

#endif  // nsPixmap_h___
