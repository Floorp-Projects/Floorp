/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Alec Flett <alecf@netscape.com>
 * Seth Spitzer <sspitzer@netscape.com>
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

/*
 * RDF datasource for the account manager
 */

#include "nsMsgAccountManagerDS.h"


#include "rdf.h"
#include "nsRDFCID.h"
#include "nsIRDFDataSource.h"
#include "nsEnumeratorUtils.h"
#include "nsIServiceManager.h"
#include "nsIMsgMailSession.h"
#include "nsXPIDLString.h"

#include "nsMsgRDFUtils.h"
#include "nsIMsgFolder.h"
#include "nsMsgBaseCID.h"
#include "nsMsgIncomingServer.h"

#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"

// turn this on to see useful output
#undef DEBUG_amds

#define NC_RDF_PAGETITLE_PREFIX               NC_NAMESPACE_URI "PageTitle"
#define NC_RDF_PAGETITLE_MAIN                 NC_RDF_PAGETITLE_PREFIX "Main"
#define NC_RDF_PAGETITLE_SERVER               NC_RDF_PAGETITLE_PREFIX "Server"
#define NC_RDF_PAGETITLE_COPIES               NC_RDF_PAGETITLE_PREFIX "Copies"
#define NC_RDF_PAGETITLE_ADVANCED             NC_RDF_PAGETITLE_PREFIX "Advanced"
#define NC_RDF_PAGETITLE_OFFLINEANDDISKSPACE  NC_RDF_PAGETITLE_PREFIX "OfflineAndDiskSpace"
#define NC_RDF_PAGETITLE_DISKSPACE            NC_RDF_PAGETITLE_PREFIX "DiskSpace"
#define NC_RDF_PAGETITLE_ADDRESSING           NC_RDF_PAGETITLE_PREFIX "Addressing"
#define NC_RDF_PAGETITLE_SMTP                 NC_RDF_PAGETITLE_PREFIX "SMTP"
#define NC_RDF_PAGETAG NC_NAMESPACE_URI "PageTag"


#define NC_RDF_ACCOUNTROOT "msgaccounts:/"

typedef struct _serverCreationParams {
  nsISupportsArray *serverArray;
  nsIRDFService *rdfService;
} serverCreationParams;

typedef struct {
    const char* serverKey;
    PRBool found;
} findServerByKeyEntry;

// the root resource (msgaccounts:/)
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_AccountRoot=nsnull;

// attributes of accounts
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Name=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_FolderTreeName=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_FolderTreeSimpleName=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_NameSort=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_FolderTreeNameSort=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTag=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_IsDefaultServer=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_SupportsFilters=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_CanGetMessages=nsnull;

// containment
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Child=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Settings=nsnull;


// properties corresponding to interfaces
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Account=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Server=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Identity=nsnull;

// individual pages
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTitleMain=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTitleServer=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTitleCopies=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTitleOfflineAndDiskSpace=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTitleDiskSpace=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTitleAddressing=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTitleAdvanced=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTitleSMTP=nsnull;

// common literals
nsIRDFLiteral* nsMsgAccountManagerDataSource::kTrueLiteral = nsnull;

nsIAtom* nsMsgAccountManagerDataSource::kDefaultServerAtom = nsnull;

nsrefcnt nsMsgAccountManagerDataSource::gAccountManagerResourceRefCnt = 0;

// shared arc lists
nsCOMPtr<nsISupportsArray> nsMsgAccountManagerDataSource::mAccountArcsOut;
nsCOMPtr<nsISupportsArray> nsMsgAccountManagerDataSource::mAccountRootArcsOut;


// RDF to match
#define NC_RDF_ACCOUNT NC_NAMESPACE_URI "Account"
#define NC_RDF_SERVER  NC_NAMESPACE_URI "Server"
#define NC_RDF_IDENTITY NC_NAMESPACE_URI "Identity"
#define NC_RDF_SETTINGS NC_NAMESPACE_URI "Settings"
#define NC_RDF_ISDEFAULTSERVER NC_NAMESPACE_URI "IsDefaultServer"
#define NC_RDF_SUPPORTSFILTERS NC_NAMESPACE_URI "SupportsFilters"
#define NC_RDF_CANGETMESSAGES NC_NAMESPACE_URI "CanGetMessages"

