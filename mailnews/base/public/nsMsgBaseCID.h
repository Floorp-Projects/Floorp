/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMessageBaseCID_h__
#define nsMessageBaseCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

//
// nsMsgFolderDataSource
//
#define NS_MAILNEWSFOLDERDATASOURCE_CONTRACTID \
  NS_RDF_DATASOURCE_CONTRACTID_PREFIX "mailnewsfolders"

#define NS_MAILNEWSFOLDERDATASOURCE_CID                    \
{ /* 2B8ED4A4-F684-11d2-8A5D-0060B0FC04D2 */         \
    0x2b8ed4a4,                                      \
    0xf684,                                          \
    0x11d2,                                          \
    {0x8a, 0x5d, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xd2} \
}

//
// nsMsgAccountManager
// 
#define NS_MSGACCOUNTMANAGER_CONTRACTID \
  "@mozilla.org/messenger/account-manager;1"

#define NS_MSGACCOUNTMANAGER_CID									\
{ /* D2876E50-E62C-11d2-B7FC-00805F05FFA5 */			\
 0xd2876e50, 0xe62c, 0x11d2,											\
 {0xb7, 0xfc, 0x0, 0x80, 0x5f, 0x5, 0xff, 0xa5 }}

// 
// nsMessengerMigrator
//
#define NS_MESSENGERMIGRATOR_CONTRACTID \
	"@mozilla.org/messenger/migrator;1"

#define NS_MESSENGERMIGRATOR_CID	\
{ /* 54818d98-1dd2-11b2-82aa-a9197f997503 */	\
	0x54818d98, 0x1dd2, 0x11b2,	\
	{ 0x82, 0xaa, 0xa9, 0x19, 0x7f, 0x99, 0x75, 0x03}}



//
// nsMsgIdentity
//
#define NS_MSGIDENTITY_CONTRACTID \
  "@mozilla.org/messenger/identity;1"

#define NS_MSGIDENTITY_CID												\
{ /* 8fbf6ac0-ebcc-11d2-b7fc-00805f05ffa5 */			\
 0x8fbf6ac0, 0xebcc, 0x11d2,											\
 {0xb7, 0xfc, 0x0, 0x80, 0x5f, 0x5, 0xff, 0xa5 }}

//
// nsMsgIncomingServer
#define NS_MSGINCOMINGSERVER_CONTRACTID_PREFIX \
  "@mozilla.org/messenger/server;1?type="

#define NS_MSGINCOMINGSERVER_CONTRACTID \
  NS_MSGINCOMINGSERVER_CONTRACTID_PREFIX "generic"

/* {66e5ff08-5126-11d3-9711-006008948010} */
#define NS_MSGINCOMINGSERVER_CID \
  {0x66e5ff08, 0x5126, 0x11d3, \
    {0x97, 0x11, 0x00, 0x60, 0x08, 0x94, 0x80, 0x10}}


//
// nsMsgAccount
//
#define NS_MSGACCOUNT_CONTRACTID \
  "@mozilla.org/messenger/account;1"

#define NS_MSGACCOUNT_CID													\
{ /* 68b25510-e641-11d2-b7fc-00805f05ffa5 */			\
 0x68b25510, 0xe641, 0x11d2,											\
 {0xb7, 0xfc, 0x0, 0x80, 0x5f, 0x5, 0xff, 0xa5 }}

//
// nsMsgFilterService
//
#define NS_MSGFILTERSERVICE_CONTRACTID \
  "@mozilla.org/messenger/services/filters;1"

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

#define NS_MSGSEARCHSESSION_CONTRACTID \
  "@mozilla.org/messenger/searchSession;1"

/* E1DA397D-FDC5-4b23-A6FE-D46A034D80B3 */
#define NS_MSGSEARCHTERM_CID						  \
{ 0xe1da397d, 0xfdc5, 0x4b23,                 \
    { 0xa6, 0xfe, 0xd4, 0x6a, 0x3, 0x4d, 0x80, 0xb3 } }

#define NS_MSGSEARCHTERM_CONTRACTID \
  "@mozilla.org/messenger/searchTerm;1"

