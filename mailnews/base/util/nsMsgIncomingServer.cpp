/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsMsgIncomingServer.h"
#include "nscore.h"
#include "nsCom.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"

#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

#include "nsMsgBaseCID.h"
#include "nsIMsgFolder.h"
#include "nsIMsgFolderCache.h"
#include "nsIMsgFolderCacheElement.h"
#include "nsIMsgWindow.h"
#include "nsIMsgFilterService.h"
#include "nsIMsgProtocolInfo.h"

#include "nsIPref.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsINetPrompt.h"
#include "nsIWalletService.h"

#include "nsIRDFService.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsIXULWindow.h"
#include "nsRDFCID.h"

#ifdef DEBUG_sspitzer
#define DEBUG_MSGINCOMING_SERVER
#endif /* DEBUG_sspitzer */

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kWalletServiceCID, NS_WALLETSERVICE_CID);
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kMsgFilterServiceCID, NS_MSGFILTERSERVICE_CID);

MOZ_DECL_CTOR_COUNTER(nsMsgIncomingServer);

nsMsgIncomingServer::nsMsgIncomingServer():
    m_prefs(0),
    m_serverKey(0),
    m_rootFolder(0)
{
  NS_INIT_REFCNT();
  m_serverBusy = PR_FALSE;
  m_password = "";
}

nsMsgIncomingServer::~nsMsgIncomingServer()
{
    nsresult rv;
    if (mFilterList) {
        nsCOMPtr<nsIMsgFilterService> filterService =
            do_GetService(kMsgFilterServiceCID, &rv);
        if (NS_SUCCEEDED(rv))
            rv = filterService->CloseFilterList(mFilterList);
    }
    if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID,
                                                  m_prefs,
                                                  nsnull);
    PR_FREEIF(m_serverKey)
}

NS_IMPL_THREADSAFE_ADDREF(nsMsgIncomingServer);
NS_IMPL_THREADSAFE_RELEASE(nsMsgIncomingServer);
NS_INTERFACE_MAP_BEGIN(nsMsgIncomingServer)
    NS_INTERFACE_MAP_ENTRY(nsIMsgIncomingServer)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMsgIncomingServer)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_GETSET(nsMsgIncomingServer, ServerBusy, PRBool, m_serverBusy)
NS_IMPL_GETTER_STR(nsMsgIncomingServer::GetKey, m_serverKey)

NS_IMETHODIMP
nsMsgIncomingServer::SetKey(const char * serverKey)
{
    nsresult rv = NS_OK;
    // in order to actually make use of the key, we need the prefs
    if (!m_prefs)
        rv = nsServiceManager::GetService(kPrefServiceCID,
                                          NS_GET_IID(nsIPref),
                                          (nsISupports**)&m_prefs);

    PR_FREEIF(m_serverKey);
    m_serverKey = PL_strdup(serverKey);
    return rv;
}
    
