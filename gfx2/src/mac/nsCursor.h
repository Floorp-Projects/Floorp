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

#include <Quickdraw.h>

#define NS_CURSOR_CID \
{ /* 312707a4-1dd2-11b2-9453-cff424d9e371 */         \
     0x312707a4,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x94, 0x53, 0xcf, 0xf4, 0x24, 0xd9, 0xe3, 0x71} \
}

class nsCursor : public nsICursor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICURSOR

  nsCursor();
  virtual ~nsCursor();

private:
    CursHandle mCursor;
};

#endif  // nsCursor_h___ 
