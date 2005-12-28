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

#ifndef nsAbBaseCID_h__
#define nsAbBaseCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsAbDirFactoryService.h"

//
// nsAddressBook
//
#define NS_ADDRESSBOOK_CONTRACTID \
  "@mozilla.org/addressbook;1"

#define NS_ADDRESSBOOKSTARTUPHANDLER_CONTRACTID \
  "@mozilla.org/commandlinehandler/general-startup;1?type=addressbook"

#define NS_ADDRESSBOOK_CID							\
{ /* {D60B84F2-2A8C-11d3-9E07-00A0C92B5F0D} */		\
  0xd60b84f2, 0x2a8c, 0x11d3,						\
	{ 0x9e, 0x7, 0x0, 0xa0, 0xc9, 0x2b, 0x5f, 0xd }	\
}



//
// nsAbDirectoryDataSource
//
#define NS_ABDIRECTORYDATASOURCE_CONTRACTID \
  NS_RDF_DATASOURCE_CONTRACTID_PREFIX "addressdirectory"

#define NS_ABDIRECTORYDATASOURCE_CID			\
{ /* 0A79186D-F754-11d2-A2DA-001083003D0C */		\
    0xa79186d, 0xf754, 0x11d2,				\
    {0xa2, 0xda, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

//
// nsAbBSDirectory
//
#define NS_ABDIRECTORY_CONTRACTID \
  NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX "moz-abdirectory"

#define NS_ABDIRECTORY_CID             	 			\
{ /* {012D3C24-1DD2-11B2-BA79-B4AD359FC461}*/			\
    	0x012D3C24, 0x1DD2, 0x11B2,				\
	{0xBA, 0x79, 0xB4, 0xAD, 0x35, 0x9F, 0xC4, 0x61}	\
}


//
// nsAbMDBDirectory
//
#define NS_ABMDBDIRECTORY_CONTRACTID \
  NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX "moz-abmdbdirectory"

#define NS_ABMDBDIRECTORY_CID             		\
{ /* {e618f894-1dd1-11b2-889c-9aaefaa90dde}*/		\
    0xe618f894, 0x1dd1, 0x11b2,				\
    {0x88, 0x9c, 0x9a, 0xae, 0xfa, 0xa9, 0x0d, 0xde}	\
}

//
// nsAbMDBCard
//
#define NS_ABMDBCARD_CONTRACTID \
  "@mozilla.org/addressbook/moz-abmdbcard;1"

#define NS_ABMDBCARD_CID				\
{ /* {f578a5d2-1dd1-11b2-8841-f45cc5e765f8} */		\
    0xf578a5d2, 0x1dd1, 0x11b2,				\
    {0x88, 0x41, 0xf4, 0x5c, 0xc5, 0xe7, 0x65, 0xf8}	\
}



//
// nsAddressBookDB
//
#define NS_ADDRDATABASE_CONTRACTID \
  "@mozilla.org/addressbook/carddatabase;1"

#define NS_ADDRDATABASE_CID						\
{ /* 63187917-1D19-11d3-A302-001083003D0C */		\
    0x63187917, 0x1d19, 0x11d3,						\
    {0xa3, 0x2, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

//
// nsAbCardProperty
//
#define NS_ABCARDPROPERTY_CONTRACTID \
  "@mozilla.org/addressbook/cardproperty;1"
#define NS_ABCARDPROPERTY_CID						\
{ /* 2B722171-2CEA-11d3-9E0B-00A0C92B5F0D */		\
    0x2b722171, 0x2cea, 0x11d3,						\
    {0x9e, 0xb, 0x0, 0xa0, 0xc9, 0x2b, 0x5f, 0xd}	\
}

//
// nsAddrBookSession
//
#define NS_ADDRBOOKSESSION_CONTRACTID \
  "@mozilla.org/addressbook/services/session;1"

#define NS_ADDRBOOKSESSION_CID						\
{ /* C5339442-303F-11d3-9E13-00A0C92B5F0D */		\
    0xc5339442, 0x303f, 0x11d3,						\
    {0x9e, 0x13, 0x0, 0xa0, 0xc9, 0x2b, 0x5f, 0xd}	\
}

//
// nsAbDirProperty
//
#define NS_ABDIRPROPERTY_CONTRACTID \
  "@mozilla.org/addressbook/directoryproperty;1"
#define NS_ABDIRPROPERTY_CID						\
{ /* 6FD8EC67-3965-11d3-A316-001083003D0C */		\
    0x6fd8ec67, 0x3965, 0x11d3,						\
    {0xa3, 0x16, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

//
// nsAbDirectoryProperties
//
#define NS_ABDIRECTORYPROPERTIES_CONTRACTID \
  "@mozilla.org/addressbook/properties;1"
#define NS_ABDIRECTORYPROPERTIES_CID						\
{ /* 8b00a972-1dd2-11b2-9d9c-9c377a9c3dba */		\
    0x8b00a972, 0x1dd2, 0x11b2, \
    {0x9d, 0x9c, 0x9c, 0x37, 0x7a, 0x9c, 0x3d, 0xba} \
}

//
// nsAbAutoCompleteSession
//
#define NS_ABAUTOCOMPLETESESSION_CONTRACTID \
  "@mozilla.org/autocompleteSession;1?type=addrbook"
#define NS_ABAUTOCOMPLETESESSION_CID				\
{ /* 138DE9BD-362B-11d3-988E-001083010E9B */		\
    0x138de9bd, 0x362b, 0x11d3,						\
    {0x98, 0x8e, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b}	\
}

//
// nsAbAddressCollecter
//
#define NS_ABADDRESSCOLLECTER_CONTRACTID \
  "@mozilla.org/addressbook/services/addressCollecter;1"
#define NS_ABADDRESSCOLLECTER_CID \
{	/* fe04c8e6-501e-11d3-a527-0060b0fc04b7 */		\
	0xfe04c8e6, 0x501e, 0x11d3,						\
	{0xa5, 0x27, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xb7}	\
}

#define NS_AB4xUPGRADER_CONTRACTID \
	"@mozilla.org/addressbook/services/4xUpgrader;1"
#define NS_AB4xUPGRADER_CID \
{	/* 0a6ae8e6-f550-11d3-a563-0060b0fc04b7 */		\
	0x0a6ae8e6, 0xf550, 0x11d3,						\
	{0xa5, 0x63, 0x00, 0x60, 0xb0, 0xfc, 0x4, 0xb7}	\
}

//
// addbook URL
//
#define NS_ADDBOOKURL_CONTRACTID \
  "@mozilla.org/addressbook/services/url;1?type=addbook"

#define NS_ADDBOOKURL_CID \
{	/* ff04c8e6-501e-11d3-a527-0060b0fc0444 */		\
	0xff04c8e6, 0x501e, 0x11d3,						\
	{0xa5, 0x27, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0x44}	\
}

//
// addbook Protocol Handler
//
#define NS_ADDBOOK_HANDLER_CONTRACTID \
  "@mozilla.org/addressbook/services/addbook;1"
#define NS_ADDBOOK_HANDLER_CID \
{	/* ff04c8e6-501e-11d3-ffcc-0060b0fc0444 */		\
	0xff04c8e6, 0x501e, 0x11d3,						\
	{0xff, 0xcc, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0x44}	\
}

//
// directory factory service
//
#define NS_ABDIRFACTORYSERVICE_CONTRACTID \
  "@mozilla.org/addressbook/directory-factory-service;1"

#define NS_ABDIRFACTORYSERVICE_CID				\
{ /* {F8B212F2-742B-4A48-B7A0-4C44D4DDB121}*/			\
	0xF8B212F2, 0x742B, 0x4A48,				\
	{0xB7, 0xA0, 0x4C, 0x44, 0xD4, 0xDD, 0xB1, 0x21}	\
}

//
// mdb directory factory
//
#define NS_ABMDBDIRFACTORY_CONTRACTID \
  NS_AB_DIRECTORY_FACTORY_CONTRACTID_PREFIX "moz-abmdbdirectory"

#define NS_ABMDBDIRFACTORY_CID				\
{ /* {E1CB9C8A-722D-43E4-9D7B-7CCAE4B0338A}*/			\
	0xE1CB9C8A, 0x722D, 0x43E4,				\
	{0x9D, 0x7B, 0x7C, 0xCA, 0xE4, 0xB0, 0x33, 0x8A}	\
}

#ifdef XP_WIN
//
// nsAbOutlookDirectory
//
#define NS_ABOUTLOOKDIRECTORY_CONTRACTID \
  NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX "moz-aboutlookdirectory"

#define NS_ABOUTLOOKDIRECTORY_CID                       \
{ /* {9cc57822-0599-4c47-a399-1c6fa185a05c}*/           \
        0x9cc57822, 0x0599, 0x4c47,                     \
        {0xa3, 0x99, 0x1c, 0x6f, 0xa1, 0x85, 0xa0, 0x5c}        \
}

//
// nsAbOutlookCard
//
#define NS_ABOUTLOOKCARD_CONTRACTID \
  "@mozilla.org/addressbook/moz-aboutlookcard"

#define NS_ABOUTLOOKCARD_CID                                    \
{ /* {32cf9734-4ee8-4f5d-acfc-71b75eee1819}*/           \
        0x32cf9734, 0x4ee8, 0x4f5d,                     \
        {0xac, 0xfc, 0x71, 0xb7, 0x5e, 0xee, 0x18, 0x19}        \
}

//
// Outlook directory factory
//
#define NS_ABOUTLOOKDIRFACTORY_CONTRACTID \
  NS_AB_DIRECTORY_FACTORY_CONTRACTID_PREFIX "moz-aboutlookdirectory"

#define NS_ABOUTLOOKDIRFACTORY_CID                                \
{ /* {558ccc0f-2681-4dac-a066-debd8d26faf6}*/                   \
        0x558ccc0f, 0x2681, 0x4dac,                             \
        {0xa0, 0x66, 0xde, 0xbd, 0x8d, 0x26, 0xfa, 0xf6}        \
}
#endif

//
//  Addressbook Query support
//

#define NS_ABDIRECTORYQUERYARGUMENTS_CONTRACTID \
  "@mozilla.org/addressbook/directory/query-arguments;1"

#define NS_ABDIRECTORYQUERYARGUMENTS_CID                          \
{ /* {f7dc2aeb-8e62-4750-965c-24b9e09ed8d2} */        \
  0xf7dc2aeb, 0x8e62, 0x4750,                     \
  { 0x96, 0x5c, 0x24, 0xb9, 0xe0, 0x9e, 0xd8, 0xd2 }  \
}


#define NS_BOOLEANCONDITIONSTRING_CONTRACTID \
  "@mozilla.org/boolean-expression/condition-string;1"

#define NS_BOOLEANCONDITIONSTRING_CID                         \
{ /* {ca1944a9-527e-4c77-895d-d0466dd41cf5} */        \
  0xca1944a9, 0x527e, 0x4c77, \
    { 0x89, 0x5d, 0xd0, 0x46, 0x6d, 0xd4, 0x1c, 0xf5 } \
}


#define NS_BOOLEANEXPRESSION_CONTRACTID \
  "@mozilla.org/boolean-expression/n-peer;1"

#define NS_BOOLEANEXPRESSION_CID                         \
{ /* {2c2e75c8-6f56-4a50-af1c-72af5d0e8d41} */        \
  0x2c2e75c8, 0x6f56, 0x4a50, \
    { 0xaf, 0x1c, 0x72, 0xaf, 0x5d, 0x0e, 0x8d, 0x41 } \
}

#define NS_ABDIRECTORYQUERYPROXY_CONTRACTID                     \
        "@mozilla.org/addressbook/directory-query/proxy;1"      

#define NS_ABDIRECTORYQUERYPROXY_CID                            \
{ /* {E162E335-541B-43B4-AAEA-FE591E240CAF}*/                   \
        0xE162E335, 0x541B, 0x43B4,                             \
        {0xAA, 0xEA, 0xFE, 0x59, 0x1E, 0x24, 0x0C, 0xAF}        \
}

// nsAbLDAPDirectory
//
#define NS_ABLDAPDIRECTORY_CONTRACTID \
  NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX "moz-abldapdirectory"

#define NS_ABLDAPDIRECTORY_CID             			\
{ /* {783E2777-66D7-4826-9E4B-8AB58C228A52}*/			\
	0x783E2777, 0x66D7, 0x4826,				\
	{0x9E, 0x4B, 0x8A, 0xB5, 0x8C, 0x22, 0x8A, 0x52}	\
}

//
// nsAbLDAPCard
//
#define NS_ABLDAPCARD_CONTRACTID \
  "@mozilla.org/addressbook/moz-abldapcard"

#define NS_ABLDAPCARD_CID					\
{ /* {10307B01-EBD6-465F-B972-1630410F70E6}*/			\
	0x10307B01, 0xEBD6, 0x465F,				\
	{0xB9, 0x72, 0x16, 0x30, 0x41, 0x0F, 0x70, 0xE6}	\
}

//
// LDAP directory factory
//
#define NS_ABLDAPDIRFACTORY_CONTRACTID \
  NS_AB_DIRECTORY_FACTORY_CONTRACTID_PREFIX "moz-abldapdirectory"

#define NS_ABLDAPDIRFACTORY_CID                                \
{  /* {8e3701af-8828-426c-84ac-124825c778f8} */			\
        0x8e3701af, 0x8828, 0x426c,                             \
        {0x84, 0xac, 0x12, 0x48, 0x25, 0xc7, 0x78, 0xf8}        \
}

//
// LDAP autcomplete directory factory
//
#define NS_ABLDAPACDIRFACTORY_CONTRACTID \
  NS_AB_DIRECTORY_FACTORY_CONTRACTID_PREFIX "ldap"
#define NS_ABLDAPSACDIRFACTORY_CONTRACTID \
  NS_AB_DIRECTORY_FACTORY_CONTRACTID_PREFIX "ldaps"

// nsAbLDAPAutoCompFormatter

// 4e276d6d-9981-46b4-9070-92f344ac5f5a
//
#define NS_ABLDAPAUTOCOMPFORMATTER_CID \
{ 0x4e276d6d, 0x9981, 0x46b4, \
 { 0x90, 0x70, 0x92, 0xf3, 0x44, 0xac, 0x5f, 0x5a }}

#define NS_ABLDAPAUTOCOMPFORMATTER_CONTRACTID \
 "@mozilla.org/ldap-autocomplete-formatter;1?type=addrbook"


// nsAbLDAPReplicationService
//
// {ece81280-2639-11d6-b791-00b0d06e5f27}
//
#define NS_ABLDAP_REPLICATIONSERVICE_CID \
  {0xece81280, 0x2639, 0x11d6, \
    { 0xb7, 0x91, 0x00, 0xb0, 0xd0, 0x6e, 0x5f, 0x27 }}

#define NS_ABLDAP_REPLICATIONSERVICE_CONTRACTID \
 "@mozilla.org/addressbook/ldap-replication-service;1"

// nsAbLDAPReplicationQuery
//
// {5414fff0-263b-11d6-b791-00b0d06e5f27}
//
#define NS_ABLDAP_REPLICATIONQUERY_CID \
  {0x5414fff0, 0x263b, 0x11d6, \
    { 0xb7, 0x91, 0x00, 0xb0, 0xd0, 0x6e, 0x5f, 0x27 }}

#define NS_ABLDAP_REPLICATIONQUERY_CONTRACTID \
 "@mozilla.org/addressbook/ldap-replication-query;1"


// nsAbLDAPChangeLogQuery
//
// {63E11D51-3C9B-11d6-B7B9-00B0D06E5F27}
#define NS_ABLDAP_CHANGELOGQUERY_CID \
  {0x63e11d51, 0x3c9b, 0x11d6, \
    { 0xb7, 0xb9, 0x0, 0xb0, 0xd0, 0x6e, 0x5f, 0x27 }}

#define NS_ABLDAP_CHANGELOGQUERY_CONTRACTID \
 "@mozilla.org/addressbook/ldap-changelog-query;1"

// nsAbLDAPProcessReplicationData
//
// {5414fff1-263b-11d6-b791-00b0d06e5f27}
//
#define NS_ABLDAP_PROCESSREPLICATIONDATA_CID \
  {0x5414fff1, 0x263b, 0x11d6, \
    { 0xb7, 0x91, 0x00, 0xb0, 0xd0, 0x6e, 0x5f, 0x27 }}

#define NS_ABLDAP_PROCESSREPLICATIONDATA_CONTRACTID \
 "@mozilla.org/addressbook/ldap-process-replication-data;1"


// nsAbLDAPProcessChangeLogData
//
// {63E11D52-3C9B-11d6-B7B9-00B0D06E5F27}
#define NS_ABLDAP_PROCESSCHANGELOGDATA_CID \
  {0x63e11d52, 0x3c9b, 0x11d6, \
    {0xb7, 0xb9, 0x0, 0xb0, 0xd0, 0x6e, 0x5f, 0x27 }}

#define NS_ABLDAP_PROCESSCHANGELOGDATA_CONTRACTID \
 "@mozilla.org/addressbook/ldap-process-changelog-data;1"

// nsABView

#define NS_ABVIEW_CID \
{ 0xc5eb5d6a, 0x1dd1, 0x11b2, \
 { 0xa0, 0x25, 0x94, 0xd1, 0x18, 0x1f, 0xc5, 0x9c }}

#define NS_ABVIEW_CONTRACTID \
 "@mozilla.org/addressbook/abview;1"

#define NS_MSGVCARDSERVICE_CID \
{ 0x3c4ac0da, 0x2cda, 0x4018, \
 { 0x95, 0x51, 0xe1, 0x58, 0xb2, 0xe1, 0x22, 0xd3 }}

#define NS_MSGVCARDSERVICE_CONTRACTID \
 "@mozilla.org/addressbook/msgvcardservice;1"

#define NS_MSGVCARDSTREAMLISTENER_CID \
{ 0xf4045da, 0x6187, 0x42ff, \
 { 0x9d, 0xf4, 0x80, 0x65, 0x44, 0xf, 0x76, 0x21 }}

#define NS_MSGVCARDSTREAMLISTENER_CONTRACTID \
 "@mozilla.org/addressbook/msgvcardstreamlistener;1"

#endif // nsAbBaseCID_h__
