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

#ifndef nsMsgLocalCID_h__
#define nsMsgLocalCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsMsgBaseCID.h"

#define NS_POP3INCOMINGSERVER_TYPE "pop3"

//
// nsLocalMailFolderResourceCID
//
#define NS_LOCALMAILFOLDERRESOURCE_CONTRACTID \
  NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX "mailbox"
#define  NS_LOCALMAILFOLDERRESOURCE_CID              \
{ /* e490d22c-cd67-11d2-8cca-0060b0fc14a3 */         \
	0xe490d22c,										 \
    0xcd67,                                          \
    0x11d2,                                          \
    {0x8c, 0xca, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

//
// nsPop3IncomingServer
//
#define NS_POP3INCOMINGSERVER_CONTRACTID \
  NS_MSGINCOMINGSERVER_CONTRACTID_PREFIX NS_POP3INCOMINGSERVER_TYPE

#define NS_POP3INCOMINGSERVER_CID									\
{ /* D2876E51-E62C-11d2-B7FC-00805F05FFA5 */			\
 0xd2876e51, 0xe62c, 0x11d2,											\
 {0xb7, 0xfc, 0x0, 0x80, 0x5f, 0x5, 0xff, 0xa5 }}

//
// nsNoIncomingServer
//
#define NS_NOINCOMINGSERVER_CONTRACTID \
  NS_MSGINCOMINGSERVER_CONTRACTID_PREFIX "none"

#define NS_NOINCOMINGSERVER_CID            	\
{ /* {ca5ffe7e-5f47-11d3-9a51-004005263078} */	\
  0xca5ffe7e, 0x5f47, 0x11d3, 			\
  {0x9a, 0x51, 0x00, 0x40, 0x05, 0x26, 0x30, 0x78}}


//
// nsMsgMailboxService
#define NS_MAILBOXSERVICE_CONTRACTID1	\
  "@mozilla.org/messenger/mailboxservice;1"

#define NS_MAILBOXSERVICE_CONTRACTID2 \
  "@mozilla.org/messenger/messageservice;1?type=mailbox"

#define NS_MAILBOXSERVICE_CONTRACTID3 \
  "@mozilla.org/messenger/messageservice;1?type=mailbox_message"

#define NS_MAILBOXSERVICE_CONTRACTID4 \
  NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "mailbox"

#define NS_MAILBOXSERVICE_CID                         \
{ /* EEF82462-CB69-11d2-8065-006008128C4E */      \
 0xeef82462, 0xcb69, 0x11d2,                      \
 {0x80, 0x65, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e}}


//
// nsMailboxUrl
//
#define NS_MAILBOXURL_CONTRACTID	\
  "@mozilla.org/messenger/mailboxurl;1"

/* 46EFCB10-CB6D-11d2-8065-006008128C4E */
#define NS_MAILBOXURL_CID                      \
{ 0x46efcb10, 0xcb6d, 0x11d2,                  \
    { 0x80, 0x65, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }


//
// nsPop3Url
//
#define NS_POP3URL_CONTRACTID \
  "@mozilla.org/messenger/popurl;1"

/* EA1B0A11-E6F4-11d2-8070-006008128C4E */
#define NS_POP3URL_CID                         \
{ 0xea1b0a11, 0xe6f4, 0x11d2,                   \
    { 0x80, 0x70, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }


//
// nsPop3Service
//

#define NS_POP3SERVICE_CONTRACTID1 \
  "@mozilla.org/messenger/popservice;1"

#define NS_POP3SERVICE_CONTRACTID2 \
  NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "pop"

#define NS_POP3PROTOCOLINFO_CONTRACTID \
  NS_MSGPROTOCOLINFO_CONTRACTID_PREFIX NS_POP3INCOMINGSERVER_TYPE

#define NS_POP3SERVICE_CID								\
{ /* 3BB459E3-D746-11d2-806A-006008128C4E */			\
 0x3bb459e3, 0xd746, 0x11d2,							\
  { 0x80, 0x6a, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e }}

//
// nsNoneService
//

#define NS_NONESERVICE_CONTRACTID \
  "@mozilla.org/messenger/noneservice;1"

#define NS_NONEPROTOCOLINFO_CONTRACTID \
  NS_MSGPROTOCOLINFO_CONTRACTID_PREFIX "none"

#define NS_NONESERVICE_CID								\
{ /* 75b63b46-1dd2-11b2-9873-bb375e1550fa */			\
 0x75b63b46, 0x1dd2, 0x11b2,							\
 { 0x98, 0x73, 0xbb, 0x37, 0x5e, 0x15, 0x50, 0xfa }}

//
// nsParseMailMsgState
//
#define NS_PARSEMAILMSGSTATE_CONTRACTID \
  "@mozilla.org/messenger/messagestateparser;1"

#define NS_PARSEMAILMSGSTATE_CID \
{ /* 2B79AC51-1459-11d3-8097-006008128C4E */ \
 0x2b79ac51, 0x1459, 0x11d3, \
  {0x80, 0x97, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e} }

//
// nsMsgMailboxParser
//

#define NS_MAILBOXPARSER_CONTRACTID \
  "@mozilla.org/messenger/mailboxparser;1"

/* 46EFCB10-CB6D-11d2-8065-006008128C4E */
#define NS_MAILBOXPARSER_CID                      \
{ 0x8597ab60, 0xd4e2, 0x11d2,                  \
    { 0x80, 0x69, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

/* 15263446-D55E-11d3-98B1-001083010E9B */
#define NS_MSG_LOCALSTRINGSERVICE_CID          \
{ 0x15263446, 0xd55e, 0x11d3,                  \
    { 0x98, 0xb1, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b } }

#define NS_MSG_MAILBOXSTRINGSERVICE_CONTRACTID \
  NS_MAILNEWS_STRINGSERVICE_CONTRACTID_PREFIX "mailbox"

#define NS_MSG_POPSTRINGSERVICE_CONTRACTID \
  NS_MAILNEWS_STRINGSERVICE_CONTRACTID_PREFIX NS_POP3INCOMINGSERVER_TYPE

#endif // nsMsgLocalCID_h__
