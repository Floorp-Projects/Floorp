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

#ifndef nsPopupWindow_h___
#define nsPopupWindow_h___

#include "nsIPopupWindow.h"
#include "nsWindow.h"

#define NS_POPUPWINDOW_CID \
{ /* e1f41f14-1dd1-11b2-bac7-e2c3167d09f4 */         \
     0xe1f41f14,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0xba, 0xc7, 0xe2, 0xc3, 0x16, 0x7d, 0x09, 0xf4} \
}

class nsPopupWindow : public nsIPopupWindow,
                      public nsWindow
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPOPUPWINDOW

  nsPopupWindow();
  virtual ~nsPopupWindow();

private:
  /* member variables */
};

#endif  // nsPopupWindow_h___