//
// nsMsgSearchValidityManager
//
/* 1510faee-ad1a-4194-8039-33de32d5a882 */
#define NS_MSGSEARCHVALIDITYMANAGER_CID \
  {0x1510faee, 0xad1a, 0x4194, \
    { 0x80, 0x39, 0x33, 0xde, 0x32, 0xd5, 0xa8, 0x82 }}

#define NS_MSGSEARCHVALIDITYMANAGER_CONTRACTID \
  "@mozilla.org/mail/search/validityManager;1"

//
// nsMsgMailSession
//
#define NS_MSGMAILSESSION_CONTRACTID \
  "@mozilla.org/messenger/services/session;1"

/* D5124441-D59E-11d2-806A-006008128C4E */
#define NS_MSGMAILSESSION_CID							\
{ 0xd5124441, 0xd59e, 0x11d2,							\
    { 0x80, 0x6a, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

//
// nsMsgBiffManager
//
#define NS_MSGBIFFMANAGER_CONTRACTID \
  "@mozilla.org/messenger/biffManager;1"

/* 4A374E7E-190F-11d3-8A88-0060B0FC04D2 */
#define NS_MSGBIFFMANAGER_CID							\
{ 0x4a374e7e, 0x190f, 0x11d3,							\
    { 0x8a, 0x88, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xd2 } }


//
// nsMsgPurgeService
//
#define NS_MSGPURGESERVICE_CONTRACTID \
  "@mozilla.org/messenger/purgeService;1"

/* a687b474-afd8-418f-8ad9-f362202ae9a9 */
#define NS_MSGPURGESERVICE_CID							\
{ 0xa687b474, 0xafd8, 0x418f,							\
    { 0x8a, 0xd9, 0xf3, 0x62, 0x20, 0x2a, 0xe9, 0xa9 } }

//
// nsStatusBarBiffManager
//
#define NS_STATUSBARBIFFMANAGER_CONTRACTID \
  "@mozilla.org/messenger/statusBarBiffManager;1"

/* 7f9a9fb0-4161-11d4-9876-00c04fa0d2a6 */
#define NS_STATUSBARBIFFMANAGER_CID                \
{ 0x7f9a9fb0, 0x4161, 0x11d4,                      \
  {0x98, 0x76, 0x00, 0xc0, 0x4f, 0xa0, 0xd2, 0xa6} }

//
// nsCopyMessageStreamListener
//
#define NS_COPYMESSAGESTREAMLISTENER_CONTRACTID \
  "@mozilla.org/messenger/copymessagestreamlistener;1"

#define NS_COPYMESSAGESTREAMLISTENER_CID							\
{ 0x7741daed, 0x2125, 0x11d3,							\
    { 0x8a, 0x90, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xd2 } }

//
// nsMsgCopyService
//
#define NS_MSGCOPYSERVICE_CONTRACTID \
  "@mozilla.org/messenger/messagecopyservice;1"

/* c766e666-29bd-11d3-afb3-001083002da8 */
#define NS_MSGCOPYSERVICE_CID \
{ 0xc766e666, 0x29bd, 0x11d3, \
    { 0xaf, 0xb3, 0x00, 0x10, 0x83, 0x00, 0x2d, 0xa8 } }

#define NS_MSGFOLDERCACHE_CONTRACTID \
	"@mozilla.org/messenger/msgFolderCache;1"

/* bcdca970-3b22-11d3-8d76-00805f8a6617 */
#define NS_MSGFOLDERCACHE_CID \
{ 0xbcdca970, 0x3b22, 0x11d3,  \
	{ 0x8d, 0x76, 0x00, 0x80, 0xf5, 0x8a, 0x66, 0x17 } }

//
// nsUrlListenerManager
//
#define NS_URLLISTENERMANAGER_CONTRACTID \
  "@mozilla.org/messenger/urlListenerManager;1"

/* B1AA0820-D04B-11d2-8069-006008128C4E */
#define NS_URLLISTENERMANAGER_CID \
{ 0xb1aa0820, 0xd04b, 0x11d2, \
  {0x80, 0x69, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e} }

//
// nsMessengerBootstrap
//
#define NS_MESSENGERBOOTSTRAP_CONTRACTID \
  "@mozilla.org/appshell/component/messenger;1"
#define NS_MAILSTARTUPHANDLER_CONTRACTID \
  "@mozilla.org/commandlinehandler/general-startup;1?type=mail"
#define NS_MESSENGERWINDOWSERVICE_CONTRACTID \
  "@mozilla.org/messenger/windowservice;1"
#define NS_MESSENGERWINDOWSERVICE_CID \
{ 0xa01b6724, 0x1dd1, 0x11b2, \
  {0xaa, 0xb9, 0x82,0xf2, 0x4c,0x59, 0x5f, 0x41} }

//
// nsMessenger
//
#define NS_MESSENGER_CONTRACTID	\
  "@mozilla.org/messenger;1"

//
// nsMsgStatusFeedback
//
#define NS_MSGSTATUSFEEDBACK_CONTRACTID \
  "@mozilla.org/messenger/statusfeedback;1"

/* B1AA0820-D04B-11d2-8069-006008128C4E */
#define NS_MSGSTATUSFEEDBACK_CID \
{ 0xbd85a417, 0x5433, 0x11d3, \
  {0x8a, 0xc5, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xd2} }

//
//nsMsgWindow
//
#define NS_MSGWINDOW_CONTRACTID \
	"@mozilla.org/messenger/msgwindow;1"

/* BB460DFF-8BF0-11d3-8AFE-0060B0FC04D2*/
#define NS_MSGWINDOW_CID \
{ 0xbb460dff, 0x8bf0, 0x11d3, \
  { 0x8a, 0xfe, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xd2}}

//
// Print Engine...
//
#define NS_MSGPRINTENGINE_CONTRACTID \
  "@mozilla.org/messenger/msgPrintEngine;1"

#define NS_MSG_PRINTENGINE_CID                  \
  { /* 91FD6B19-E0BC-11d3-8F97-000064657374 */  \
    0x91fd6b19, 0xe0bc, 0x11d3,			\
  { 0x8f, 0x97, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

//
// nsMsgServiceProviderService
//
#define NS_MSGSERVICEPROVIDERSERVICE_CONTRACTID \
  NS_RDF_DATASOURCE_CONTRACTID_PREFIX "ispdefaults"
  
/* 10998cef-d7f2-4772-b7db-bd097454984c */
#define NS_MSGSERVICEPROVIDERSERVICE_CID \
{ 0x10998cef, 0xd7f2, 0x4772, \
  { 0xb7, 0xdb, 0xbd, 0x09, 0x74, 0x54, 0x98, 0x4c}}

#define NS_MSGLOGONREDIRECTORSERVICE_CONTRACTID \
	"@mozilla.org/messenger/msglogonredirector;1"

#define NS_MSGLOGONREDIRECTORSERVICE_CID \
{0x0d7456ae, 0xe28a, 0x11d3, \
  {0xa5, 0x60, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7}}

//
// nsSubscribableServer
//
#define NS_SUBSCRIBABLESERVER_CONTRACTID \
 "@mozilla.org/messenger/subscribableserver;1"

#define NS_SUBSCRIBABLESERVER_CID \
{0x8510876a, 0x1dd2, 0x11b2, \
  {0x82, 0x53, 0x91, 0xf7, 0x1b, 0x34, 0x8a, 0x25}}

//
// nsSubscribeDataSource
//
#define NS_SUBSCRIBEDATASOURCE_CONTRACTID \
  NS_RDF_DATASOURCE_CONTRACTID_PREFIX "subscribe"

/* 00e89c82-1dd2-11b2-9a1c-e75995d7d595 */
#define NS_SUBSCRIBEDATASOURCE_CID \
{ 0x00e89c82, 0x1dd2, 0x11b2, \
  { 0x9a, 0x1c, 0xe7, 0x59, 0x95, 0xd7, 0xd5, 0x95}} 

//
// delegate factory
//

/* c6584cee-8ee8-4b2c-8dbe-7dfcb55c9c61 */
#define NS_MSGFILTERDELEGATEFACTORY_CID \
  {0xc6584cee, 0x8ee8, 0x4b2c, \
    { 0x8d, 0xbe, 0x7d, 0xfc, 0xb5, 0x5c, 0x9c, 0x61 }}

#define NS_MSGFILTERDELEGATEFACTORY_CONTRACTID_PREFIX \
  NS_RDF_DELEGATEFACTORY_CONTRACTID_PREFIX "filter" "&scheme="   

// Note: the above CID should live in base, but each protocol
// should be creating the ContractID themselves. for now we'll
// do it for news/imap/local mail

#define NS_MSGFILTERDELEGATEFACTORY_MAILBOX_CONTRACTID \
  NS_MSGFILTERDELEGATEFACTORY_CONTRACTID_PREFIX "mailbox"

#define NS_MSGFILTERDELEGATEFACTORY_NEWS_CONTRACTID \
  NS_MSGFILTERDELEGATEFACTORY_CONTRACTID_PREFIX "news"

#define NS_MSGFILTERDELEGATEFACTORY_IMAP_CONTRACTID \
  NS_MSGFILTERDELEGATEFACTORY_CONTRACTID_PREFIX "imap"

//
// nsMsgFilterDataSource
//
#define NS_MSGFILTERDATASOURCE_CONTRACTID \
  NS_RDF_DATASOURCE_CONTRACTID_PREFIX "msgfilters"

/* d97edfb5-bcbe-4a15-a4fb-fbf2f958b388 */
#define NS_MSGFILTERDATASOURCE_CID \
  {0xd97edfb5, 0xbcbe, 0x4a15, \
    { 0xa4, 0xfb, 0xfb, 0xf2, 0xf9, 0x58, 0xb3, 0x88 }}

#define NS_MSGLOCALFOLDERCOMPACTOR_CONTRACTID \
  "@mozilla.org/messenger/localfoldercompactor;1"

/* 7d1d315c-e5c6-11d4-a5b7-0060b0fc04b7 */
#define NS_MSGLOCALFOLDERCOMPACTOR_CID \
  {0x7d1d315c, 0xe5c6, 0x11d4, \
    {0xa5, 0xb7, 0x00,0x60, 0xb0, 0xfc, 0x04, 0xb7 }}

#define NS_MSGOFFLINESTORECOMPACTOR_CONTRACTID \
  "@mozilla.org/messenger/offlinestorecompactor;1"

/* 2db43d16-e5c8-11d4-a5b7-0060b0fc04b7 */
#define NS_MSG_OFFLINESTORECOMPACTOR_CID \
  {0x2db43d16, 0xe5c8, 0x11d4, \
    {0xa5, 0xb7, 0x00,0x60, 0xb0, 0xfc, 0x04, 0xb7 }}

//
// nsMsgDBView
//
#define NS_MSGDBVIEW_CONTRACTID_PREFIX \
  "@mozilla.org/messenger/msgdbview;1?type="

#define NS_MSGTHREADEDDBVIEW_CONTRACTID \
  NS_MSGDBVIEW_CONTRACTID_PREFIX "threaded"

#define NS_MSGTHREADSWITHUNREADDBVIEW_CONTRACTID \
  NS_MSGDBVIEW_CONTRACTID_PREFIX "threadswithunread"

#define NS_MSGWATCHEDTHREADSWITHUNREADDBVIEW_CONTRACTID \
  NS_MSGDBVIEW_CONTRACTID_PREFIX "watchedthreadswithunread"

#define NS_MSGSEARCHDBVIEW_CONTRACTID \
  NS_MSGDBVIEW_CONTRACTID_PREFIX "search"

#define NS_MSGQUICKSEARCHDBVIEW_CONTRACTID \
  NS_MSGDBVIEW_CONTRACTID_PREFIX "quicksearch"

#define NS_MSGXFVFDBVIEW_CONTRACTID \
  NS_MSGDBVIEW_CONTRACTID_PREFIX "xfvf"

#define NS_MSGGROUPDBVIEW_CONTRACTID \
  NS_MSGDBVIEW_CONTRACTID_PREFIX "group"

/* 52f860e0-1dd2-11b2-aa72-bb751981bd00 */
#define NS_MSGTHREADEDDBVIEW_CID \
  {0x52f860e0, 0x1dd2, 0x11b2, \
    {0xaa, 0x72, 0xbb, 0x75, 0x19, 0x81, 0xbd, 0x00 }}

/* ca79a00e-010d-11d5-a5be-0060b0fc04b7 */
#define NS_MSGTHREADSWITHUNREADDBVIEW_CID \
  {0xca79a00e, 0x010d, 0x11d5, \
    {0xa5, 0xbe, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 }}

/* 597e1ffe-0123-11d5-a5be-0060b0fc04b7 */
#define NS_MSGWATCHEDTHREADSWITHUNREADDBVIEW_CID \
  {0x597e1ffe, 0x0123, 0x11d5, \
    {0xa5, 0xbe, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 }}

/* aeac118c-0823-11d5-a5bf-0060b0fc04b7 */
#define NS_MSGSEARCHDBVIEW_CID \
  {0xaeac118c, 0x0823, 0x11d5, \
    {0xa5, 0xbf, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7}}

/* 2dd9d0fe-b609-11d6-bacc-00108335748d */
#define NS_MSGQUICKSEARCHDBVIEW_CID \
  {0x2dd9d0fe, 0xb609, 0x11d6, \
    {0xba, 0xcc, 0x00, 0x10, 0x83, 0x35, 0x74, 0x8d}}

/* 2af6e050-04f6-495a-8387-86b0aeb1863c */
#define NS_MSG_XFVFDBVIEW_CID \
  {0x2af6e050, 0x04f6, 0x495a, \
    {0x83, 0x87, 0x86, 0xb0, 0xae, 0xb1, 0x86, 0x3c}}

/* e4603d6c-0a74-47c5-b69e-2f8876990304 */
#define NS_MSG_GROUPDBVIEW_CID \
  {0xe4603d6c, 0x0a74, 0x47c5, \
    {0xb6, 0x9e, 0x2f, 0x88, 0x76, 0x99, 0x03, 0x04}}
//
// nsMsgAccountManager
// 
#define NS_MSGOFFLINEMANAGER_CONTRACTID \
  "@mozilla.org/messenger/offline-manager;1"

#define NS_MSGOFFLINEMANAGER_CID									\
{ /* ac6c518a-09b2-11d5-a5bf-0060b0fc04b7 */			\
 0xac6c518a, 0x09b2, 0x11d5,											\
 {0xa5, 0xbf, 0x0, 0x60, 0xb0, 0xfc, 0x04, 0xb7 }}


//
// nsMsgProgress
// 
#define NS_MSGPROGRESS_CONTRACTID \
  "@mozilla.org/messenger/progress;1"

#define NS_MSGPROGRESS_CID									      \
{ /* 9f4dd201-3b1f-11d5-9daa-c345c9453d3c */			\
 0x9f4dd201, 0x3b1f, 0x11d5,											\
 {0x9d, 0xaa, 0xc3, 0x45, 0xc9, 0x45, 0x3d, 0x3c }}

//
// nsSpamSettings
// 
#define NS_SPAMSETTINGS_CONTRACTID \
  "@mozilla.org/messenger/spamsettings;1"

#define NS_SPAMSETTINGS_CID									      \
{ /* ce6038ae-e5e0-4372-9cff-2a6633333b2b */			\
 0xce6038ae, 0xe5e0, 0x4372,											\
 {0x9c, 0xff, 0x2a, 0x66, 0x33, 0x33, 0x3b, 0x2b }}

// 
// nsMessengerOSIntegration 
//
#define NS_MESSENGEROSINTEGRATION_CONTRACTID \
  "@mozilla.org/messenger/osintegration;1"

//
// cid protocol handler
//
#define NS_CIDPROTOCOLHANDLER_CONTRACTID \
 NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "cid"

#define NS_CIDPROTOCOL_CID \
{ /* b3db9392-1b15-48ba-a136-0cc3db13d87b */ \
 0xb3db9392, 0x1b15, 0x48ba,      \
 {0xa1, 0x36, 0x0c, 0xc3, 0xdb, 0x13, 0xd8, 0x7b }}

//
// nsMessengerContentHandler
//
#define NS_MESSENGERCONTENTHANDLER_CID          \
{ /* 57E1BCBB-1FBA-47e7-B96B-F59E392473B0 */    \
 0x57e1bcbb, 0x1fba, 0x47e7,                    \
 {0xb9, 0x6b, 0xf5, 0x9e, 0x39, 0x24, 0x73, 0xb0}}

#define NS_MESSENGERCONTENTHANDLER_CONTRACTID   \
  NS_CONTENT_HANDLER_CONTRACTID_PREFIX "x-message-display"

#endif // nsMessageBaseCID_h__
