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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsMsgIncomingServer.h"
#include "nscore.h"
#include "nsCom.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"

#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsEscape.h"

#include "nsMsgBaseCID.h"
#include "nsMsgDBCID.h"
#include "nsIMsgFolder.h"
#include "nsIMsgFolderCache.h"
#include "nsIMsgFolderCacheElement.h"
#include "nsIMsgWindow.h"
#include "nsIMsgFilterService.h"
#include "nsIMsgProtocolInfo.h"

#include "nsIPref.h"
#include "nsIDocShell.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIAuthPrompt.h"
#include "nsIWalletService.h"
#include "nsIWindowWatcher.h"
#include "nsIStringBundle.h"

#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#ifdef DEBUG_sspitzer
#define DEBUG_MSGINCOMING_SERVER
#endif /* DEBUG_sspitzer */

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kWalletServiceCID, NS_WALLETSERVICE_CID);
static NS_DEFINE_CID(kMsgFilterServiceCID, NS_MSGFILTERSERVICE_CID);

#define OFFLINE_STATUS_CHANGED_TOPIC "network:offline-status-changed"

#define PORT_NOT_SET -1

MOZ_DECL_CTOR_COUNTER(nsMsgIncomingServer)

nsMsgIncomingServer::nsMsgIncomingServer():
    m_prefs(0),
    m_serverBusy(PR_FALSE),
    m_rootFolder(0),
    m_canHaveFilters(PR_FALSE),
    m_displayStartupPage(PR_TRUE)
{
  NS_INIT_REFCNT();
}

