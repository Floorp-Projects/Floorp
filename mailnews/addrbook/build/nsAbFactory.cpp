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

#include "nsDirectoryDataSource.h"
#include "nsAbBSDirectory.h"
#include "nsAbMDBDirectory.h"
#include "nsAbMDBCard.h"
#include "nsAbDirFactoryService.h"
#include "nsAbMDBDirFactory.h"
#include "nsAddrDatabase.h"
#include "nsAddressBook.h"
#include "nsAddrBookSession.h"
#include "nsAbDirProperty.h"
#include "nsAbAutoCompleteSession.h"
#include "nsAbAddressCollecter.h"
#include "nsAddbookProtocolHandler.h"
#include "nsAddbookUrl.h"
#include "nsCURILoader.h"

#if defined(XP_WIN) && !defined(__MINGW32__)
#include "nsAbOutlookDirectory.h"
#include "nsAbOutlookCard.h"
#include "nsAbOutlookDirFactory.h"
#endif

#include "nsAbDirectoryQuery.h"
#include "nsAbBooleanExpression.h"
#include "nsAbDirectoryQueryProxy.h"
#include "nsAbView.h"
#include "nsMsgVCardService.h"

#if defined(MOZ_LDAP_XPCOM)
#include "nsAbLDAPDirectory.h"
#include "nsAbLDAPCard.h"
#include "nsAbLDAPDirFactory.h"
#include "nsAbLDAPAutoCompFormatter.h"
#include "nsAbLDAPReplicationService.h"
#include "nsAbLDAPReplicationQuery.h"
#include "nsAbLDAPReplicationData.h"
#include "nsAbLDAPChangeLogQuery.h"
#include "nsAbLDAPChangeLogData.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddressBook)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAbDirectoryDataSource,Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbDirProperty)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbDirectoryProperties)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbCardProperty)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbBSDirectory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbMDBDirectory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbMDBCard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddrDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddrBookSession)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbAutoCompleteSession)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAbAddressCollecter,Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddbookUrl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbDirFactoryService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbMDBDirFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddbookProtocolHandler)

#if defined(XP_WIN) && !defined(__MINGW32__)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbOutlookDirectory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbOutlookCard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbOutlookDirFactory)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbDirectoryQueryArguments)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbBooleanConditionString)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbBooleanExpression)

#if defined(MOZ_LDAP_XPCOM)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPDirectory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPCard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPDirFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPAutoCompFormatter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPReplicationService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPReplicationQuery)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPProcessReplicationData)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPChangeLogQuery)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPProcessChangeLogData)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbDirectoryQueryProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgVCardService)

static const nsModuleComponentInfo components[] =
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

  { "Address Boot Strap Directory",
    NS_ABDIRECTORY_CID,
    NS_ABDIRECTORY_CONTRACTID,
    nsAbBSDirectoryConstructor },

  { "Address MDB Book Directory",
    NS_ABMDBDIRECTORY_CID,
    NS_ABMDBDIRECTORY_CONTRACTID,
    nsAbMDBDirectoryConstructor },

  { "Address MDB Book Card",
    NS_ABMDBCARD_CID,
    NS_ABMDBCARD_CONTRACTID,
    nsAbMDBCardConstructor },

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
  { "AB Directory Properties",
    NS_ABDIRECTORYPROPERTIES_CID,
    NS_ABDIRECTORYPROPERTIES_CONTRACTID,
    nsAbDirectoryPropertiesConstructor },
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
    nsAddbookProtocolHandlerConstructor },

  { "add vCard content handler",
    NS_ADDRESSBOOK_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"x-application-addvcard",
    nsAddressBookConstructor },

  { "add vCard content handler",
    NS_ADDRESSBOOK_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"text/x-vcard",
    nsAddressBookConstructor },

  { "The directory factory service interface",
    NS_ABDIRFACTORYSERVICE_CID,
    NS_ABDIRFACTORYSERVICE_CONTRACTID,
    nsAbDirFactoryServiceConstructor },

  { "The MDB directory factory interface",
    NS_ABMDBDIRFACTORY_CID,
    NS_ABMDBDIRFACTORY_CONTRACTID,
    nsAbMDBDirFactoryConstructor },

#if defined(XP_WIN) && !defined(__MINGW32__)
  { "Address OUTLOOK Book Directory",
    NS_ABOUTLOOKDIRECTORY_CID,
    NS_ABOUTLOOKDIRECTORY_CONTRACTID,
    nsAbOutlookDirectoryConstructor },

  { "Address OUTLOOK Book Card",
    NS_ABOUTLOOKCARD_CID,
    NS_ABOUTLOOKCARD_CONTRACTID,
    nsAbOutlookCardConstructor },

  { "The outlook factory Interface", 
    NS_ABOUTLOOKDIRFACTORY_CID,
    NS_ABOUTLOOKDIRFACTORY_CONTRACTID,
    nsAbOutlookDirFactoryConstructor },
