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

#define NS_PIXMAP_CID \
{ /* 6c090a8a-1dd2-11b2-9c3a-e52a0dc93584 */         \
     0x6c090a8a,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x9c, 0x3a, 0xe5, 0x2a, 0x0d, 0xc9, 0x35, 0x84} \
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
  Pixmap mPixmap;
};

#endif  // nsPixmap_h___