nsMsgIncomingServer::~nsMsgIncomingServer()
{
    if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID,
                                                  m_prefs,
                                                  nsnull);
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

    m_serverKey.Assign(serverKey);
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
nsMsgIncomingServer::PerformExpand(nsIMsgWindow *aMsgWindow)
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
			rv = msgFolder->WriteToFolderCache(folderCache, PR_TRUE /* deep */);
	}
	return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::CloseCachedConnections()
{
	// derived class should override if they cache connections.
	return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetDownloadMessagesAtStartup(PRBool *getMessagesAtStartup)
{
    // derived class should override if they need to do this.
    *getMessagesAtStartup = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetCanHaveFilters(PRBool *canHaveFilters)
{
    // derived class should override if they need to do this.
    *canHaveFilters = m_canHaveFilters;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetCanHaveFilters(PRBool canHaveFilters)
{
    // derived class should override if they need to do this.
    m_canHaveFilters = canHaveFilters;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetCanBeDefaultServer(PRBool *canBeDefaultServer)
{
    // derived class should override if they need to do this.
    *canBeDefaultServer = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetCanSearchMessages(PRBool *canSearchMessages)
{
    // derived class should override if they need to do this.
    NS_ENSURE_ARG_POINTER(canSearchMessages);
    *canSearchMessages = PR_FALSE;
    return NS_OK;
}

// construct <localStoreType>://[<username>@]<hostname
NS_IMETHODIMP
nsMsgIncomingServer::GetServerURI(char* *aResult)
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

  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));

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
  
  nsCOMPtr<nsILocalFile> prefLocal;
  nsCOMPtr<nsIFileSpec> outSpec;
  nsXPIDLCString pathBuf;
  
  nsresult rv = m_prefs->GetFileXPref(fullPrefName, getter_AddRefs(prefLocal));
  if (NS_FAILED(rv)) return rv;
  rv = NS_NewFileSpec(getter_AddRefs(outSpec));
  if (NS_FAILED(rv)) return rv;
  rv = prefLocal->GetPath(getter_Copies(pathBuf));
  if (NS_FAILED(rv)) return rv;
  rv = outSpec->SetNativePath((const char *)pathBuf);
  if (NS_FAILED(rv)) return rv;
  
  *spec = outSpec;
  NS_ADDREF(*spec);

  return NS_OK;
}

nsresult
nsMsgIncomingServer::SetFileValue(const char* prefname,
                                    nsIFileSpec *spec)
{
  nsCAutoString fullPrefName;
  getPrefName(m_serverKey, prefname, fullPrefName);
  
  nsresult rv;
  nsFileSpec tempSpec;
  nsCOMPtr<nsILocalFile> prefLocal;
  
  rv = spec->GetFileSpec(&tempSpec);
  if (NS_FAILED(rv)) return rv;
  rv = NS_FileSpecToIFile(&tempSpec, getter_AddRefs(prefLocal));
  if (NS_FAILED(rv)) return rv;
  rv = m_prefs->SetFileXPref(fullPrefName, prefLocal);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
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

  // if there's no name, then just return the hostname
  if (nsCRT::strlen(val) == 0) 
    return GetConstructedPrettyName(retval);
  else
    *retval = nsCRT::strdup(val);
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


// construct the pretty name to show to the user if they haven't
// specified one. This should be overridden for news and mail.
NS_IMETHODIMP
nsMsgIncomingServer::GetConstructedPrettyName(PRUnichar **retval) 
{
    
  nsXPIDLCString username;
  nsAutoString prettyName;
  nsresult rv = GetUsername(getter_Copies(username));
  if (NS_FAILED(rv)) return rv;
  if ((const char*)username &&
      PL_strcmp((const char*)username, "")!=0) {
    prettyName.AssignWithConversion(username);
    prettyName.AppendWithConversion(" on ");
  }
  
  nsXPIDLCString hostname;
  rv = GetHostName(getter_Copies(hostname));
  if (NS_FAILED(rv)) return rv;


  prettyName.AppendWithConversion(hostname);

  *retval = prettyName.ToNewUnicode();
  
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::ToString(PRUnichar** aResult) {
  nsString servername; servername.AssignWithConversion("[nsIMsgIncomingServer: ");
  servername.AppendWithConversion(m_serverKey);
  servername.AppendWithConversion("]");
  
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

NS_IMETHODIMP nsMsgIncomingServer::GetServerRequiresPasswordForBiff(PRBool *_retval)
{
    if (!_retval) return NS_ERROR_NULL_POINTER;
	*_retval = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetPasswordWithUI(const PRUnichar * aPromptMessage, const
                                       PRUnichar *aPromptTitle, 
                                       nsIMsgWindow* aMsgWindow,
                                       PRBool *okayValue,
                                       char **aPassword) 
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPassword);
    NS_ENSURE_ARG_POINTER(okayValue);

    if (m_password.IsEmpty()) {
        nsCOMPtr<nsIAuthPrompt> dialog;
        // aMsgWindow is required if we need to prompt
        if (aMsgWindow)
        {
            // prompt the user for the password
            nsCOMPtr<nsIDocShell> docShell;
            rv = aMsgWindow->GetRootDocShell(getter_AddRefs(docShell));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell, &rv));
            if (NS_FAILED(rv)) return rv;
            dialog = do_GetInterface(webShell, &rv);
			if (NS_FAILED(rv)) return rv;
        }
        else
        {
          nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
          if (wwatch)
            wwatch->GetNewAuthPrompter(0, getter_AddRefs(dialog));
          if (!dialog) return NS_ERROR_FAILURE;
        }
		if (NS_SUCCEEDED(rv) && dialog)
		{
            nsXPIDLString uniPassword;
			nsXPIDLCString serverUri;
			rv = GetServerURI(getter_Copies(serverUri));
			if (NS_FAILED(rv)) return rv;
			rv = dialog->PromptPassword(aPromptTitle, aPromptMessage, 
                                        NS_ConvertASCIItoUCS2(serverUri).get(), nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                        getter_Copies(uniPassword), okayValue);
            if (NS_FAILED(rv)) return rv;
				
			if (!*okayValue) // if the user pressed cancel, just return NULL;
			{
				*aPassword = nsnull;
				return rv;
			}

			// we got a password back...so remember it
			nsCString aCStr; aCStr.AssignWithConversion(uniPassword); 

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

    nsCOMPtr<nsIWalletService> walletservice = 
             do_GetService(kWalletServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString serverUri;
    rv = GetServerURI(getter_Copies(serverUri));
    if (NS_FAILED(rv)) return rv;

    nsAutoString password; password.AssignWithConversion((const char *)pwd);
    rv = walletservice->SI_StorePassword((const char *)serverUri, nsnull, password.get());
    return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::ForgetPassword()
{
    nsresult rv;
    nsCOMPtr<nsIWalletService> walletservice = 
             do_GetService(kWalletServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    
    nsXPIDLCString serverUri;
    rv = GetServerURI(getter_Copies(serverUri));
    if (NS_FAILED(rv)) return rv;

    rv = SetPassword("");
    if (NS_FAILED(rv)) return rv;
    
    
    rv = walletservice->SI_RemoveUser((const char *)serverUri, nsnull);
    return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::ForgetSessionPassword()
{
    m_password.Truncate(0);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetDefaultLocalPath(nsIFileSpec *aDefaultLocalPath)
{
    nsresult rv;
    nsCOMPtr<nsIMsgProtocolInfo> protocolInfo;
    rv = getProtocolInfo(getter_AddRefs(protocolInfo));
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
    
    // otherwise, create the path using the protocol info.
    // note we are using the
    // hostname, unless that directory exists.
	// this should prevent all collisions.
    nsCOMPtr<nsIMsgProtocolInfo> protocolInfo;
    rv = getProtocolInfo(getter_AddRefs(protocolInfo));
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
        // IMPORTANT, see bug #77652
        // don't turn this code on yet.  we don't inform the user that
	// we are going to be deleting the directory, and they might have
	// tweaked their localPath pref for this server to point to 
	// somewhere they didn't want deleted.
        // until we tell them, we shouldn't do the delete.
#if 0
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

      mFilterFile = do_CreateInstance(NS_FILESPEC_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mFilterFile->FromFileSpec(thisFolder);
      NS_ENSURE_SUCCESS(rv, rv);

      mFilterFile->AppendRelativeUnixPath("rules.dat");
      
      nsCOMPtr<nsIMsgFilterService> filterService =
          do_GetService(kMsgFilterServiceCID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = filterService->OpenFilterList(mFilterFile, msgFolder, getter_AddRefs(mFilterList));
      NS_ENSURE_SUCCESS(rv, rv);
  }
  
  *aResult = mFilterList;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
    
}
         
// if the user has a : appended, then
nsresult
nsMsgIncomingServer::SetHostName(const char *aHostname)
{
    nsresult rv;
    if (PL_strchr(aHostname, ':')) {
	nsCAutoString newHostname(aHostname);
	PRInt32 colonPos = newHostname.FindChar(':');

        nsCAutoString portString;
        newHostname.Right(portString, newHostname.Length() - colonPos);

        newHostname.Truncate(colonPos);
        
	PRInt32 err;
        PRInt32 port = portString.ToInteger(&err);
        if (!err) SetPort(port);

	rv = SetCharValue("hostname", (const char*)newHostname);
    } else {
        rv = SetCharValue("hostname", aHostname);
    }
    return rv;
}

nsresult
nsMsgIncomingServer::GetHostName(char **aResult)
{
    nsresult rv;
    rv = GetCharValue("hostname", aResult);
    if (PL_strchr(*aResult, ':')) {
	// gack, we need to reformat the hostname - SetHostName will do that
        SetHostName(*aResult);
        rv = GetCharValue("hostname", aResult);
    }
    return rv;
}

#define BIFF_PREF_NAME "check_new_mail"

NS_IMETHODIMP
nsMsgIncomingServer::GetDoBiff(PRBool *aDoBiff)
{
    NS_ENSURE_ARG_POINTER(aDoBiff);
    nsresult rv;
   
    nsCAutoString fullPrefName;
    getPrefName(m_serverKey, BIFF_PREF_NAME, fullPrefName);
    rv = m_prefs->GetBoolPref(fullPrefName, aDoBiff);
    if (NS_SUCCEEDED(rv)) return rv;

    // if the pref isn't set, use the default
    // value based on the protocol
    nsCOMPtr<nsIMsgProtocolInfo> protocolInfo;

    rv = getProtocolInfo(getter_AddRefs(protocolInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = protocolInfo->GetDefaultDoBiff(aDoBiff);
    // note, don't call SetDoBiff()
    // since we keep changing our minds on
    // if biff should be on or off, let's keep the ability
    // to change the default in future builds.
    // if we call SetDoBiff() here, it will be in the users prefs.
    // and we can't do anything after that.
    return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetDoBiff(PRBool aDoBiff)
{
    nsresult rv;
    nsCAutoString fullPrefName;
    getPrefName(m_serverKey, BIFF_PREF_NAME, fullPrefName);

    rv = m_prefs->SetBoolPref(fullPrefName, aDoBiff);
    NS_ENSURE_SUCCESS(rv,rv);
    return NS_OK;
}


NS_IMETHODIMP
nsMsgIncomingServer::GetPort(PRInt32 *aPort)
{
    NS_ENSURE_ARG_POINTER(aPort);
    nsresult rv;
    
    rv = GetIntValue("port", aPort);
    if (*aPort != PORT_NOT_SET) return rv;
    
    // if the port isn't set, use the default
    // port based on the protocol
    nsCOMPtr<nsIMsgProtocolInfo> protocolInfo;

    rv = getProtocolInfo(getter_AddRefs(protocolInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool isSecure = PR_FALSE;
    // Try this, and if it fails, fall back to the non-secure port
    GetIsSecure(&isSecure);
    return protocolInfo->GetDefaultServerPort(isSecure, aPort);
}

NS_IMETHODIMP
nsMsgIncomingServer::SetPort(PRInt32 aPort)
{
    nsresult rv;
 
    nsCOMPtr<nsIMsgProtocolInfo> protocolInfo;
    rv = getProtocolInfo(getter_AddRefs(protocolInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 defaultPort;
    // First param is set to FALSE so that the non-secure
    // default port is returned
    rv = protocolInfo->GetDefaultServerPort(PR_FALSE, &defaultPort);
    if (NS_SUCCEEDED(rv) && aPort == defaultPort)
        // clear it out by setting it to the default
        rv = SetIntValue("port", PORT_NOT_SET);
    else
        rv = SetIntValue("port", aPort);

    return NS_OK;
}

nsresult
nsMsgIncomingServer::getProtocolInfo(nsIMsgProtocolInfo **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    nsresult rv;

    nsXPIDLCString type;
    rv = GetType(getter_Copies(type));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString contractid(NS_MSGPROTOCOLINFO_CONTRACTID_PREFIX);
    contractid.Append(type);

    nsCOMPtr<nsIMsgProtocolInfo> protocolInfo =
        do_GetService(contractid, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    *aResult = protocolInfo;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP nsMsgIncomingServer::GetRetentionSettings(nsIMsgRetentionSettings **settings)
{
  NS_ENSURE_ARG_POINTER(settings);
  nsMsgRetainByPreference retainByPreference;
  PRInt32 daysToKeepHdrs = 0;
  PRInt32 numHeadersToKeep = 0;
  PRBool keepUnreadMessagesOnly = PR_FALSE;
  PRInt32 daysToKeepBodies = 0;
  PRBool cleanupBodiesByDays = PR_FALSE;
  nsresult rv = NS_OK;
  if (!m_retentionSettings)
  {
    m_retentionSettings = do_CreateInstance(NS_MSG_RETENTIONSETTINGS_CONTRACTID);
    if (m_retentionSettings)
    {
      rv = GetBoolValue("keepUnreadOnly", &keepUnreadMessagesOnly);
      rv = GetIntValue("retainBy", (PRInt32*) &retainByPreference);
      rv = GetIntValue("numHdrsToKeep", &numHeadersToKeep);
      rv = GetIntValue("daysToKeepHdrs", &daysToKeepHdrs);
      rv = GetIntValue("daysToKeepBodies", &daysToKeepBodies);
      rv = GetBoolValue("cleanupBodies", &cleanupBodiesByDays);
      m_retentionSettings->SetRetainByPreference(retainByPreference);
      m_retentionSettings->SetNumHeadersToKeep((PRUint32) numHeadersToKeep);
      m_retentionSettings->SetKeepUnreadMessagesOnly(keepUnreadMessagesOnly);
      m_retentionSettings->SetDaysToKeepBodies(daysToKeepBodies);
      m_retentionSettings->SetDaysToKeepHdrs(daysToKeepHdrs);
      m_retentionSettings->SetCleanupBodiesByDays(cleanupBodiesByDays);
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    // Create an empty retention settings object, 
    // get the settings from the server prefs, and init the object from the prefs.
  }
  *settings = m_retentionSettings;
  NS_IF_ADDREF(*settings);  return rv;
}

NS_IMETHODIMP nsMsgIncomingServer::SetRetentionSettings(nsIMsgRetentionSettings *settings)
{
  nsMsgRetainByPreference retainByPreference;
  PRUint32 daysToKeepHdrs = 0;
  PRUint32 numHeadersToKeep = 0;
  PRBool keepUnreadMessagesOnly = PR_FALSE;
  PRUint32 daysToKeepBodies = 0;
  PRBool cleanupBodiesByDays = PR_FALSE;
  m_retentionSettings = settings;
  m_retentionSettings->GetRetainByPreference(&retainByPreference);
  m_retentionSettings->GetNumHeadersToKeep(&numHeadersToKeep);
  m_retentionSettings->GetKeepUnreadMessagesOnly(&keepUnreadMessagesOnly);
  m_retentionSettings->GetDaysToKeepBodies(&daysToKeepBodies);
  m_retentionSettings->GetDaysToKeepHdrs(&daysToKeepHdrs);
  m_retentionSettings->GetCleanupBodiesByDays(&cleanupBodiesByDays);
  nsresult rv = SetBoolValue("keepUnreadOnly", keepUnreadMessagesOnly);
  rv = SetIntValue("retainBy", retainByPreference);
  rv = SetIntValue("numHdrsToKeep", numHeadersToKeep);
  rv = SetIntValue("daysToKeepHdrs", daysToKeepHdrs);
  rv = SetIntValue("daysToKeepBodies", daysToKeepBodies);
  rv = SetBoolValue("cleanupBodies", cleanupBodiesByDays);
  return rv;
}
 
NS_IMETHODIMP
nsMsgIncomingServer::GetDisplayStartupPage(PRBool *displayStartupPage)
{
    NS_ENSURE_ARG_POINTER(displayStartupPage);
    *displayStartupPage = m_displayStartupPage;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetDisplayStartupPage(PRBool displayStartupPage)
{
    m_displayStartupPage = displayStartupPage;
    return NS_OK;
}


NS_IMETHODIMP nsMsgIncomingServer::GetDownloadSettings(nsIMsgDownloadSettings **settings)
{
  NS_ENSURE_ARG_POINTER(settings);
  PRBool downloadUnreadOnly = PR_FALSE;
  PRBool downloadByDate = PR_FALSE;
  PRUint32 ageLimitOfMsgsToDownload = 0;
  nsresult rv = NS_OK;
  if (!m_downloadSettings)
  {
    m_downloadSettings = do_CreateInstance(NS_MSG_DOWNLOADSETTINGS_CONTRACTID);
    if (m_downloadSettings)
    {
      rv = GetBoolValue("downloadUnreadOnly", &downloadUnreadOnly);
      rv = GetBoolValue("downloadByDate", &downloadByDate);
      rv = GetIntValue("ageLimit", (PRInt32 *) &ageLimitOfMsgsToDownload);
      m_downloadSettings->SetDownloadUnreadOnly(downloadUnreadOnly);
      m_downloadSettings->SetDownloadByDate(downloadByDate);
      m_downloadSettings->SetAgeLimitOfMsgsToDownload(ageLimitOfMsgsToDownload);
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    // Create an empty download settings object, 
    // get the settings from the server prefs, and init the object from the prefs.
  }
  *settings = m_downloadSettings;
  NS_IF_ADDREF(*settings);  return rv;
}

NS_IMETHODIMP nsMsgIncomingServer::SetDownloadSettings(nsIMsgDownloadSettings *settings)
{
  m_downloadSettings = settings;
  PRBool downloadUnreadOnly = PR_FALSE;
  PRBool downloadByDate = PR_FALSE;
  PRUint32 ageLimitOfMsgsToDownload = 0;
  m_downloadSettings->GetDownloadUnreadOnly(&downloadUnreadOnly);
  m_downloadSettings->GetDownloadByDate(&downloadByDate);
  m_downloadSettings->GetAgeLimitOfMsgsToDownload(&ageLimitOfMsgsToDownload);
  nsresult rv = SetBoolValue("downloadUnreadOnly", downloadUnreadOnly);
  rv = SetBoolValue("downloadByDate", downloadByDate);
  rv = SetIntValue("ageLimit", ageLimitOfMsgsToDownload);

  return rv;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetSupportsDiskSpace(PRBool *aSupportsDiskSpace)
{
    NS_ENSURE_ARG_POINTER(aSupportsDiskSpace);
    *aSupportsDiskSpace = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetOfflineSupportLevel(PRInt32 *aSupportLevel)
{
    NS_ENSURE_ARG_POINTER(aSupportLevel);
    nsresult rv;
    
    rv = GetIntValue("offline_support_level", aSupportLevel);
    if (*aSupportLevel != OFFLINE_SUPPORT_LEVEL_UNDEFINED) return rv;
    
    // set default value
    *aSupportLevel = OFFLINE_SUPPORT_LEVEL_NONE;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::SetOfflineSupportLevel(PRInt32 aSupportLevel)
{
    SetIntValue("offline_support_level", aSupportLevel);
    return NS_OK;
}
#define BASE_MSGS_URL       "chrome://messenger/locale/messenger.properties"

NS_IMETHODIMP nsMsgIncomingServer::DisplayOfflineMsg(nsIMsgWindow *aMsgWindow)
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle(BASE_MSGS_URL, getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);
  if (bundle)
  {
    nsXPIDLString errorMsgTitle;
    nsXPIDLString errorMsgBody;

    bundle->GetStringFromName(NS_LITERAL_STRING("nocachedbodybody").get(), getter_Copies(errorMsgBody));
    bundle->GetStringFromName(NS_LITERAL_STRING("nocachedbodytitle").get(),  getter_Copies(errorMsgTitle));
    if (aMsgWindow)
      return aMsgWindow->DisplayHTMLInMessagePane(errorMsgTitle, errorMsgBody);
    else
      return NS_ERROR_FAILURE;

  }
  return rv;
}

// Called only during the migration process. A unique name is generated for the
// migrated account.
NS_IMETHODIMP
nsMsgIncomingServer::GeneratePrettyNameForMigration(PRUnichar **aPrettyName)
{
    /** 
     * 4.x had provisions for multiple imap servers to be maintained under
     * single identity. So, when migrated each of those server accounts need
     * to be represented by unique account name. nsImapIncomingServer will
     * override the implementation for this to do the right thing.
     */
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetFilterScope(nsMsgSearchScopeValue *filterScope)
{
   NS_ENSURE_ARG_POINTER(filterScope);

   *filterScope = nsMsgSearchScope::offlineMail;
   return NS_OK;
}

NS_IMETHODIMP
nsMsgIncomingServer::GetSearchScope(nsMsgSearchScopeValue *searchScope)
{
   NS_ENSURE_ARG_POINTER(searchScope);

   *searchScope = nsMsgSearchScope::offlineMail;
   return NS_OK;
}

// use the convenience macros to implement the accessors
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, Username, "userName");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, PrefPassword, "password");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, IsSecure, "isSecure");
NS_IMPL_SERVERPREF_INT(nsMsgIncomingServer, BiffMinutes, "check_time");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, Type, "type");
// in 4.x, this was "mail.pop3_gets_new_mail" for pop and 
// "mail.imap.new_mail_get_headers" for imap (it was global)
// in 5.0, this will be per server, and it will be "download_on_biff"
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, DownloadOnBiff, "download_on_biff");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, Valid, "valid");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, RedirectorType,  "redirector_type");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, EmptyTrashOnExit,
                        "empty_trash_on_exit");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, CanDelete, "canDelete");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, LoginAtStartUp, "login_at_startup");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, 
                        DefaultCopiesAndFoldersPrefsToServer, 
                        "allows_specialfolders_usage");

NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, 
                        CanCreateFoldersOnServer, 
                        "canCreateFolders");

NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, 
                        CanFileMessagesOnServer, 
                        "canFileMessages");

NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer,
                        LimitMessageSize,
                        "limit_message_size")

NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer,
      LimitOfflineMessageSize,
      "limit_offline_message_size")

NS_IMPL_SERVERPREF_INT(nsMsgIncomingServer, MaxMessageSize, "max_size")
