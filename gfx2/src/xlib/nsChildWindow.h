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

#ifndef nsChildWindow_h___
#define nsChildWindow_h___

#include "nsIChildWindow.h"
#include "nsWindow.h"

#define NS_CHILDWINDOW_CID \
{ /* a61effcc-1dd1-11b2-8167-c8e35841fd2f */         \
     0xa61effcc,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0x81, 0x67, 0xc8, 0xe3, 0x58, 0x41, 0xfd, 0x2f} \
}

class nsChildWindow : public nsIChildWindow,
                      public nsWindow
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICHILDWINDOW

  nsChildWindow();
  virtual ~nsChildWindow();

private:
  /* member variables */
};

#endif  // nsChildWindow_h___
