/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is XMLterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 */

// mozIXMLTermSuspend.h: interface to suspend/resume select XMLterm operations
//

#ifndef mozIXMLTermSuspend_h___
#define mozIXMLTermSuspend_h___

#include "nscore.h"

#include "nsISupports.h"

/* {0eb82b50-43a2-11d3-8e76-006008948af5} */
#define MOZIXMLTERMSUSPEND_IID_STR "0eb82b50-43a2-11d3-8e76-006008948af5"
#define MOZIXMLTERMSUSPEND_IID \
  {0x0eb82b50, 0x43a2, 0x11d3, \
    { 0x8e, 0x76, 0x00, 0x60, 0x08, 0x94, 0x8a, 0xf5 }}

class mozIXMLTermSuspend : public nsISupports
{
  public:
  NS_DEFINE_STATIC_IID_ACCESSOR(MOZIXMLTERMSUSPEND_IID);

  // mozIXMLTermSuspend interface
  NS_IMETHOD GetSuspend(PRBool* aSuspend) = 0;
  NS_IMETHOD SetSuspend(const PRBool aSuspend) = 0;

};

#endif /* mozIXMLTermSuspend_h___ */
