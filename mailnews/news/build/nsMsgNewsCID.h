/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsMsgNewsCID_h__
#define nsMsgNewsCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

#define NS_NEWSFOLDERRESOURCE_CID                    	\
{ /* 4ace448a-f6d4-11d2-880d-004005263078 */         	\
 0x4ace448a, 0xf6d4, 0x11d2, 				\
 {0x88, 0x0d, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} 	\
}

#define NS_NEWSMESSAGERESOURCE_CID               	\
{ /* 2dae7f80-f104-11d2-973b-00805f916fd3 */		\
 0x2dae7f80, 0xf104, 0x11d2, 				\
 {0x97, 0x3b, 0x00, 0x80, 0x5f, 0x91, 0x6f, 0xd3}	\
} 

#define NS_NNTPINCOMINGSERVER_CID 			\
{ /* 6ff28d0a-f776-11d2-87ca-004005263078 */ 		\
 0x6ff28d0a, 0xf776, 0x11d2, 				\
 {0x87, 0xca, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} 	\
}

#define NS_NNTPPROTOCOLINFO_PROGID		\
  NS_MSGPROTOCOLINFO_PROGID_PREFIX "nntp"

#define NS_NNTPSERVICE_CID                              \
{ /* 4C9F90E1-E19B-11d2-806E-006008128C4E */  		\
 0x4c9f90e1, 0xe19b, 0x11d2,                            \
 {0x80, 0x6e, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e} 	\
}              

#define NS_NNTPPROTOCOL_CID \
{ /* 43f594c8-184f-11d3-9e09-004005263078 */ \
 0x43f594c8, 0x184f, 0x11d3, \
 {0x9e, 0x09, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} \
}

#define NS_NNTPNEWSGROUP_CID  \
{ /* 2c070e8a-187e-11d3-8e02-004005263078 */ \
 0x2c070e8a, 0x187e, 0x11d3, \
 {0x8e, 0x02, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} \
}

#define NS_NNTPNEWSGROUPPOST_CID  \
{ /* 30c60228-187e-11d3-842f-004005263078 */ \
 0x30c60228, 0x187e, 0x11d3, \
 {0x84, 0x2f, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} \
}

#define NS_NNTPNEWSGROUPLIST_CID \
{ /* 631e9054-1893-11d3-9916-004005263078 */ \
 0x631e9054, 0x1893, 0x11d3, \
 {0x99, 0x16, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} \
}

#define NS_NNTPARTICLELIST_CID  \
{ /* 9f12bdf0-189f-11d3-973e-00805f916fd3 */ \
 0x9f12bdf0, 0x189f, 0x11d3, \
 {0x97, 0x3e, 0x00, 0x80, 0x5f, 0x91, 0x6f, 0xd3} \
}

#define NS_NNTPHOST_CID \
{ /* 339e5576-196a-11d3-929a-004005263078 */ \
 0x339e5576, 0x196a, 0x11d3, \
 {0x92, 0x9a, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} \
}

#define NS_NNTPURL_CID									\
{ /* 196B4B30-E18C-11d2-806E-006008128C4E */			\
  0x196b4b30, 0xe18c, 0x11d2,							\
    { 0x80, 0x6e, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }


#endif // nsMsgNewsCID_h__