nsMsgAccountManagerDataSource::nsMsgAccountManagerDataSource()
{
#ifdef DEBUG_amds
  printf("nsMsgAccountManagerDataSource() being created\n");
#endif
  
  // do per-class initialization here
  if (gAccountManagerResourceRefCnt++ == 0) {
      getRDFService()->GetResource(NC_RDF_CHILD, &kNC_Child);
      getRDFService()->GetResource(NC_RDF_NAME, &kNC_Name);
      getRDFService()->GetResource(NC_RDF_FOLDERTREENAME, &kNC_FolderTreeName);
      getRDFService()->GetResource(NC_RDF_FOLDERTREESIMPLENAME, &kNC_FolderTreeSimpleName);
      getRDFService()->GetResource(NC_RDF_NAME_SORT, &kNC_NameSort);
      getRDFService()->GetResource(NC_RDF_FOLDERTREENAME_SORT, &kNC_FolderTreeNameSort);
      getRDFService()->GetResource(NC_RDF_PAGETAG, &kNC_PageTag);
      getRDFService()->GetResource(NC_RDF_ISDEFAULTSERVER, &kNC_IsDefaultServer);
      getRDFService()->GetResource(NC_RDF_SUPPORTSFILTERS, &kNC_SupportsFilters);
      getRDFService()->GetResource(NC_RDF_CANGETMESSAGES, &kNC_CanGetMessages);
      getRDFService()->GetResource(NC_RDF_ACCOUNT, &kNC_Account);
      getRDFService()->GetResource(NC_RDF_SERVER, &kNC_Server);
      getRDFService()->GetResource(NC_RDF_IDENTITY, &kNC_Identity);
      getRDFService()->GetResource(NC_RDF_PAGETITLE_MAIN, &kNC_PageTitleMain);
      getRDFService()->GetResource(NC_RDF_PAGETITLE_SERVER, &kNC_PageTitleServer);
      getRDFService()->GetResource(NC_RDF_PAGETITLE_COPIES, &kNC_PageTitleCopies);
      getRDFService()->GetResource(NC_RDF_PAGETITLE_OFFLINEANDDISKSPACE, &kNC_PageTitleOfflineAndDiskSpace);
      getRDFService()->GetResource(NC_RDF_PAGETITLE_DISKSPACE, &kNC_PageTitleDiskSpace);
      getRDFService()->GetResource(NC_RDF_PAGETITLE_ADDRESSING, &kNC_PageTitleAddressing);
      getRDFService()->GetResource(NC_RDF_PAGETITLE_ADVANCED, &kNC_PageTitleAdvanced);
      getRDFService()->GetResource(NC_RDF_PAGETITLE_SMTP, &kNC_PageTitleSMTP);
      
      getRDFService()->GetResource(NC_RDF_ACCOUNTROOT, &kNC_AccountRoot);

      getRDFService()->GetLiteral(NS_LITERAL_STRING("true").get(),
                                  &kTrueLiteral);
      
      // eventually these need to exist in some kind of array
      // that's easily extensible
      getRDFService()->GetResource(NC_RDF_SETTINGS, &kNC_Settings);
      
      kDefaultServerAtom = NS_NewAtom("DefaultServer");
    }
}

nsMsgAccountManagerDataSource::~nsMsgAccountManagerDataSource()
{
    nsCOMPtr<nsIMsgAccountManager> am = do_QueryReferent(mAccountManager);
    if (am)
      am->RemoveIncomingServerListener(this);
    
	if (--gAccountManagerResourceRefCnt == 0)
	{
      NS_IF_RELEASE(kNC_Child);
      NS_IF_RELEASE(kNC_Name);
      NS_IF_RELEASE(kNC_FolderTreeName);
      NS_IF_RELEASE(kNC_FolderTreeSimpleName);
      NS_IF_RELEASE(kNC_NameSort);
      NS_IF_RELEASE(kNC_FolderTreeNameSort);
      NS_IF_RELEASE(kNC_PageTag);
      NS_IF_RELEASE(kNC_IsDefaultServer);
      NS_IF_RELEASE(kNC_SupportsFilters);
      NS_IF_RELEASE(kNC_CanGetMessages);
      NS_IF_RELEASE(kNC_Account);
      NS_IF_RELEASE(kNC_Server);
      NS_IF_RELEASE(kNC_Identity);
      NS_IF_RELEASE(kNC_PageTitleMain);
      NS_IF_RELEASE(kNC_PageTitleServer);
      NS_IF_RELEASE(kNC_PageTitleCopies);
      NS_IF_RELEASE(kNC_PageTitleOfflineAndDiskSpace);
      NS_IF_RELEASE(kNC_PageTitleDiskSpace);
      NS_IF_RELEASE(kNC_PageTitleAddressing);
      NS_IF_RELEASE(kNC_PageTitleAdvanced);
      NS_IF_RELEASE(kNC_PageTitleSMTP);
      NS_IF_RELEASE(kTrueLiteral);
      
      NS_IF_RELEASE(kNC_AccountRoot);
      
      // eventually these need to exist in some kind of array
      // that's easily extensible
      NS_IF_RELEASE(kNC_Settings);


      NS_IF_RELEASE(kDefaultServerAtom);
      mAccountArcsOut = nsnull;
      mAccountRootArcsOut = nsnull;
	}

}

NS_IMPL_ADDREF_INHERITED(nsMsgAccountManagerDataSource, nsMsgRDFDataSource)
NS_IMPL_RELEASE_INHERITED(nsMsgAccountManagerDataSource, nsMsgRDFDataSource)
NS_INTERFACE_MAP_BEGIN(nsMsgAccountManagerDataSource)
    NS_INTERFACE_MAP_ENTRY(nsIIncomingServerListener)
    NS_INTERFACE_MAP_ENTRY(nsIFolderListener)
NS_INTERFACE_MAP_END_INHERITING(nsMsgRDFDataSource)

nsresult
nsMsgAccountManagerDataSource::Init()
{
    nsresult rv;

    rv = nsMsgRDFDataSource::Init();
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIMsgAccountManager> am;

    // get a weak ref to the account manager
    if (!mAccountManager) {
        am = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
        mAccountManager = getter_AddRefs(NS_GetWeakReference(am));
    } else
        am = do_QueryReferent(mAccountManager);

    if (am) {
        am->AddIncomingServerListener(this);
        am->AddRootFolderListener(this);
    }
    

    return NS_OK;
}

void nsMsgAccountManagerDataSource::Cleanup()
{
    nsCOMPtr<nsIMsgAccountManager> am =
        do_QueryReferent(mAccountManager);

    if (am) {
        am->RemoveIncomingServerListener(this);
        am->RemoveRootFolderListener(this);
    }

	nsMsgRDFDataSource::Cleanup();
}

/* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource property, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::GetTarget(nsIRDFResource *source,
                                         nsIRDFResource *property,
                                         PRBool aTruthValue,
                                         nsIRDFNode **target)
{
  nsresult rv;
  
  
  rv = NS_RDF_NO_VALUE;

  nsAutoString str;
  if (property == kNC_Name || property == kNC_FolderTreeName || property == kNC_FolderTreeSimpleName) {

      rv = getStringBundle();
      NS_ENSURE_SUCCESS(rv, rv);
      
      nsXPIDLString pageTitle;
      if (source == kNC_PageTitleServer)
          mStringBundle->GetStringFromName(NS_LITERAL_STRING("prefPanel-server").get(),
                                           getter_Copies(pageTitle));
      
      else if (source == kNC_PageTitleCopies)
          mStringBundle->GetStringFromName(NS_LITERAL_STRING("prefPanel-copies").get(),
                                           getter_Copies(pageTitle));
      else if (source == kNC_PageTitleOfflineAndDiskSpace)
          mStringBundle->GetStringFromName(NS_LITERAL_STRING("prefPanel-offline-and-diskspace").get(),
                                           getter_Copies(pageTitle));
      else if (source == kNC_PageTitleDiskSpace)
          mStringBundle->GetStringFromName(NS_LITERAL_STRING("prefPanel-diskspace").get(),
                                           getter_Copies(pageTitle));
      else if (source == kNC_PageTitleAddressing)
          mStringBundle->GetStringFromName(NS_LITERAL_STRING("prefPanel-addressing").get(),
                                           getter_Copies(pageTitle));

      else if (source == kNC_PageTitleAdvanced)
          mStringBundle->GetStringFromName(NS_LITERAL_STRING("prefPanel-advanced").get(),
                                           getter_Copies(pageTitle));

      else if (source == kNC_PageTitleSMTP)
          mStringBundle->GetStringFromName(NS_LITERAL_STRING("prefPanel-smtp").get(),
                                           getter_Copies(pageTitle));

      else {
          // if it's a server, use the pretty name
          nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(source, &rv);
          if (NS_SUCCEEDED(rv) && folder) {
              PRBool isServer;
              rv = folder->GetIsServer(&isServer);
              if(NS_SUCCEEDED(rv) && isServer)
                  rv = folder->GetPrettyName(getter_Copies(pageTitle));
          }
          else {
            // allow for the accountmanager to be dynamically extended.

            nsCOMPtr<nsIStringBundleService> strBundleService =
                do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
            NS_ENSURE_SUCCESS(rv,rv);

            const char *sourceValue;
            rv = source->GetValueConst(&sourceValue);
            NS_ENSURE_SUCCESS(rv,rv);

            // make sure the pointer math we're about to do is safe.
            NS_ENSURE_TRUE(sourceValue && (nsCRT::strlen(sourceValue) > nsCRT::strlen(NC_RDF_PAGETITLE_PREFIX)), NS_ERROR_UNEXPECTED);

            nsCAutoString bundleURL;
            bundleURL = "chrome://messenger/locale/";
            bundleURL += "am-";
            // turn NC#PageTitlefoobar into foobar, so we can get the am-foobar.properties bundle
            bundleURL += (sourceValue + nsCRT::strlen(NC_RDF_PAGETITLE_PREFIX));
            bundleURL += ".properties";

            nsCOMPtr <nsIStringBundle> bundle;
            rv = strBundleService->CreateBundle(bundleURL.get(),
                                        getter_AddRefs(bundle));

            NS_ENSURE_SUCCESS(rv,rv);

            nsAutoString panelTitleName;
            panelTitleName = NS_LITERAL_STRING("prefPanel-");
            panelTitleName.AppendWithConversion(sourceValue + nsCRT::strlen(NC_RDF_PAGETITLE_PREFIX));
            bundle->GetStringFromName(panelTitleName.get(),
                getter_Copies(pageTitle));
      }
  }
      str = pageTitle.get();
  }
  else if (property == kNC_PageTag) {
    // do NOT localize these strings. these are the urls of the XUL files
    if (source == kNC_PageTitleServer)
      str = NS_LITERAL_STRING("am-server.xul");
    else if (source == kNC_PageTitleCopies)
      str = NS_LITERAL_STRING("am-copies.xul");
    else if ((source == kNC_PageTitleOfflineAndDiskSpace) || (source == kNC_PageTitleDiskSpace))
      str = NS_LITERAL_STRING("am-offline.xul");
    else if (source == kNC_PageTitleAddressing)
      str = NS_LITERAL_STRING("am-addressing.xul");
    else if (source == kNC_PageTitleAdvanced)
      str = NS_LITERAL_STRING("am-advanced.xul");
    else if (source == kNC_PageTitleSMTP) 
      str = NS_LITERAL_STRING("am-smtp.xul");
    else {
      nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(source, &rv);
      if (NS_SUCCEEDED(rv) && folder) {
      /* if this is a server, with no identities, then we show a special panel */
      nsCOMPtr<nsIMsgIncomingServer> server;
      rv = getServerForFolderNode(source, getter_AddRefs(server));
      if (server) {
          PRBool hasIdentities;
          rv = serverHasIdentities(server, &hasIdentities);
          if (NS_SUCCEEDED(rv) && !hasIdentities) {
            str = NS_LITERAL_STRING("am-serverwithnoidentities.xul");
          }
          else {
            str = NS_LITERAL_STRING("am-main.xul");
          }
        }
        else {
          str = NS_LITERAL_STRING("am-main.xul");
        }
      }
      else {
        // allow for the accountmanager to be dynamically extended
        const char *sourceValue;
        rv = source->GetValueConst(&sourceValue);
        NS_ENSURE_SUCCESS(rv,rv);
        
        // make sure the pointer math we're about to do is safe.
        NS_ENSURE_TRUE(sourceValue && (nsCRT::strlen(sourceValue) > nsCRT::strlen(NC_RDF_PAGETITLE_PREFIX)), NS_ERROR_UNEXPECTED);
        
        // turn NC#PageTitlefoobar into foobar, so we can get the am-foobar.xul file
        str = NS_LITERAL_STRING("am-");
        str.AppendWithConversion((sourceValue + nsCRT::strlen(NC_RDF_PAGETITLE_PREFIX)));
        str += NS_LITERAL_STRING(".xul");
      }
    }
  }

  // handle sorting of servers
  else if ((property == kNC_NameSort) ||
           (property == kNC_FolderTreeNameSort)) {

    // make sure we're handling a root folder that is a server
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = getServerForFolderNode(source, getter_AddRefs(server));

    // only answer for servers!
    if (NS_FAILED(rv) || !server)
        return NS_RDF_NO_VALUE;

    // order is:
    // - default account
    // - <other servers>
    // - Local Folders
    // - news
    
    PRInt32 accountNum;
    nsCOMPtr<nsIMsgAccountManager> am =
        do_QueryReferent(mAccountManager);

    if (isDefaultServer(server))
        str = NS_LITERAL_STRING("0000");
    else {
    
        rv = am->FindServerIndex(server, &accountNum);
        if (NS_FAILED(rv)) return rv;
        
        // this is a hack for now - hardcode server order by type
        nsXPIDLCString serverType;
        server->GetType(getter_Copies(serverType));
        
        if (nsCRT::strcasecmp(serverType, "none")==0)
            accountNum += 2000;
        else if (nsCRT::strcasecmp(serverType, "nntp")==0)
            accountNum += 3000;
        else
            accountNum += 1000;     // default is to appear at the top
        
        str.AppendInt(accountNum);
    }
  }

  // GetTargets() stuff - need to return a valid answer so that
  // twisties will appear
  else if (property == kNC_Settings) {
    nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(source,&rv);
    if (NS_FAILED(rv))
      return NS_RDF_NO_VALUE;
    
    PRBool isServer=PR_FALSE;
    folder->GetIsServer(&isServer);
    // no need to localize this!
    if (isServer)
      str = NS_LITERAL_STRING("ServerSettings");
  }

  else if (property == kNC_IsDefaultServer) {
      nsCOMPtr<nsIMsgIncomingServer> thisServer;
      rv = getServerForFolderNode(source, getter_AddRefs(thisServer));
      if (NS_FAILED(rv) || !thisServer) return NS_RDF_NO_VALUE;

      if (isDefaultServer(thisServer))
          str = NS_LITERAL_STRING("true");
  }

  else if (property == kNC_SupportsFilters) {
      nsCOMPtr<nsIMsgIncomingServer> server;
      rv = getServerForFolderNode(source, getter_AddRefs(server));
      if (NS_FAILED(rv) || !server) return NS_RDF_NO_VALUE;

      if (supportsFilters(server))
          str = NS_LITERAL_STRING("true");
  }
  else if (property == kNC_CanGetMessages) {
      nsCOMPtr<nsIMsgIncomingServer> server;
      rv = getServerForFolderNode(source, getter_AddRefs(server));
      if (NS_FAILED(rv) || !server) return NS_RDF_NO_VALUE;

      if (canGetMessages(server))
          str = NS_LITERAL_STRING("true");
  }
  if (!str.IsEmpty())
    rv = createNode(str.get(), target, getRDFService());

  //if we have an empty string and we don't have an error value, then 
  //we don't have a value for RDF.
  else if(NS_SUCCEEDED(rv))
	rv = NS_RDF_NO_VALUE;

  return rv;
}



/* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource property, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::GetTargets(nsIRDFResource *source,
                                   nsIRDFResource *property,
                                   PRBool aTruthValue,
                                   nsISimpleEnumerator **_retval)
{
  nsresult rv = NS_RDF_NO_VALUE;

  // create array and enumerator
  // even if we're not handling this we need to return something empty?
  nsCOMPtr<nsISupportsArray> nodes;
  rv = NS_NewISupportsArray(getter_AddRefs(nodes));
  if (NS_FAILED(rv)) return rv;

  rv = NS_NewArrayEnumerator(_retval, nodes);
  if (NS_FAILED(rv)) return rv;

#ifdef DEBUG_amds
  nsXPIDLCString source_value;
  rv = source->GetValue(getter_Copies(source_value));

  nsXPIDLCString property_arc;
  rv = property->GetValue(getter_Copies(property_arc));
  if (NS_FAILED(rv)) return rv;
  
  printf("GetTargets(%s with arc %s...)\n",
         (const char*)source_value,
         (const char*)property_arc);
#endif
  
  if (source == kNC_AccountRoot)
      rv = createRootResources(property, nodes);
  else if (property == kNC_Settings)
      rv = createSettingsResources(source, nodes);

  if (NS_FAILED(rv))
      return NS_RDF_NO_VALUE;
    
  return NS_OK;
}

// end of all arcs coming out of msgaccounts:/
nsresult
nsMsgAccountManagerDataSource::createRootResources(nsIRDFResource *property,
                                                   nsISupportsArray* aNodeArray)
{
    nsresult rv = NS_OK;
    if (isContainment(property)) {
        
        nsCOMPtr<nsIMsgAccountManager> am =
            do_QueryReferent(mAccountManager);
        if (!am) return NS_ERROR_FAILURE;
        
        nsCOMPtr<nsISupportsArray> servers;
        rv = am->GetAllServers(getter_AddRefs(servers));
        if (NS_FAILED(rv)) return rv;
        
        // fill up the nodes array with the RDF Resources for the servers
        serverCreationParams params = { aNodeArray, getRDFService() };
        servers->EnumerateForwards(createServerResources, (void*)&params);
#ifdef DEBUG_amds
        PRUint32 nodecount;
        aNodeArray->Count(&nodecount);
        printf("GetTargets(): added %d servers on %s\n", nodecount,
               (const char*)property_arc);
#endif
        // for the "settings" arc, we also want to do an SMTP tag
        if (property == kNC_Settings) {
            aNodeArray->AppendElement(kNC_PageTitleSMTP);
        }
    }

#ifdef DEBUG_amds
    else {
        printf("unknown arc %s on msgaccounts:/\n", (const char*)property_arc);
    }
#endif

    return rv;
}

nsresult
nsMsgAccountManagerDataSource::appendGenericSettingsResources(nsIMsgIncomingServer *server, nsISupportsArray *aNodeArray)
{
  nsresult rv;

  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsISimpleEnumerator> e;
  rv = catman->EnumerateCategory(MAILNEWS_ACCOUNTMANAGER_EXTENSIONS, getter_AddRefs(e));
  if(NS_SUCCEEDED(rv) && e) {
    while (PR_TRUE) {
      nsCOMPtr<nsISupportsString> catEntry;
      rv = e->GetNext(getter_AddRefs(catEntry));
      if (NS_FAILED(rv) || !catEntry) 
        break;

      nsXPIDLCString entryString;
      rv = catEntry->GetData(getter_Copies(entryString));
      if (NS_FAILED(rv))
         break;

      nsXPIDLCString contractidString;
      rv = catman->GetCategoryEntry(MAILNEWS_ACCOUNTMANAGER_EXTENSIONS, entryString.get(), getter_Copies(contractidString));
      if (NS_FAILED(rv)) 
        break;

      nsCOMPtr <nsIMsgAccountManagerExtension> extension = do_GetService(contractidString.get(), &rv);
      if (NS_FAILED(rv) || !extension)
        break;
      
      PRBool showPanel;
      rv = extension->ShowPanel(server, &showPanel);
      if (NS_FAILED(rv)) 
        break;

      if (showPanel) {
        nsXPIDLCString name;
        rv = extension->GetName(getter_Copies(name));
        if (NS_FAILED(rv))
          break;
      
        rv = appendGenericSetting(name.get(), aNodeArray);
        if (NS_FAILED(rv))
          break;
      }
    }
  }
  return NS_OK;
}

nsresult 
nsMsgAccountManagerDataSource::appendGenericSetting(const char *name, nsISupportsArray *aNodeArray)
{
  NS_ENSURE_ARG_POINTER(name);
  NS_ENSURE_ARG_POINTER(aNodeArray);

  nsCOMPtr <nsIRDFResource> resource;

  nsCAutoString resourceStr;
  resourceStr = NC_RDF_PAGETITLE_PREFIX;
  resourceStr += name;

  nsresult rv = getRDFService()->GetResource(resourceStr.get(), getter_AddRefs(resource));
  NS_ENSURE_SUCCESS(rv,rv);

  // AppendElement will addref.
  aNodeArray->AppendElement(resource);
  return NS_OK;
}

// end of all #Settings arcs
nsresult
nsMsgAccountManagerDataSource::createSettingsResources(nsIRDFResource *aSource,
                                                       nsISupportsArray *aNodeArray)
{
    nsresult rv;
    if (aSource == kNC_PageTitleSMTP) {
        // aNodeArray->AppendElement(kNC_PageTitleAdvanced);
    } else {

        /* if this is a server, then support the settings */
        nsCOMPtr<nsIMsgIncomingServer> server;
        rv = getServerForFolderNode(aSource, getter_AddRefs(server));
        if (NS_FAILED(rv)) return rv;
        if (server) {
        
            PRBool hasIdentities;
            rv = serverHasIdentities(server, &hasIdentities);
            if (NS_FAILED(rv)) return rv;
            
            if (hasIdentities) {
                aNodeArray->AppendElement(kNC_PageTitleServer);
                aNodeArray->AppendElement(kNC_PageTitleCopies);
                aNodeArray->AppendElement(kNC_PageTitleAddressing);

		// XXX todo, these should not be in the if block for
		// hasIdentities.  "Local Folders", which doesn't have any
		// identities, still has disk space settings
		// see bug #86132

                // Check the offline capability before adding 
                // offline item 
                PRInt32 offlineSupportLevel = 0;
                rv = server->GetOfflineSupportLevel(&offlineSupportLevel); 
                NS_ENSURE_SUCCESS(rv,rv);

                PRBool supportsDiskSpace;
                rv = server->GetSupportsDiskSpace(&supportsDiskSpace); 
                NS_ENSURE_SUCCESS(rv,rv);
                  
	        // currently there is no offline without diskspace
                if (offlineSupportLevel >= OFFLINE_SUPPORT_LEVEL_REGULAR) {
                   aNodeArray->AppendElement(kNC_PageTitleOfflineAndDiskSpace);
                }
                else if (supportsDiskSpace) {
                   aNodeArray->AppendElement(kNC_PageTitleDiskSpace);
                }
                
                // extensions come after the default panels
                rv = appendGenericSettingsResources(server, aNodeArray);
                NS_ASSERTION(NS_SUCCEEDED(rv), "failed to add generic panels");
            }
        }
    }

    return NS_OK;
}

