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
 * Copyright (C) 1998,1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIGenericFactory.h"
#include "nsIModule.h"

#include "nsAbBaseCID.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "rdf.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"


/* Include all of the interfaces our factory can generate components for */

#include "nsDirectoryDataSource.h"
#include "nsCardDataSource.h"
#include "nsAbDirectory.h"
#include "nsAbCard.h"
#include "nsAddrDatabase.h"
#include "nsAddressBook.h"
#include "nsAddrBookSession.h"
#include "nsAbDirProperty.h"
#include "nsAbAutoCompleteSession.h"
#include "nsAbAddressCollecter.h"
#include "nsAddbookProtocolHandler.h"
#include "nsAddbookUrl.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddressBook)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAbDirectoryDataSource,Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbDirectory)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAbCardDataSource,Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbCard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbCardProperty)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddrDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbDirProperty)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddrBookSession)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbAutoCompleteSession)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbAddressCollecter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddbookUrl)

//NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddbookProtocolHandler)

static nsModuleComponentInfo components[] =
{
  { "Address Book",
    NS_ADDRESSBOOK_CID,
    NS_ADDRESSBOOK_CONTRACTID,
    nsAddressBookConstructor },
  { "Address Book Startup Handler",
    NS_ADDRESSBOOK_CID,
    NS_ADDRESSBOOKSTARTUPHANDLER_CONTRACTID,
    nsAddressBookConstructor,
    nsAddressBook::RegisterProc,
    nsAddressBook::UnregisterProc },
  { "Address Book Directory Datasource",
    NS_ABDIRECTORYDATASOURCE_CID,
    NS_ABDIRECTORYDATASOURCE_CONTRACTID,
    nsAbDirectoryDataSourceConstructor },
  { "Address Book Directory",
    NS_ABDIRECTORY_CID,
    NS_ABDIRECTORY_CONTRACTID,
    nsAbDirectoryConstructor },
  { "Address Book Card Datasource",
    NS_ABCARDDATASOURCE_CID,
    NS_ABCARDDATASOURCE_CONTRACTID,
    nsAbCardDataSourceConstructor },
  { "Address Book Card",
    NS_ABCARD_CID,
    NS_ABCARD_CONTRACTID,
    nsAbCardConstructor },
  { "Address Database",
    NS_ADDRDATABASE_CID,
    NS_ADDRDATABASE_CONTRACTID,
    nsAddrDatabaseConstructor },
  { "Address Book Card Property",
    NS_ABCARDPROPERTY_CID,
    NS_ABCARDPROPERTY_CONTRACTID,
    nsAbCardPropertyConstructor },
  { "Address Book Directory Property",
    NS_ABDIRPROPERTY_CID,
    NS_ABDIRPROPERTY_CONTRACTID,
    nsAbDirPropertyConstructor },
  { "Address Book Session",
    NS_ADDRBOOKSESSION_CID,
    NS_ADDRBOOKSESSION_CONTRACTID,
    nsAddrBookSessionConstructor },
  { "Address Book Auto Complete Session",
    NS_ABAUTOCOMPLETESESSION_CID,
    NS_ABAUTOCOMPLETESESSION_CONTRACTID,
    nsAbAutoCompleteSessionConstructor },
  { "Address Book Address Collector",
    NS_ABADDRESSCOLLECTER_CID,
    NS_ABADDRESSCOLLECTER_CONTRACTID,
    nsAbAddressCollecterConstructor },
  { "The addbook URL Interface", 
    NS_ADDBOOKURL_CID,
    NS_ADDBOOKURL_CONTRACTID,
    nsAddbookUrlConstructor },
  { "The addbook Protocol Handler", 
    NS_ADDBOOK_HANDLER_CID,
    NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "addbook",
    nsAddbookProtocolHandler::Create }
};

NS_IMPL_NSGETMODULE("nsAbModule", components)
