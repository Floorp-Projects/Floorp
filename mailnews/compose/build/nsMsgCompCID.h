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

#ifndef nsMessageCompCID_h__
#define nsMessageCompCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

//
// nsMsgComposeService
//
#define NS_MSGCOMPOSESERVICE_CID				  \
{ /* 588595FE-1ADA-11d3-A715-0060B0EB39B5 */      \
 0x588595fe, 0x1ada, 0x11d3,                      \
 {0xa7, 0x15, 0x0, 0x60, 0xb0, 0xeb, 0x39, 0xb5}}

#define NS_MSGCOMPOSESERVICE_CONTRACTID			\
  "@mozilla.org/messengercompose;1"
#define NS_MSGCOMPOSESTARTUPHANDLER_CONTRACTID \
  "@mozilla.org/commandlinehandler/general-startup;1?type=compose"

//
// nsMsgCompose
//
#define NS_MSGCOMPOSE_CONTRACTID \
  "@mozilla.org/messengercompose/compose;1"

#define NS_MSGCOMPOSE_CID						  \
{ /* EB5BDAF8-BBC6-11d2-A6EC-0060B0EB39B5 */      \
 0xeb5bdaf8, 0xbbc6, 0x11d2,                      \
 {0xa6, 0xec, 0x0, 0x60, 0xb0, 0xeb, 0x39, 0xb5}}

//
// nsMsgCompFields
//
#define NS_MSGCOMPFIELDS_CONTRACTID \
  "@mozilla.org/messengercompose/composefields;1"

#define NS_MSGCOMPFIELDS_CID	                  \
{ /* 6D222BA0-BD46-11d2-8293-000000000000 */      \
 0x6d222ba0, 0xbd46, 0x11d2,                      \
 {0x82, 0x93, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}}

//
// nsMsgSend
//
#define NS_MSGSEND_CONTRACTID \
  "@mozilla.org/messengercompose/send;1"

#define NS_MSGSEND_CID							  \
{ /* 935284E0-C5D8-11d2-8297-000000000000 */      \
 0x935284e0, 0xc5d8, 0x11d2,                      \
 {0x82, 0x97, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}}

//
// nsMsgSendLater
//
#define NS_MSGSENDLATER_CONTRACTID							              \
  "@mozilla.org/messengercompose/sendlater;1"

#define NS_MSGSENDLATER_CID							              \
{ /* E15C83F1-1CF4-11d3-8EF0-00A024A7D144 */      \
 0xe15c83f1, 0x1cf4, 0x11d3,                      \
 {0x8e, 0xf0, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 }}

//
// nsSmtpUrl
//
#define NS_SMTPURL_CONTRACTID \
  "@mozilla.org/messengercompose/smtpurl;1"

#define NS_SMTPURL_CID                            \
{ /* BE59DBF0-2812-11d3-80A3-006008128C4E} */      \
 0xbe59dbf0, 0x2812, 0x11d3,                      \
 {0x80, 0xa3, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e}}

//
// nsMailtoUrl
//
#define NS_MAILTOURL_CONTRACTID \
  "@mozilla.org/messengercompose/mailtourl;1"

#define NS_MAILTOURL_CID                            \
{ /* 05BAB5E7-9C7D-11d3-98A3-001083010E9B} */       \
 0x5bab5e7, 0x9c7d, 0x11d3,                         \
 {0x98, 0xa3, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b}}

//
// nsSmtpServer
//
#define NS_SMTPSERVER_CONTRACTID \
  "@mozilla.org/messenger/smtp/server;1"

#define NS_SMTPSERVER_CID                      \
{ /* 60dc861a-56ce-11d3-9118-00a0c900d445 */   \
  0x60dc861a,0x56ce,0x11d3,										\
  {0x91,0x18, 0x0, 0xa0, 0xc9, 0x0, 0xd4, 0x45 }}

//
// nsSmtpService
//
#define NS_SMTPSERVICE_CONTRACTID \
  "@mozilla.org/messengercompose/smtp;1"

#define NS_MAILTOHANDLER_CONTRACTID \
  NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "mailto"

#define NS_SMTPSERVICE_CID						  \
{ /* 5B6419F1-CA9B-11d2-8063-006008128C4E */      \
 0x5b6419f1, 0xca9b, 0x11d2,                      \
 {0x80, 0x63, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e}}

//
// nsMsgQuote
//
#define NS_MSGQUOTE_CONTRACTID \
  "@mozilla.org/messengercompose/quoting;1"
#define NS_MSGQUOTE_CID \
  {0x1C7ABF0C, 0x21E5, 0x11d3, \
    { 0x8E, 0xF1, 0x00, 0xA0, 0x24, 0xA7, 0xD1, 0x44 }}

#define NS_MSGQUOTELISTENER_CONTRACTID \
  "@mozilla.org/messengercompose/quotinglistener;1"
#define NS_MSGQUOTELISTENER_CID \
  {0x683728ac, 0x88df, 0x11d3, \
    { 0x98, 0x9d, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b }}

//
// nsMsgDraft
//
#define NS_MSGDRAFT_CONTRACTID \
  "@mozilla.org/messengercompose/drafts;1"
#define NS_MSGDRAFT_CID \
  { 0xa623746c, 0x453b, 0x11d3, \
  { 0x8f, 0xf, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } }

//
// nsMsgRecipientArray
//
#define NS_MSGRECIPIENTARRAY_CONTRACTID \
  "@mozilla.org/messengercompose/recipientarray;1"
#define NS_MSGRECIPIENTARRAY_CID \
  {0x9d653e00, 0xa1ca, 0x11d3, \
    { 0xa4, 0x49, 0xf7, 0x9a, 0xb1, 0xc7, 0xd8, 0xfc }}


// compose string service routines 

/* B51E56BD-FF80-11d3-98BC-001083010E9B */
#define NS_MSG_COMPOSESTRINGSERVICE_CID          \
{ 0xb51e56bd, 0xff80, 0x11d3,                  \
    { 0x98, 0xbc, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b } }

#define NS_MSG_SMTPSTRINGSERVICE_CONTRACTID \
  NS_MAILNEWS_STRINGSERVICE_CONTRACTID_PREFIX "smtp"

#define NS_MSG_COMPOSESTRINGSERVICE_CONTRACTID \
  NS_MAILNEWS_STRINGSERVICE_CONTRACTID_PREFIX "compose"

//
// nsSmtpDataSource
//
#define NS_SMTPDATASOURCE_CONTRACTID \
  NS_RDF_DATASOURCE_CONTRACTID_PREFIX "smtp"

// {0C612140-25FE-11d4-9A27-0010A4E03656}
#define NS_SMTPDATASOURCE_CID \
{ 0xc612140, 0x25fe, 0x11d4, \
 { 0x9a, 0x27, 0x0, 0x10, 0xa4, 0xe0, 0x36, 0x56 } }

//
// nsSmtpDelegateFactory
//
#define NS_SMTPDELEGATEFACTORY_CONTRACTID \
  NS_RDF_DELEGATEFACTORY_CONTRACTID_PREFIX "smtpserver/smtp;1"

// {0C612141-25FE-11d4-9A27-0010A4E03656}
#define NS_SMTPDELEGATEFACTORY_CID \
{ 0xc612141, 0x25fe, 0x11d4, \
 { 0x9a, 0x27, 0x0, 0x10, 0xa4, 0xe0, 0x36, 0x56 } }


#endif // nsMessageCompCID_h__
