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

#ifndef nsMessageBaseCID_h__
#define nsMessageBaseCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

#define NS_MSGFOLDEREVENT_CID				              \
{ /* FBFEBE7A-C1DD-11d2-8A40-0060B0FC04D2 */      \
 0xfbfebe7a, 0xc1dd, 0x11d2,                      \
 {0x8a, 0x40, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xd2}}

#define NS_MSGGROUPRECORD_CID                     \
{ /* a8f54ee0-d292-11d2-b7f6-00805f05ffa5 */      \
 0xa8f54ee0, 0xd292, 0x11d2,                      \
 {0xb7, 0xf6, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5}}

//
// nsMsgFolderDataSource
//
#define NS_MAILNEWSFOLDERDATASOURCE_PROGID \
  NS_RDF_DATASOURCE_PROGID_PREFIX "mailnewsfolders"

#define NS_MAILNEWSFOLDERDATASOURCE_CID                    \
{ /* 2B8ED4A4-F684-11d2-8A5D-0060B0FC04D2 */         \
    0x2b8ed4a4,                                      \
    0xf684,                                          \
    0x11d2,                                          \
    {0x8a, 0x5d, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xd2} \
}

//
// nsMsgMessageDataSource
//
#define NS_MAILNEWSMESSAGEDATASOURCE_PROGID \
  NS_RDF_DATASOURCE_PROGID_PREFIX "mailnewsmessages"

#define NS_MAILNEWSMESSAGEDATASOURCE_CID                    \
{ /* 2B8ED4A5-F684-11d2-8A5D-0060B0FC04D2 */         \
    0x2b8ed4a5,                                      \
    0xf684,                                          \
    0x11d2,                                          \
    {0x8a, 0x5d, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xd2} \
}


//
// nsMessageViewDataSource
//
#define NS_MESSAGEVIEWDATASOURCE_PROGID \
  NS_RDF_DATASOURCE_PROGID_PREFIX "mail-messageview"

#define NS_MESSAGEVIEWDATASOURCE_CID				\
{ /* 14495573-E945-11d2-8A52-0060B0FC04D2 */		\
0x14495573, 0xe945, 0x11d2,							\
{0x8a, 0x52, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xd2}}


//
// nsMsgAccountManager
// 
#define NS_MSGACCOUNTMANAGER_PROGID \
  "component://netscape/messenger/account-manager"

#define NS_MSGACCOUNTMANAGER_CID									\
{ /* D2876E50-E62C-11d2-B7FC-00805F05FFA5 */			\
 0xd2876e50, 0xe62c, 0x11d2,											\
 {0xb7, 0xfc, 0x0, 0x80, 0x5f, 0x5, 0xff, 0xa5 }}

//
// nsMsgIdentity
//
#define NS_MSGIDENTITY_PROGID \
  "component://netscape/messenger/identity"

#define NS_MSGIDENTITY_CID												\
{ /* 8fbf6ac0-ebcc-11d2-b7fc-00805f05ffa5 */			\
 0x8fbf6ac0, 0xebcc, 0x11d2,											\
 {0xb7, 0xfc, 0x0, 0x80, 0x5f, 0x5, 0xff, 0xa5 }}

//
// nsMsgIncomingServer
#define NS_MSGINCOMINGSERVER_PROGID_PREFIX \
  "component://netscape/messenger/server&type="

#define NS_MSGINCOMINGSERVER_PROGID \
  NS_MSGINCOMINGSERVER_PROGID_PREFIX "generic"

/* {66e5ff08-5126-11d3-9711-006008948010} */
#define NS_MSGINCOMINGSERVER_CID \
  {0x66e5ff08, 0x5126, 0x11d3, \
    {0x97, 0x11, 0x00, 0x60, 0x08, 0x94, 0x80, 0x10}}


//
// nsMsgAccount
//
#define NS_MSGACCOUNT_PROGID \
  "component://netscape/messenger/account"

#define NS_MSGACCOUNT_CID													\
{ /* 68b25510-e641-11d2-b7fc-00805f05ffa5 */			\
 0x68b25510, 0xe641, 0x11d2,											\
 {0xb7, 0xfc, 0x0, 0x80, 0x5f, 0x5, 0xff, 0xa5 }}

