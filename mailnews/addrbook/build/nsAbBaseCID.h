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

#ifndef nsAbBaseCID_h__
#define nsAbBaseCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

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

#define NS_ABDIRECTORYDATASOURCE_CID				\
{ /* 0A79186D-F754-11d2-A2DA-001083003D0C */		\
    0xa79186d, 0xf754, 0x11d2,						\
    {0xa2, 0xda, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

//
// nsAbDirectory
//
#define NS_ABDIRECTORY_CONTRACTID \
  NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX "abdirectory"

#define NS_ABDIRECTORY_CID                  \
{ /* {6C21831D-FCC2-11d2-A2E2-001083003D0C}*/		\
	0x6c21831d, 0xfcc2, 0x11d2,						\
	{0xa2, 0xe2, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

//
// nsAbCardDataSource
//
#define NS_ABCARDDATASOURCE_CONTRACTID \
  NS_RDF_DATASOURCE_CONTRACTID_PREFIX "addresscard"

#define NS_ABCARDDATASOURCE_CID						\
{ /* 1920E486-0709-11d3-A2EC-001083003D0C */		\
    0x1920e486, 0x709, 0x11d3,						\
    {0xa2, 0xec, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

//
// nsAbCard
//
#define NS_ABCARD_CONTRACTID \
  NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX "abcard"

#define NS_ABCARD_CID						\
{ /* {1920E487-0709-11d3-A2EC-001083003D0C}*/		\
	0x1920e487, 0x709, 0x11d3,						\
	{0xa2, 0xec, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
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
  "@mozilla.org/addressbook/services/addbookurl;1"
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

#endif // nsAbBaseCID_h__