#endif

  { "The addbook query arguments", 
    NS_ABDIRECTORYQUERYARGUMENTS_CID,
    NS_ABDIRECTORYQUERYARGUMENTS_CONTRACTID,
    nsAbDirectoryQueryArgumentsConstructor },
    
  { "The query boolean condition string", 
    NS_BOOLEANCONDITIONSTRING_CID,
    NS_BOOLEANCONDITIONSTRING_CONTRACTID,
    nsAbBooleanConditionStringConstructor },
    
  { "The query n-peer expression", 
    NS_BOOLEANEXPRESSION_CID,
    NS_BOOLEANEXPRESSION_CONTRACTID,
    nsAbBooleanExpressionConstructor },

#if defined(MOZ_LDAP_XPCOM)
  { "Address LDAP Book Directory",
    NS_ABLDAPDIRECTORY_CID,
    NS_ABLDAPDIRECTORY_CONTRACTID,
    nsAbLDAPDirectoryConstructor },

  { "Address LDAP Book Card",
    NS_ABLDAPCARD_CID,
    NS_ABLDAPCARD_CONTRACTID,
    nsAbLDAPCardConstructor },

  { "Address LDAP factory Interface", 
    NS_ABLDAPDIRFACTORY_CID,
    NS_ABLDAPDIRFACTORY_CONTRACTID,
    nsAbLDAPDirFactoryConstructor },

 {"Address LDAP Replication Service Interface",
   NS_ABLDAP_REPLICATIONSERVICE_CID,
   NS_ABLDAP_REPLICATIONSERVICE_CONTRACTID,
   nsAbLDAPReplicationServiceConstructor },

 {"Address LDAP Replication Query Interface",
   NS_ABLDAP_REPLICATIONQUERY_CID,
   NS_ABLDAP_REPLICATIONQUERY_CONTRACTID,
   nsAbLDAPReplicationQueryConstructor },

 {"Address LDAP Replication Processor Interface",
   NS_ABLDAP_PROCESSREPLICATIONDATA_CID,
   NS_ABLDAP_PROCESSREPLICATIONDATA_CONTRACTID,
   nsAbLDAPProcessReplicationDataConstructor },

 {"Address LDAP ChangeLog Query Interface",
   NS_ABLDAP_CHANGELOGQUERY_CID,
   NS_ABLDAP_CHANGELOGQUERY_CONTRACTID,
   nsAbLDAPChangeLogQueryConstructor },

 {"Address LDAP ChangeLog Processor Interface",
   NS_ABLDAP_PROCESSCHANGELOGDATA_CID,
   NS_ABLDAP_PROCESSCHANGELOGDATA_CONTRACTID,
   nsAbLDAPProcessChangeLogDataConstructor },

  { "Address LDAP autocomplete factory Interface", 
    NS_ABLDAPDIRFACTORY_CID,
    NS_ABLDAPACDIRFACTORY_CONTRACTID,
    nsAbLDAPDirFactoryConstructor },

  { "Address LDAP over SSL autocomplete factory Interface", 
    NS_ABLDAPDIRFACTORY_CID,
    NS_ABLDAPSACDIRFACTORY_CONTRACTID,
    nsAbLDAPDirFactoryConstructor },

  { "Address book LDAP autocomplete formatter",
    NS_ABLDAPAUTOCOMPFORMATTER_CID,
    NS_ABLDAPAUTOCOMPFORMATTER_CONTRACTID,
    nsAbLDAPAutoCompFormatterConstructor },

#endif

  { "The directory query proxy interface",
    NS_ABDIRECTORYQUERYPROXY_CID,
    NS_ABDIRECTORYQUERYPROXY_CONTRACTID,
    nsAbDirectoryQueryProxyConstructor },

  { "addressbook view",
    NS_ABVIEW_CID,
    NS_ABVIEW_CONTRACTID,
    nsAbViewConstructor },

  { "vcard helper service",
    NS_MSGVCARDSERVICE_CID,
    NS_MSGVCARDSERVICE_CONTRACTID,
    nsMsgVCardServiceConstructor }

};


PR_STATIC_CALLBACK(void)
msgAbModuleDtor(nsIModule* self)
{
    nsAddrDatabase::CleanupCache();
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsAbModule, components, msgAbModuleDtor)

