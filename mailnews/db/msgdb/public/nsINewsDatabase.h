/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsINewsDatabase_h__
#define nsINewsDatabase_h__

#include "nsISupports.h"

class nsMsgKeySet; 

#define NS_INEWSDATABASE_IID \
{0x6c7c2890, 0x2f62, 0x11d3, {0x97, 0x3f, 0x00, 0x80, 0x5f, 0x91, 0x6f, 0xd3} }

class nsINewsDatabase : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_INEWSDATABASE_IID; return iid; }

  NS_IMETHOD GetUnreadSet(nsMsgKeySet **pSet) = 0;
  NS_IMETHOD SetUnreadSet(const char * setStr) = 0;    
};

#endif // nsINewsDatabase_h__

