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

#ifndef nsTopLevelWindow_h___
#define nsTopLevelWindow_h___

#include "nsITopLevelWindow.h"
#include "nsWindow.h"

#define NS_TOPLEVELWINDOW_CID \
{ /* ad24f1e2-1dd1-11b2-a613-df946decae20 */         \
     0xad24f1e2,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0xa6, 0x13, 0xdf, 0x94, 0x6d, 0xec, 0xae, 0x20} \
}

class nsTopLevelWindow : public nsITopLevelWindow,
                         public nsWindow
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITOPLEVELWINDOW

  nsTopLevelWindow();
  virtual ~nsTopLevelWindow();

private:
  /* member variables */
};

#endif  // nsTopLevelWindow_h___
