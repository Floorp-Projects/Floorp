/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMsgNewsCID_h__
#define nsMsgNewsCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsMsgBaseCID.h"

//
// nsMsgNewsFolder
#define NS_NEWSFOLDERRESOURCE_CONTRACTID \
  NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX "news"
#define NS_NEWSFOLDERRESOURCE_CID                    	\
{ /* 4ace448a-f6d4-11d2-880d-004005263078 */         	\
 0x4ace448a, 0xf6d4, 0x11d2, 				\
 {0x88, 0x0d, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} 	\
}

//
// nsNntpIncomingServer
//
#define NS_NNTPINCOMINGSERVER_CONTRACTID \
  NS_MSGINCOMINGSERVER_CONTRACTID_PREFIX "nntp"

#define NS_NNTPINCOMINGSERVER_CID 			\
{ /* 6ff28d0a-f776-11d2-87ca-004005263078 */ 		\
 0x6ff28d0a, 0xf776, 0x11d2, 				\
 {0x87, 0xca, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} 	\
}

//
// nsNntpService
//

#define NS_NNTPPROTOCOLINFO_CONTRACTID		\
  NS_MSGPROTOCOLINFO_CONTRACTID_PREFIX "nntp"

#define NS_NEWSPROTOCOLHANDLER_CONTRACTID \
  NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "news"
#define NS_SNEWSPROTOCOLHANDLER_CONTRACTID \
  NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "snews"
#define NS_NNTPPROTOCOLHANDLER_CONTRACTID \
  NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "nntp"
#define NS_NEWSMESSAGEPROTOCOLHANDLER_CONTRACTID \
  NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "news_message"
#define NS_NEWSMESSAGESERVICE_CONTRACTID \
  "@mozilla.org/messenger/messageservice;1?type=news_message"
#define NS_NNTPMESSAGESERVICE_CONTRACTID \
  "@mozilla.org/messenger/messageservice;1?type=news" 
#define NS_NNTPSERVICE_CONTRACTID \
  "@mozilla.org/messenger/nntpservice;1"
#define NS_NEWSSTARTUPHANDLER_CONTRACTID \
  "@mozilla.org/commandlinehandler/general-startup;1?type=news"

#define NS_NNTPSERVICE_CID                              \
{ /* 4C9F90E1-E19B-11d2-806E-006008128C4E */  		\
 0x4c9f90e1, 0xe19b, 0x11d2,                            \
 {0x80, 0x6e, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e} 	\
}              

//
// nsNNTPNewsgroupPost
//
#define NS_NNTPNEWSGROUPPOST_CONTRACTID \
  "@mozilla.org/messenger/nntpnewsgrouppost;1"
#define NS_NNTPNEWSGROUPPOST_CID  \
{ /* 30c60228-187e-11d3-842f-004005263078 */ \
 0x30c60228, 0x187e, 0x11d3, \
 {0x84, 0x2f, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} \
}

//
// nsNNTPNewsgroupList
//
#define NS_NNTPNEWSGROUPLIST_CONTRACTID \
  "@mozilla.org/messenger/nntpnewsgrouplist;1"
#define NS_NNTPNEWSGROUPLIST_CID \
{ /* 631e9054-1893-11d3-9916-004005263078 */ \
 0x631e9054, 0x1893, 0x11d3, \
 {0x99, 0x16, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78} \
}

//
// nsNNTPArticleList
//
#define NS_NNTPARTICLELIST_CONTRACTID \
  "@mozilla.org/messenger/nntparticlelist;1"
#define NS_NNTPARTICLELIST_CID  \
{ /* 9f12bdf0-189f-11d3-973e-00805f916fd3 */ \
 0x9f12bdf0, 0x189f, 0x11d3, \
 {0x97, 0x3e, 0x00, 0x80, 0x5f, 0x91, 0x6f, 0xd3} \
}

//
// nsNntpUrl
//
#define NS_NNTPURL_CONTRACTID \
  "@mozilla.org/messenger/nntpurl;1"
#define NS_NNTPURL_CID									\
{ /* 196B4B30-E18C-11d2-806E-006008128C4E */			\
  0x196b4b30, 0xe18c, 0x11d2,							\
    { 0x80, 0x6e, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

//
// nsNewsDownloadDialogArgs 
//
#define NS_NEWSDOWNLOADDIALOGARGS_CONTRACTID \
   "@mozilla.org/messenger/newsdownloaddialogargs;1"
#define NS_NEWSDOWNLOADDIALOGARGS_CID   \
{ /* 1540689e-1dd2-11b2-933d-f0d1e460ef4a */    \
  0x1540689e, 0x1dd2, 0x11b2,   \
    { 0x93, 0x3d, 0xf0, 0xd1, 0xe4, 0x60, 0xef, 0x4a} }

#endif // nsMsgNewsCID_h__
