/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMsgNewsCID_h__
#define nsMsgNewsCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsMsgBaseCID.h"

//
// nsMsgNewsFolder
#define NS_NEWSFOLDERRESOURCE_PROGID \
  NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX "news"
#define NS_NEWSFOLDERRESOURCE_CID                    	\
{ /* 4ace448a-f6d4-11d2-880d-004005263078 */         	\
 0x4ace448a, 0xf6d4, 0x11d2, 				\
 {0x88, 0x0d, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} 	\
}

//
// nsNntpIncomingServer
//
#define NS_NNTPINCOMINGSERVER_PROGID \
  NS_MSGINCOMINGSERVER_PROGID_PREFIX "nntp"

#define NS_NNTPINCOMINGSERVER_CID 			\
{ /* 6ff28d0a-f776-11d2-87ca-004005263078 */ 		\
 0x6ff28d0a, 0xf776, 0x11d2, 				\
 {0x87, 0xca, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} 	\
}

//
// nsNntpService
//
#define NS_NNTPPROTOCOLINFO_PROGID		\
  NS_MSGPROTOCOLINFO_PROGID_PREFIX "nntp"
#define NS_NEWSPROTOCOLHANDLER_PROGID \
  NS_NETWORK_PROTOCOL_PROGID_PREFIX "news"
#define NS_SNEWSPROTOCOLHANDLER_PROGID \
  NS_NETWORK_PROTOCOL_PROGID_PREFIX "snews"
#define NS_NEWSMESSAGESERVICE_PROGID \
  "component://netscape/messenger/messageservice;type=news_message"
#define NS_NNTPMESSAGESERVICE_PROGID \
  "component://netscape/messenger/messageservice;type=news" 
#define NS_NNTPSERVICE_PROGID \
  "component://netscape/messenger/nntpservice"
#define NS_NEWSSTARTUPHANDLER_PROGID \
  "component://netscape/commandlinehandler/general-startup-news"

#define NS_NNTPSERVICE_CID                              \
{ /* 4C9F90E1-E19B-11d2-806E-006008128C4E */  		\
 0x4c9f90e1, 0xe19b, 0x11d2,                            \
 {0x80, 0x6e, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e} 	\
}              

//
// nsNNTPNewsgroup
//
#define NS_NNTPNEWSGROUP_PROGID \
  "component://netscape/messenger/nntpnewsgroup"
#define NS_NNTPNEWSGROUP_CID  \
{ /* 2c070e8a-187e-11d3-8e02-004005263078 */ \
 0x2c070e8a, 0x187e, 0x11d3, \
 {0x8e, 0x02, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} \
}

//
// nsNNTPNewsgroupPost
//
#define NS_NNTPNEWSGROUPPOST_PROGID \
  "component://netscape/messenger/nntpnewsgrouppost"
#define NS_NNTPNEWSGROUPPOST_CID  \
{ /* 30c60228-187e-11d3-842f-004005263078 */ \
 0x30c60228, 0x187e, 0x11d3, \
 {0x84, 0x2f, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} \
}

//
// nsNNTPNewsgroupList
//
#define NS_NNTPNEWSGROUPLIST_PROGID \
  "component://netscape/messenger/nntpnewsgrouplist"
#define NS_NNTPNEWSGROUPLIST_CID \
{ /* 631e9054-1893-11d3-9916-004005263078 */ \
 0x631e9054, 0x1893, 0x11d3, \
 {0x99, 0x16, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} \
}

//
// nsNNTPArticleList
//
#define NS_NNTPARTICLELIST_PROGID \
  "component://netscape/messenger/nntparticlelist"
#define NS_NNTPARTICLELIST_CID  \
{ /* 9f12bdf0-189f-11d3-973e-00805f916fd3 */ \
 0x9f12bdf0, 0x189f, 0x11d3, \
 {0x97, 0x3e, 0x00, 0x80, 0x5f, 0x91, 0x6f, 0xd3} \
}

//
// nsNNTPHost
//
#define NS_NNTPHOST_PROGID \
  "component://netscape/messenger/nntphost"
#define NS_NNTPHOST_CID \
{ /* 339e5576-196a-11d3-929a-004005263078 */ \
 0x339e5576, 0x196a, 0x11d3, \
 {0x92, 0x9a, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} \
}

//
// nsNntpUrl
//
#define NS_NNTPURL_PROGID \
  "component://netscape/messenger/nntpurl"
#define NS_NNTPURL_CID									\
{ /* 196B4B30-E18C-11d2-806E-006008128C4E */			\
  0x196b4b30, 0xe18c, 0x11d2,							\
    { 0x80, 0x6e, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

#endif // nsMsgNewsCID_h__
