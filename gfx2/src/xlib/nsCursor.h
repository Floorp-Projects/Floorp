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

#ifndef nsCursor_h___
#define nsCursor_h___

#include "nsICursor.h"

#include <X11/Xlib.h>

#define NS_CURSOR_CID \
{ /* 86f574ae-1dd2-11b2-a4c5-d5509ba74703 */         \
     0x86f574ae,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0xa4, 0xc5, 0xd5, 0x50, 0x9b, 0xa7, 0x47, 0x03} \
}

class nsCursor : public nsICursor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICURSOR

  friend class nsWindow;

  nsCursor();
  virtual ~nsCursor();

private:
  Display *mDisplay;
  Cursor mCursor;
};

#endif  // nsCursor_h___ 