nsresult
nsMsgAccountManagerDataSource::serverHasIdentities(nsIMsgIncomingServer* aServer,
                                                   PRBool *aResult)
{
    nsresult rv;
    *aResult = PR_FALSE;
    
    nsCOMPtr<nsIMsgAccountManager> am =
        do_QueryReferent(mAccountManager, &rv);
    
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsISupportsArray> identities;
    rv = am->GetIdentitiesForServer(aServer, getter_AddRefs(identities));
    
    // no identities just means no arcs
    if (NS_FAILED(rv)) return NS_OK;
    
    PRUint32 count;
    rv = identities->Count(&count);
    if (NS_FAILED(rv)) return NS_OK;

    if (count >0)
        *aResult = PR_TRUE;

    return NS_OK;
}

// enumeration function to convert each server (element)
// to an nsIRDFResource and append it to the array (in data)
// always return PR_TRUE to try on every element instead of aborting early
PRBool
nsMsgAccountManagerDataSource::createServerResources(nsISupports *element,
                                                     void *data)
{
  nsresult rv;
  // get parameters out of the data argument
  serverCreationParams *params = (serverCreationParams*)data;
  nsCOMPtr<nsISupportsArray> servers = dont_QueryInterface(params->serverArray);
  nsCOMPtr<nsIRDFService> rdf = dont_QueryInterface(params->rdfService);

  // the server itself is in the element argument
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return PR_TRUE;

	nsCOMPtr <nsIFolder> serverFolder;
	rv = server->GetRootFolder(getter_AddRefs(serverFolder));
	if(NS_FAILED(rv)) return PR_TRUE;

  // add the resource to the array
  nsCOMPtr<nsIRDFResource> serverResource = do_QueryInterface(serverFolder);
	if(!serverResource)
		return PR_TRUE;

  rv = servers->AppendElement(serverResource);
  if (NS_FAILED(rv)) return PR_TRUE;
  
  return PR_TRUE;
}