NS_IMETHODIMP
nsMsgIncomingServer::SetRootFolder(nsIFolder * aRootFolder)
{
	m_rootFolder = aRootFolder;
	return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetRootFolder(nsIFolder * *aRootFolder)
{
	if (!aRootFolder)
		return NS_ERROR_NULL_POINTER;
    if (m_rootFolder) {
      *aRootFolder = m_rootFolder;
      NS_ADDREF(*aRootFolder);
    } else {
      nsresult rv = CreateRootFolder();
      if (NS_FAILED(rv)) return rv;
      
      *aRootFolder = m_rootFolder;
      NS_IF_ADDREF(*aRootFolder);
    }
	return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::PerformExpand()
{
#ifdef DEBUG_sspitzer
	printf("PerformExpand()\n");
#endif
	return NS_OK;
}

  
NS_IMETHODIMP
nsMsgIncomingServer::PerformBiff()
{
	//This had to be implemented in the derived class, but in case someone doesn't implement it
	//just return not implemented.
	return NS_ERROR_NOT_IMPLEMENTED;	
}

NS_IMETHODIMP nsMsgIncomingServer::WriteToFolderCache(nsIMsgFolderCache *folderCache)
{
	nsresult rv = NS_OK;
	if (m_rootFolder)
	{
		nsCOMPtr <nsIMsgFolder> msgFolder = do_QueryInterface(m_rootFolder, &rv);
		if (NS_SUCCEEDED(rv) && msgFolder)
			rv = msgFolder->WriteToFolderCache(folderCache);
	}
	return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::CloseCachedConnections()
{
	// derived class should override if they cache connections.
	return NS_OK;
}


// construct <localStoreType>://[<username>@]<hostname
NS_IMETHODIMP
nsMsgIncomingServer::GetServerURI(char **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    nsresult rv;
    nsCAutoString uri;

    nsXPIDLCString localStoreType;
    rv = GetLocalStoreType(getter_Copies(localStoreType));
    if (NS_FAILED(rv)) return rv;

    uri.Append(localStoreType);
    uri += "://";

    nsXPIDLCString username;
    rv = GetUsername(getter_Copies(username));

    if (NS_SUCCEEDED(rv) && ((const char*)username) && username[0]) {
        nsXPIDLCString escapedUsername;
        *((char **)getter_Copies(escapedUsername)) =
            nsEscape(username, url_XAlphas);
//            nsEscape(username, url_Path);
        // not all servers have a username 
        uri.Append(escapedUsername);
        uri += '@';
    }

    nsXPIDLCString hostname;
    rv = GetHostName(getter_Copies(hostname));

    if (NS_SUCCEEDED(rv) && ((const char*)hostname) && hostname[0]) {
        nsXPIDLCString escapedHostname;
        *((char **)getter_Copies(escapedHostname)) =
            nsEscape(hostname, url_Path);
        // not all servers have a hostname
        uri.Append(escapedHostname);
    }

    *aResult = uri.ToNewCString();
    return NS_OK;
}

nsresult
nsMsgIncomingServer::CreateRootFolder()
{
	nsresult rv;
			  // get the URI from the incoming server
  nsXPIDLCString serverUri;
  rv = GetServerURI(getter_Copies(serverUri));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE(nsIRDFService, rdf,
                        kRDFServiceCID, &rv);

  // get the corresponding RDF resource
  // RDF will create the server resource if it doesn't already exist
  nsCOMPtr<nsIRDFResource> serverResource;
  rv = rdf->GetResource(serverUri, getter_AddRefs(serverResource));
  if (NS_FAILED(rv)) return rv;

  // make incoming server know about its root server folder so we 
  // can find sub-folders given an incoming server.
  m_rootFolder = do_QueryInterface(serverResource, &rv);
  return rv;
}

void
nsMsgIncomingServer::getPrefName(const char *serverKey,
                                 const char *prefName,
                                 nsCString& fullPrefName)
{
    // mail.server.<key>.<pref>
    fullPrefName = "mail.server.";
    fullPrefName.Append(serverKey);
    fullPrefName.Append('.');
    fullPrefName.Append(prefName);
}

// this will be slightly faster than the above, and allows
// the "default" server preference root to be set in one place
void
nsMsgIncomingServer::getDefaultPrefName(const char *prefName,
                                        nsCString& fullPrefName)
{
    // mail.server.default.<pref>
    fullPrefName = "mail.server.default.";
    fullPrefName.Append(prefName);
}


nsresult
nsMsgIncomingServer::GetBoolValue(const char *prefname,
                                 PRBool *val)
{
  nsCAutoString fullPrefName;
  getPrefName(m_serverKey, prefname, fullPrefName);
  nsresult rv = m_prefs->GetBoolPref(fullPrefName, val);
  
  if (NS_FAILED(rv))
    rv = getDefaultBoolPref(prefname, val);
  
  return rv;
}

nsresult
nsMsgIncomingServer::getDefaultBoolPref(const char *prefname,
                                        PRBool *val) {
  
  nsCAutoString fullPrefName;
  getDefaultPrefName(prefname, fullPrefName);
  nsresult rv = m_prefs->GetBoolPref(fullPrefName, val);

  if (NS_FAILED(rv)) {
    *val = PR_FALSE;
    rv = NS_OK;
  }
  return rv;
}

nsresult
nsMsgIncomingServer::SetBoolValue(const char *prefname,
                                 PRBool val)
{
  nsresult rv;
  nsCAutoString fullPrefName;
  getPrefName(m_serverKey, prefname, fullPrefName);

  PRBool defaultValue;
  rv = getDefaultBoolPref(prefname, &defaultValue);

  if (NS_SUCCEEDED(rv) &&
      val == defaultValue)
    m_prefs->ClearUserPref(fullPrefName);
  else
    rv = m_prefs->SetBoolPref(fullPrefName, val);
  
  return rv;
}

nsresult
nsMsgIncomingServer::GetIntValue(const char *prefname,
                                PRInt32 *val)
{
  nsCAutoString fullPrefName;
  getPrefName(m_serverKey, prefname, fullPrefName);
  nsresult rv = m_prefs->GetIntPref(fullPrefName, val);

  if (NS_FAILED(rv))
    rv = getDefaultIntPref(prefname, val);
  
  return rv;
}

nsresult
nsMsgIncomingServer::GetFileValue(const char* prefname,
                                  nsIFileSpec **spec)
{
  nsCAutoString fullPrefName;
  getPrefName(m_serverKey, prefname, fullPrefName);
  nsresult rv = m_prefs->GetFilePref(fullPrefName, spec);

  return rv;
}

nsresult
nsMsgIncomingServer::SetFileValue(const char* prefname,
                                    nsIFileSpec *spec)
{
  nsCAutoString fullPrefName;
  getPrefName(m_serverKey, prefname, fullPrefName);
  nsresult rv = m_prefs->SetFilePref(fullPrefName, spec, PR_FALSE);

  return rv;
}

nsresult
nsMsgIncomingServer::getDefaultIntPref(const char *prefname,
                                        PRInt32 *val) {
  
  nsCAutoString fullPrefName;
  getDefaultPrefName(prefname, fullPrefName);
  nsresult rv = m_prefs->GetIntPref(fullPrefName, val);

  if (NS_FAILED(rv)) {
    *val = 0;
    rv = NS_OK;
  }
  
  return rv;
}

nsresult
nsMsgIncomingServer::SetIntValue(const char *prefname,
                                 PRInt32 val)
{
  nsresult rv;
  nsCAutoString fullPrefName;
  getPrefName(m_serverKey, prefname, fullPrefName);
  
  PRInt32 defaultVal;
  rv = getDefaultIntPref(prefname, &defaultVal);
  
  if (NS_SUCCEEDED(rv) && defaultVal == val)
    m_prefs->ClearUserPref(fullPrefName);
  else
    rv = m_prefs->SetIntPref(fullPrefName, val);
  
  return rv;
}

nsresult
nsMsgIncomingServer::GetCharValue(const char *prefname,
                                 char  **val)
{
  nsCAutoString fullPrefName;
  getPrefName(m_serverKey, prefname, fullPrefName);
  nsresult rv = m_prefs->CopyCharPref(fullPrefName, val);
  
  if (NS_FAILED(rv))
    rv = getDefaultCharPref(prefname, val);
  
  return rv;
}

nsresult
nsMsgIncomingServer::GetUnicharValue(const char *prefname,
                                     PRUnichar **val)
{
  nsCAutoString fullPrefName;
  getPrefName(m_serverKey, prefname, fullPrefName);
  nsresult rv = m_prefs->CopyUnicharPref(fullPrefName, val);
  
  if (NS_FAILED(rv))
    rv = getDefaultUnicharPref(prefname, val);
  
  return rv;
}

nsresult
nsMsgIncomingServer::getDefaultCharPref(const char *prefname,
                                        char **val) {
  
  nsCAutoString fullPrefName;
  getDefaultPrefName(prefname, fullPrefName);
  nsresult rv = m_prefs->CopyCharPref(fullPrefName, val);

  if (NS_FAILED(rv)) {
    *val = nsnull;              // null is ok to return here
    rv = NS_OK;
  }
  return rv;
}

nsresult
nsMsgIncomingServer::getDefaultUnicharPref(const char *prefname,
                                           PRUnichar **val) {
  
  nsCAutoString fullPrefName;
  getDefaultPrefName(prefname, fullPrefName);
  nsresult rv = m_prefs->CopyUnicharPref(fullPrefName, val);

  if (NS_FAILED(rv)) {
    *val = nsnull;              // null is ok to return here
    rv = NS_OK;
  }
  return rv;
}

nsresult
nsMsgIncomingServer::SetCharValue(const char *prefname,
                                 const char * val)
{
  nsresult rv;
  nsCAutoString fullPrefName;
  getPrefName(m_serverKey, prefname, fullPrefName);

  if (!val) {
    m_prefs->ClearUserPref(fullPrefName);
    return NS_OK;
  }
  
  char *defaultVal=nsnull;
  rv = getDefaultCharPref(prefname, &defaultVal);
  
  if (NS_SUCCEEDED(rv) &&
      PL_strcmp(defaultVal, val) == 0)
    m_prefs->ClearUserPref(fullPrefName);
  else
    rv = m_prefs->SetCharPref(fullPrefName, val);
  
  PR_FREEIF(defaultVal);
  
  return rv;
}

nsresult
nsMsgIncomingServer::SetUnicharValue(const char *prefname,
                                  const PRUnichar * val)
{
  nsresult rv;
  nsCAutoString fullPrefName;
  getPrefName(m_serverKey, prefname, fullPrefName);

  if (!val) {
    m_prefs->ClearUserPref(fullPrefName);
    return NS_OK;
  }

  PRUnichar *defaultVal=nsnull;
  rv = getDefaultUnicharPref(prefname, &defaultVal);
  if (defaultVal && NS_SUCCEEDED(rv) &&
      nsCRT::strcmp(defaultVal, val) == 0)
    m_prefs->ClearUserPref(fullPrefName);
  else
    rv = m_prefs->SetUnicharPref(fullPrefName, val);
  
  PR_FREEIF(defaultVal);
  
  return rv;
}

// pretty name is the display name to show to the user
NS_IMETHODIMP
nsMsgIncomingServer::GetPrettyName(PRUnichar **retval) {

  nsXPIDLString val;
  nsresult rv = GetUnicharValue("name", getter_Copies(val));
  if (NS_FAILED(rv)) return rv;

  nsAutoString prettyName(val);
  
  // if there's no name, then just return the hostname
  if (prettyName.IsEmpty()) {
    
    nsXPIDLCString username;
    rv = GetUsername(getter_Copies(username));
    if (NS_FAILED(rv)) return rv;
    if ((const char*)username &&
        PL_strcmp((const char*)username, "")!=0) {
      prettyName.Assign(username);
      prettyName += " on ";
    }
    
    nsXPIDLCString hostname;
    rv = GetHostName(getter_Copies(hostname));
    if (NS_FAILED(rv)) return rv;


    prettyName += hostname;
  }

  *retval = prettyName.ToNewUnicode();
  
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetPrettyName(const PRUnichar *value)
{
    SetUnicharValue("name", value);
    
    nsCOMPtr<nsIFolder> rootFolder;
    GetRootFolder(getter_AddRefs(rootFolder));

    if (rootFolder)
        rootFolder->SetPrettyName(value);

    return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::ToString(PRUnichar** aResult) {
  nsString servername("[nsIMsgIncomingServer: ");
  servername += m_serverKey;
  servername += "]";
  
  *aResult = servername.ToNewUnicode();
  NS_ASSERTION(*aResult, "no server name!");
  return NS_OK;
}
  

NS_IMETHODIMP nsMsgIncomingServer::SetPassword(const char * aPassword)
{
	m_password = aPassword;
	
	nsresult rv;
	PRBool rememberPassword = PR_FALSE;
	
	rv = GetRememberPassword(&rememberPassword);
	if (NS_FAILED(rv)) return rv;

	if (rememberPassword) {
		rv = StorePassword();
		if (NS_FAILED(rv)) return rv;
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgIncomingServer::GetPassword(char ** aPassword)
{
    NS_ENSURE_ARG_POINTER(aPassword);

	*aPassword = m_password.ToNewCString();

    return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetPasswordWithUI(const PRUnichar * aPromptMessage, const
                                       PRUnichar *aPromptTitle, 
                                       nsIMsgWindow* aMsgWindow,
                                       char **aPassword) 
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPassword);

    if (m_password.IsEmpty()) {
        nsCOMPtr<nsINetPrompt> dialog;
        // aMsgWindow is required if we need to prompt
        if (aMsgWindow)
        {
            // prompt the user for the password
            nsCOMPtr<nsIWebShell> webShell;
            rv = aMsgWindow->GetRootWebShell(getter_AddRefs(webShell));
            if (NS_FAILED(rv)) return rv;
            // get top level window
            nsCOMPtr<nsIWebShellContainer> topLevelWindow;
            rv = webShell->GetTopLevelWindow(getter_AddRefs(topLevelWindow));
            if (NS_FAILED(rv)) return rv;
            dialog =  do_QueryInterface( topLevelWindow, &rv );
        }
        else
        {
            NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID,
                            &rv);
            if (NS_SUCCEEDED(rv))
            {
                nsCOMPtr<nsIXULWindow> hiddenWindow;
                rv = appShell->GetHiddenWindow(getter_AddRefs(hiddenWindow));
                if (NS_SUCCEEDED(rv))
                    dialog = do_QueryInterface(hiddenWindow, &rv);
            }
        }
		if (NS_SUCCEEDED(rv) && dialog)
		{
            nsXPIDLString uniPassword;
			PRBool okayValue = PR_TRUE;
			nsXPIDLCString serverUri;
			rv = GetServerURI(getter_Copies(serverUri));
			if (NS_FAILED(rv)) return rv;
			rv = dialog->PromptPassword(serverUri, PR_FALSE, aPromptTitle, aPromptMessage, getter_Copies(uniPassword), &okayValue);
            		if (NS_FAILED(rv)) return rv;
				
			if (!okayValue) // if the user pressed cancel, just return NULL;
			{
				*aPassword = nsnull;
				return rv;
			}

			// we got a password back...so remember it
			nsCString aCStr(uniPassword); 

			rv = SetPassword((const char *) aCStr);
            if (NS_FAILED(rv)) return rv;
		} // if we got a prompt dialog
	} // if the password is empty

    rv = GetPassword(aPassword);
	return rv;
}

nsresult
nsMsgIncomingServer::StorePassword()
{
    nsresult rv;

    nsXPIDLCString pwd;
    rv = GetPassword(getter_Copies(pwd));
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIWalletService, walletservice, kWalletServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString serverUri;
    rv = GetServerURI(getter_Copies(serverUri));
    if (NS_FAILED(rv)) return rv;

    nsAutoString password = (const char *)pwd;
    rv = walletservice->SI_StorePassword((const char *)serverUri, PR_FALSE, nsnull, password.GetUnicode());
    return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::ForgetPassword()
{
    nsresult rv;
    NS_WITH_SERVICE(nsIWalletService, walletservice, kWalletServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    
    nsXPIDLCString serverUri;
    rv = GetServerURI(getter_Copies(serverUri));
    if (NS_FAILED(rv)) return rv;

    rv = SetPassword("");
    if (NS_FAILED(rv)) return rv;
    
    
    rv = walletservice->SI_RemoveUser((const char *)serverUri, PR_FALSE, nsnull);
    return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetDefaultLocalPath(nsIFileSpec *aDefaultLocalPath)
{
    nsresult rv;
    nsXPIDLCString type;
    GetType(getter_Copies(type));

    nsCAutoString progid(NS_MSGPROTOCOLINFO_PROGID_PREFIX);
    progid.Append(type);

    NS_WITH_SERVICE(nsIMsgProtocolInfo, protocolInfo, progid, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = protocolInfo->SetDefaultLocalPath(aDefaultLocalPath);
    return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetLocalPath(nsIFileSpec **aLocalPath)
{
    nsresult rv;

    // if the local path has already been set, use it
    rv = GetFileValue("directory", aLocalPath);
    if (NS_SUCCEEDED(rv) && *aLocalPath) return rv;
    
    // otherwise, create the path using.  note we are using the
    // hostname, unless that directory exists.
	// this should prevent all collisions.
    nsXPIDLCString type;
    GetType(getter_Copies(type));

    nsCAutoString progid(NS_MSGPROTOCOLINFO_PROGID_PREFIX);
    progid.Append(type);

    NS_WITH_SERVICE(nsIMsgProtocolInfo, protocolInfo, progid, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIFileSpec> path;
    rv = protocolInfo->GetDefaultLocalPath(getter_AddRefs(path));
    if (NS_FAILED(rv)) return rv;
    
    path->CreateDir();
    
	// set the leaf name to "dummy", and then call MakeUnique with a suggested leaf name
    rv = path->AppendRelativeUnixPath("dummy");
    if (NS_FAILED(rv)) return rv;
	nsXPIDLCString hostname;
    rv = GetHostName(getter_Copies(hostname));
    if (NS_FAILED(rv)) return rv;
	rv = path->MakeUniqueWithSuggestedName((const char *)hostname);
    if (NS_FAILED(rv)) return rv;

    rv = SetLocalPath(path);
    if (NS_FAILED(rv)) return rv;

    *aLocalPath = path;
    NS_ADDREF(*aLocalPath);

    return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetLocalPath(nsIFileSpec *spec)
{
    if (spec) {
        spec->CreateDir();
        return SetFileValue("directory", spec);
    }
    else {
        return NS_ERROR_NULL_POINTER;
    }
}

NS_IMETHODIMP
nsMsgIncomingServer::SetRememberPassword(PRBool value)
{
    if (!value) {
        ForgetPassword();
    }
    else {
	StorePassword();
    }
    return SetBoolValue("remember_password", value);
}

NS_IMETHODIMP
nsMsgIncomingServer::GetRememberPassword(PRBool* value)
{
    NS_ENSURE_ARG_POINTER(value);

    return GetBoolValue("remember_password", value);
}

NS_IMETHODIMP
nsMsgIncomingServer::GetLocalStoreType(char **aResult)
{
    NS_NOTYETIMPLEMENTED("nsMsgIncomingServer superclass not implementing GetLocalStoreType!");
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsMsgIncomingServer::Equals(nsIMsgIncomingServer *server, PRBool *_retval)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(server);
    NS_ENSURE_ARG_POINTER(_retval);

    nsXPIDLCString key1;
    nsXPIDLCString key2;

    rv = GetKey(getter_Copies(key1));
    if (NS_FAILED(rv)) return rv;

    rv = server->GetKey(getter_Copies(key2));
    if (NS_FAILED(rv)) return rv;

    // compare the server keys
    if (PL_strcmp((const char *)key1,(const char *)key2)) {
#ifdef DEBUG_MSGINCOMING_SERVER
        printf("%s and %s are different, servers are not the same\n",(const char *)key1,(const char *)key2);
#endif /* DEBUG_MSGINCOMING_SERVER */
        *_retval = PR_FALSE;
    }
    else {
#ifdef DEBUG_MSGINCOMING_SERVER
        printf("%s and %s are equal, servers are the same\n",(const char *)key1,(const char *)key2);
#endif /* DEBUG_MSGINCOMING_SERVER */
        *_retval = PR_TRUE;
    }
    return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::ClearAllValues()
{
    nsresult rv;
    nsCAutoString rootPref("mail.server.");
    rootPref += m_serverKey;

    rv = m_prefs->EnumerateChildren(rootPref, clearPrefEnum, (void *)m_prefs);

    return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::RemoveFiles()
{
	// this is not ready for prime time.  the problem is that if files are in use, they won't
	// get deleted properly.  for example, when we get here, we may have .msf files in open
	// and in use.  I need to think about this some more.
#if 0
#ifdef DEBUG_MSGINCOMING_SERVER
	printf("remove files for %s\n", (const char *)m_serverKey);
#endif /* DEBUG_MSGINCOMING_SERVER */
	nsresult rv = NS_OK;
	nsCOMPtr <nsIFileSpec> localPath;
	rv = GetLocalPath(getter_AddRefs(localPath));
	if (NS_FAILED(rv)) return rv;
	
	if (!localPath) return NS_ERROR_FAILURE;
	
	PRBool exists = PR_FALSE;
	rv = localPath->Exists(&exists);
	if (NS_FAILED(rv)) return rv;

	// if it doesn't exist, that's ok.
	if (!exists) return NS_OK;

	rv = localPath->Delete(PR_TRUE /* recursive */);
	if (NS_FAILED(rv)) return rv;

	// now check if it really gone
	rv = localPath->Exists(&exists);
	if (NS_FAILED(rv)) return rv;

	// if it still exists, something failed.
	if (exists) return NS_ERROR_FAILURE;
#endif /* 0 */
	return NS_OK;
}


void
nsMsgIncomingServer::clearPrefEnum(const char *aPref, void *aClosure)
{
    nsIPref *prefs = (nsIPref *)aClosure;
    prefs->ClearUserPref(aPref);
}

nsresult
nsMsgIncomingServer::GetFilterList(nsIMsgFilterList **aResult)
{

  nsresult rv;
  
  if (!mFilterList) {
      nsCOMPtr<nsIFolder> folder;
      rv = GetRootFolder(getter_AddRefs(folder));

      nsCOMPtr<nsIMsgFolder> msgFolder(do_QueryInterface(folder, &rv));
      NS_ENSURE_SUCCESS(rv, rv);
      
      nsCOMPtr<nsIFileSpec> thisFolder;
      rv = msgFolder->GetPath(getter_AddRefs(thisFolder));
      NS_ENSURE_SUCCESS(rv, rv);
      
      nsFileSpec filterFile;
      
      rv = thisFolder->GetFileSpec(&filterFile);
      NS_ENSURE_SUCCESS(rv, rv);
      
      filterFile += "rules.dat";
      
      nsCOMPtr<nsIMsgFilterService> filterService =
          do_GetService(kMsgFilterServiceCID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = filterService->OpenFilterList(&filterFile, msgFolder, getter_AddRefs(mFilterList));
      NS_ENSURE_SUCCESS(rv, rv);
  }
  
  *aResult = mFilterList;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
    
}    


// use the convenience macros to implement the accessors
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, HostName, "hostname");
NS_IMPL_SERVERPREF_INT(nsMsgIncomingServer, Port, "port");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, Username, "userName");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, PrefPassword, "password");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, DoBiff, "check_new_mail");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, IsSecure, "isSecure");
NS_IMPL_SERVERPREF_INT(nsMsgIncomingServer, BiffMinutes, "check_time");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, Type, "type");
// in 4.x, this was "mail.pop3_gets_new_mail" for pop and 
// "mail.imap.new_mail_get_headers" for imap (it was global)
// in 5.0, this will be per server, and it will be "download_on_biff"
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, DownloadOnBiff, "download_on_biff");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, Valid, "valid");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, RedirectorType,  "redirector_type");