//
// nsMsgFilterService
//
#define NS_MSGFILTERSERVICE_PROGID \
  "component://netscape/messenger/services/filters"

#define NS_MSGFILTERSERVICE_CID                         \
{ 0x5cbb0700, 0x04bc, 0x11d3,                 \
    { 0xa5, 0x0a, 0x0, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }


//
// nsMsgSearchSession
//
/* e9a7cd70-0303-11d3-a50a-0060b0fc04b7 */
#define NS_MSGSEARCHSESSION_CID						  \
{ 0xe9a7cd70, 0x0303, 0x11d3,                 \
    { 0xa5, 0x0a, 0x0, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

//
// nsMsgMailSession
//
#define NS_MSGMAILSESSION_PROGID \
  "component://netscape/messenger/services/session"

/* D5124441-D59E-11d2-806A-006008128C4E */
#define NS_MSGMAILSESSION_CID							\
{ 0xd5124441, 0xd59e, 0x11d2,							\
    { 0x80, 0x6a, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

//
// nsMsgBiffManager
//
#define NS_MSGBIFFMANAGER_PROGID \
  "component://netscape/messenger/biffManager"

/* 4A374E7E-190F-11d3-8A88-0060B0FC04D2 */
#define NS_MSGBIFFMANAGER_CID							\
{ 0x4a374e7e, 0x190f, 0x11d3,							\
    { 0x8a, 0x88, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xd2 } }

//
// nsMsgNotificationManager
//
#define NS_MSGNOTIFICATIONMANAGER_PROGID \
  NS_RDF_DATASOURCE_PROGID_PREFIX "msgnotifications"

/* 7C601F60-1EF3-11d3-9574-006097222B83 */
#define NS_MSGNOTIFICATIONMANAGER_CID							\
{ 0x7c601f60, 0x1ef3, 0x11d3,							\
    { 0x95, 0x74, 0x0, 0x60, 0x97, 0x22, 0x2b, 0x83 } }

//
// nsCopyMessageStreamListener
//
#define NS_COPYMESSAGESTREAMLISTENER_PROGID \
  "component://netscape/messenger/copymessagestreamlistener"

#define NS_COPYMESSAGESTREAMLISTENER_CID							\
{ 0x7741daed, 0x2125, 0x11d3,							\
    { 0x8a, 0x90, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xd2 } }

//
// nsMsgCopyService
//
#define NS_MSGCOPYSERVICE_PROGID \
  "component://netscape/messenger/messagecopyservice"

/* c766e666-29bd-11d3-afb3-001083002da8 */
#define NS_MSGCOPYSERVICE_CID \
{ 0xc766e666, 0x29bd, 0x11d3, \
    { 0xaf, 0xb3, 0x00, 0x10, 0x83, 0x00, 0x2d, 0xa8 } }

#define NS_MSGFOLDERCACHE_PROGID \
	"component://netscape/messenger/msgFolderCache"

/* bcdca970-3b22-11d3-8d76-00805f8a6617 */
#define NS_MSGFOLDERCACHE_CID \
{ 0xbcdca970, 0x3b22, 0x11d3,  \
	{ 0x8d, 0x76, 0x00, 0x80, 0xf5, 0x8a, 0x66, 0x17 } }

//
// nsUrlListenerManager
//
#define NS_URLLISTENERMANAGER_PROGID \
  "component://netscape/messenger/urlListenerManager"

/* B1AA0820-D04B-11d2-8069-006008128C4E */
#define NS_URLLISTENERMANAGER_CID \
{ 0xb1aa0820, 0xd04b, 0x11d2, \
  {0x80, 0x69, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e} }

//
// nsMessengerBootstrap
//
#define NS_MESSENGERBOOTSTRAP_PROGID \
  "component://netscape/appshell/component/messenger"

//
// nsMessenger
//
#define NS_MESSENGER_PROGID	\
  "component://netscape/messenger"

//
// nsMsgStatusFeedback
//
#define NS_MSGSTATUSFEEDBACK_PROGID \
  "component://netscape/messenger/statusfeedback"

/* B1AA0820-D04B-11d2-8069-006008128C4E */
#define NS_MSGSTATUSFEEDBACK_CID \
{ 0xbd85a417, 0x5433, 0x11d3, \
  {0x8a, 0xc5, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xd2} }


#endif // nsMessageBaseCID_h__