nsresult
nsMsgAccountManagerDataSource::getAccountArcs(nsISupportsArray **aResult)
{
    nsresult rv;
    if (!mAccountArcsOut) {

        rv = NS_NewISupportsArray(getter_AddRefs(mAccountArcsOut));
        NS_ENSURE_SUCCESS(rv, rv);
        
        mAccountArcsOut->AppendElement(kNC_Settings);
        mAccountArcsOut->AppendElement(kNC_Name);
        mAccountArcsOut->AppendElement(kNC_FolderTreeName);
        mAccountArcsOut->AppendElement(kNC_FolderTreeSimpleName);
        mAccountArcsOut->AppendElement(kNC_NameSort);
        mAccountArcsOut->AppendElement(kNC_FolderTreeNameSort);
        mAccountArcsOut->AppendElement(kNC_PageTag);
    }

    *aResult = mAccountArcsOut;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

nsresult
nsMsgAccountManagerDataSource::getAccountRootArcs(nsISupportsArray **aResult)
{
    nsresult rv;
    if (!mAccountRootArcsOut) {

        rv = NS_NewISupportsArray(getter_AddRefs(mAccountRootArcsOut));
        NS_ENSURE_SUCCESS(rv, rv);
        
        mAccountRootArcsOut->AppendElement(kNC_Server);
        mAccountRootArcsOut->AppendElement(kNC_Child);
        
        mAccountRootArcsOut->AppendElement(kNC_Settings);
        mAccountRootArcsOut->AppendElement(kNC_Name);
        mAccountRootArcsOut->AppendElement(kNC_FolderTreeName);
        mAccountRootArcsOut->AppendElement(kNC_FolderTreeSimpleName);
        mAccountRootArcsOut->AppendElement(kNC_NameSort);
        mAccountRootArcsOut->AppendElement(kNC_FolderTreeNameSort);
        mAccountRootArcsOut->AppendElement(kNC_PageTag);
    }

    *aResult = mAccountRootArcsOut;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManagerDataSource::HasArcOut(nsIRDFResource *source, nsIRDFResource *aArc, PRBool *result)
{
    nsresult rv = NS_OK;
    if (aArc == kNC_Settings) {
      // based on createSettingsResources()
      // we only have settings for servers with identities
      nsCOMPtr<nsIMsgIncomingServer> server;
      rv = getServerForFolderNode(source, getter_AddRefs(server));
      if (server) {
          // this is not complete, see bug #86132
          return serverHasIdentities(server, result);
      }
	}
	
	*result = PR_FALSE;
	return NS_OK;
}

/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::ArcLabelsOut(nsIRDFResource *source,
                                            nsISimpleEnumerator **_retval)
{
  nsresult rv;

  // we have to return something, so always create the array/enumerators
  nsCOMPtr<nsISupportsArray> arcs;
  if (source == kNC_AccountRoot)
      rv = getAccountRootArcs(getter_AddRefs(arcs));
  else
      rv = getAccountArcs(getter_AddRefs(arcs));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewArrayEnumerator(_retval, arcs);
  if (NS_FAILED(rv)) return rv;
  
#ifdef DEBUG_amds_
  printf("GetArcLabelsOut(%s): Adding child, settings, and name arclabels\n", value);
#endif
  
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManagerDataSource::HasAssertion(nsIRDFResource *aSource,
                                            nsIRDFResource *aProperty,
                                            nsIRDFNode *aTarget,
                                            PRBool aTruthValue,
                                            PRBool *_retval)
{
  nsresult rv=NS_ERROR_FAILURE;

  //
  // msgaccounts:/ properties
  //
  if (aSource == kNC_AccountRoot) {
    rv = HasAssertionAccountRoot(aProperty, aTarget, aTruthValue, _retval);
  }
  //
  // server properties
  //   try to convert the resource to a folder, and then only
  //   answer if it's a server.. any failure falls through to the default case
  //
  // short-circuit on property, so objects like filters, etc, don't get queried
  else if (aProperty == kNC_IsDefaultServer || aProperty == kNC_CanGetMessages || 
           aProperty == kNC_SupportsFilters) {
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = getServerForFolderNode(aSource, getter_AddRefs(server));
    if (NS_SUCCEEDED(rv) && server)
      rv = HasAssertionServer(server, aProperty, aTarget,
                              aTruthValue, _retval);
  }

  // any failures above fallthrough to the parent class
  if (NS_FAILED(rv))
    return nsMsgRDFDataSource::HasAssertion(aSource, aProperty,
                                            aTarget, aTruthValue, _retval);
  return NS_OK;
}



nsresult
nsMsgAccountManagerDataSource::HasAssertionServer(nsIMsgIncomingServer *aServer,
                                                  nsIRDFResource *aProperty,
                                                  nsIRDFNode *aTarget,
                                                  PRBool aTruthValue,
                                                  PRBool *_retval)
{

    if (aProperty == kNC_IsDefaultServer) {
        if (aTarget == kTrueLiteral)
            *_retval = isDefaultServer(aServer);
        else
            *_retval = !isDefaultServer(aServer);

    } else if (aProperty == kNC_SupportsFilters) {
        if (aTarget == kTrueLiteral) {
            *_retval = supportsFilters(aServer);
        } else
            *_retval = !supportsFilters(aServer);
    } else if (aProperty == kNC_CanGetMessages) {
        if (aTarget == kTrueLiteral) {
            *_retval = canGetMessages(aServer);
        } else
            *_retval = !canGetMessages(aServer);
    } else {
        *_retval = PR_FALSE;
    }
    return NS_OK;
}

PRBool
nsMsgAccountManagerDataSource::isDefaultServer(nsIMsgIncomingServer *aServer)
{
    nsresult rv;
    if (!aServer) return PR_FALSE;
    
    nsCOMPtr<nsIMsgAccountManager> am =
        do_QueryReferent(mAccountManager, &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    
    nsCOMPtr<nsIMsgAccount> defaultAccount;
    rv = am->GetDefaultAccount(getter_AddRefs(defaultAccount));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    if (!defaultAccount) return PR_FALSE;

    // in some wierd case that there is no default and they asked
    // for the default
    nsCOMPtr<nsIMsgIncomingServer> defaultServer;
    rv = defaultAccount->GetIncomingServer(getter_AddRefs(defaultServer));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    if (!defaultServer) return PR_FALSE;

    PRBool isEqual;
    rv = defaultServer->Equals(aServer, &isEqual);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    return isEqual;
}


PRBool
nsMsgAccountManagerDataSource::supportsFilters(nsIMsgIncomingServer *aServer)
{
    PRBool supportsFilters;
    nsresult rv = aServer->GetCanHaveFilters(&supportsFilters);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    return supportsFilters;
}

PRBool
nsMsgAccountManagerDataSource::canGetMessages(nsIMsgIncomingServer *aServer)
{
      nsresult rv; 

      nsXPIDLCString type;
      rv = aServer->GetType(getter_Copies(type));
      NS_ENSURE_SUCCESS(rv, PR_FALSE);

      nsCAutoString contractid(NS_MSGPROTOCOLINFO_CONTRACTID_PREFIX);
      contractid.Append(type);

      nsCOMPtr<nsIMsgProtocolInfo> protocolInfo =
           do_GetService(contractid.get(), &rv);
      NS_ENSURE_SUCCESS(rv, PR_FALSE);

      PRBool canGetMessages = PR_FALSE;
      protocolInfo->GetCanGetMessages(&canGetMessages);

      return canGetMessages;
}

nsresult
nsMsgAccountManagerDataSource::HasAssertionAccountRoot(nsIRDFResource *aProperty,
                                                       nsIRDFNode *aTarget,
                                                       PRBool aTruthValue,
                                                       PRBool *_retval)
{

  nsresult rv;

  // set up default
  *_retval = PR_FALSE;
  
  // for child and settings arcs, just make sure it's a valid server:
  if (isContainment(aProperty)) {

    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = getServerForFolderNode(aTarget, getter_AddRefs(server));
    if (NS_FAILED(rv) || !server) return rv;

    nsXPIDLCString serverKey;
    server->GetKey(getter_Copies(serverKey));
    
    nsCOMPtr<nsIMsgAccountManager> am =
        do_QueryReferent(mAccountManager, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsISupportsArray> serverArray;
    rv = am->GetAllServers(getter_AddRefs(serverArray));
    if (NS_FAILED(rv)) return rv;

    findServerByKeyEntry entry;
    entry.serverKey = serverKey;
    entry.found = PR_FALSE;

    serverArray->EnumerateForwards(findServerByKey, &entry);
    (*_retval) = entry.found;
    
  }
  
  return NS_OK;
}

PRBool
nsMsgAccountManagerDataSource::isContainment(nsIRDFResource *aProperty) {
  
  if (aProperty == kNC_Child ||
      aProperty == kNC_Settings)
    return PR_TRUE;
  return PR_FALSE;
}

// returns failure if the object is not a root server
nsresult
nsMsgAccountManagerDataSource::getServerForFolderNode(nsIRDFNode *aResource,
                                                      nsIMsgIncomingServer **aResult)
{
    return getServerForObject(aResource, aResult);
}

nsresult
nsMsgAccountManagerDataSource::getServerForObject(nsISupports *aObject,
                                                  nsIMsgIncomingServer **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIMsgFolder> folder =
    do_QueryInterface(aObject, &rv);
  if (NS_SUCCEEDED(rv)) {
    PRBool isServer;
    rv = folder->GetIsServer(&isServer);
    if (NS_SUCCEEDED(rv) && isServer) {
        return folder->GetServer(aResult);
    }
  }
  return NS_ERROR_FAILURE;
}

PRBool
nsMsgAccountManagerDataSource::findServerByKey(nsISupports *aElement,
                                               void *aData)
{
    nsresult rv;
    findServerByKeyEntry *entry = (findServerByKeyEntry*)aData;
    
    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(aElement, &rv);
    if (NS_FAILED(rv)) return PR_TRUE;

    nsXPIDLCString key;
    server->GetKey(getter_Copies(key));
    if (nsCRT::strcmp(key, entry->serverKey)==0) {
        entry->found = PR_TRUE;
        return PR_FALSE;        // stop when found
    }
    
    return PR_TRUE;
}

nsresult
nsMsgAccountManagerDataSource::getStringBundle()
{
    if (mStringBundle) return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIStringBundleService> strBundleService =
        do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = strBundleService->CreateBundle("chrome://messenger/locale/prefs.properties",
                                        getter_AddRefs(mStringBundle));
    return rv;
}

NS_IMETHODIMP
nsMsgAccountManagerDataSource::OnServerLoaded(nsIMsgIncomingServer* aServer)
{
  nsresult rv;

  nsCOMPtr<nsIFolder> serverFolder;
  rv = aServer->GetRootFolder(getter_AddRefs(serverFolder));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFResource> serverResource =
    do_QueryInterface(serverFolder,&rv);
  if (NS_FAILED(rv)) return rv;

#ifdef DEBUG_alecf
  nsXPIDLCString serverUri;
  serverResource->GetValue(getter_Copies(serverUri));
  printf("nsMsgAccountmanagerDataSource::OnServerLoaded(%s)\n", (const char*)serverUri);
#endif
  
  NotifyObservers(kNC_AccountRoot, kNC_Child, serverResource, PR_TRUE, PR_FALSE);
  NotifyObservers(kNC_AccountRoot, kNC_Settings, serverResource, PR_TRUE, PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgAccountManagerDataSource::OnServerUnloaded(nsIMsgIncomingServer* aServer)
{
  nsresult rv;
  
  
  nsCOMPtr<nsIFolder> serverFolder;
  rv = aServer->GetRootFolder(getter_AddRefs(serverFolder));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFResource> serverResource =
    do_QueryInterface(serverFolder,&rv);
  if (NS_FAILED(rv)) return rv;

  
  NotifyObservers(kNC_AccountRoot, kNC_Child, serverResource, PR_FALSE, PR_FALSE);
  NotifyObservers(kNC_AccountRoot, kNC_Settings, serverResource, PR_FALSE, PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP 
nsMsgAccountManagerDataSource::OnServerChanged(nsIMsgIncomingServer *server)
{
  return NS_OK;
}

nsresult
nsMsgAccountManagerDataSource::OnItemPropertyChanged(nsISupports *, nsIAtom *, char const *, char const *)
{
    return NS_OK;
}

nsresult
nsMsgAccountManagerDataSource::OnItemUnicharPropertyChanged(nsISupports *, nsIAtom *, const PRUnichar *, const PRUnichar *)
{
    return NS_OK;
}

nsresult
nsMsgAccountManagerDataSource::OnItemRemoved(nsISupports *, nsISupports *, const char *)
{
    return NS_OK;
}

nsresult
nsMsgAccountManagerDataSource::OnItemPropertyFlagChanged(nsISupports *, nsIAtom *, PRUint32, PRUint32)
{
    return NS_OK;
}

nsresult
nsMsgAccountManagerDataSource::OnItemAdded(nsISupports *, nsISupports *, const char *)
{
    return NS_OK;
}


nsresult
nsMsgAccountManagerDataSource::OnItemBoolPropertyChanged(nsISupports *aItem,
                                                         nsIAtom *aProperty,
                                                         PRBool aOldValue,
                                                         PRBool aNewValue)
{
    nsresult rv;

    // server properties
    // check property first because that's fast
    if (aProperty == kDefaultServerAtom) {

        nsCOMPtr<nsIMsgIncomingServer> server;
        rv = getServerForObject(aItem, getter_AddRefs(server));
        if (NS_FAILED(rv))
            return NS_OK;

        // tricky - turn aItem into a resource -
        // aItem should be an nsIMsgFolder
        nsCOMPtr<nsIRDFResource> serverResource =
            do_QueryInterface(aItem, &rv);
        if (NS_FAILED(rv)) return NS_OK;

        NotifyObservers(serverResource, kNC_IsDefaultServer, kTrueLiteral,
                        aNewValue, PR_FALSE);
        
    }    
    return NS_OK;
}

nsresult
nsMsgAccountManagerDataSource::OnItemEvent(nsIFolder *, nsIAtom *)
{
    return NS_OK;
}

nsresult
nsMsgAccountManagerDataSource::OnItemIntPropertyChanged(nsISupports *, nsIAtom *, PRInt32, PRInt32)
{
    return NS_OK;
}

