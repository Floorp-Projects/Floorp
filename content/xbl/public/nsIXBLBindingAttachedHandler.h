/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

/*

  Private interface to the XBL Binding Attached handler

*/

#ifndef nsIXBLBindingAttachedHandler_h__
#define nsIXBLBindingAttachedHandler_h__

#include "nsISupports.h"

// {861E971F-D48F-4889-91CD-2A711FA4FDBA}
#define NS_IXBLBINDINGATTACHEDHANDLER_IID \
{ 0x861e971f, 0xd48f, 0x4889, { 0x91, 0xcd, 0x2a, 0x71, 0x1f, 0xa4, 0xfd, 0xba } }

class nsIXBLBindingAttachedHandler : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXBLBINDINGATTACHEDHANDLER_IID; return iid; }

  NS_IMETHOD OnBindingAttached()=0;
};

#endif // nsIXBLBindingAttachedHandler_h__
