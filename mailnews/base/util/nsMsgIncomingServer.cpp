/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsMsgIncomingServer.h"
#include "nscore.h"
#include "nsCom.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"

#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "nsCOMPtr.h"
#include "nsIMsgFolder.h"
#include "nsIMsgFolderCache.h"
#include "nsIMsgFolderCacheElement.h"
#include "nsXPIDLString.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsMsgIncomingServer::nsMsgIncomingServer():
    m_prefs(0),
    m_serverKey(0),
    m_rootFolder(0)
{
  NS_INIT_REFCNT();
}

nsMsgIncomingServer::~nsMsgIncomingServer()
{
    if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID,
                                                  m_prefs,
                                                  nsnull);
    PR_FREEIF(m_serverKey)
}

NS_IMPL_ISUPPORTS(nsMsgIncomingServer, GetIID());


NS_IMPL_GETTER_STR(nsMsgIncomingServer::GetKey, m_serverKey)


NS_IMETHODIMP
nsMsgIncomingServer::SetKey(char * serverKey)
{
    nsresult rv = NS_OK;
    // in order to actually make use of the key, we need the prefs
    if (!m_prefs)
        rv = nsServiceManager::GetService(kPrefServiceCID,
                                          nsCOMTypeInfo<nsIPref>::GetIID(),
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
			CreateRootFolder();
			*aRootFolder = m_rootFolder;
      NS_IF_ADDREF(*aRootFolder);
    }
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

void nsMsgIncomingServer::CreateRootFolder()
{
	nsresult rv;
			  // get the URI from the incoming server
  nsXPIDLCString serverUri;
  rv = GetServerURI(getter_Copies(serverUri));
  if (NS_FAILED(rv)) return ;

  NS_WITH_SERVICE(nsIRDFService, rdf,
                        kRDFServiceCID, &rv);

  // get the corresponding RDF resource
  // RDF will create the server resource if it doesn't already exist
  nsCOMPtr<nsIRDFResource> serverResource;
  rv = rdf->GetResource(serverUri, getter_AddRefs(serverResource));
  if (NS_FAILED(rv)) return;

  // make incoming server know about its root server folder so we 
  // can find sub-folders given an incoming server.
  m_rootFolder = do_QueryInterface(serverResource);
}

char *
nsMsgIncomingServer::getPrefName(const char *serverKey,
                                 const char *fullPrefName)
{
  return PR_smprintf("mail.server.%s.%s", serverKey, fullPrefName);
}

// this will be slightly faster than the above, and allows
// the "default" server preference root to be set in one place
char *
nsMsgIncomingServer::getDefaultPrefName(const char *fullPrefName)
{
  return PR_smprintf("mail.server.default.%s", fullPrefName);
}


nsresult
nsMsgIncomingServer::getBoolPref(const char *prefname,
                                 PRBool *val)
{
  char *fullPrefName = getPrefName(m_serverKey, prefname);
  nsresult rv = m_prefs->GetBoolPref(fullPrefName, val);
  PR_Free(fullPrefName);
  
  if (NS_FAILED(rv))
    rv = getDefaultBoolPref(prefname, val);
  
  return rv;
}

nsresult
nsMsgIncomingServer::getDefaultBoolPref(const char *prefname,
                                        PRBool *val) {
  
  char *fullPrefName = getDefaultPrefName(prefname);
  nsresult rv = m_prefs->GetBoolPref(fullPrefName, val);
  PR_Free(fullPrefName);

  if (NS_FAILED(rv)) {
    *val = PR_FALSE;
    rv = NS_OK;
  }
  return rv;
}

nsresult
nsMsgIncomingServer::setBoolPref(const char *prefname,
                                 PRBool val)
{
  nsresult rv;
  char *fullPrefName = getPrefName(m_serverKey, prefname);

  PRBool defaultValue;
  rv = getDefaultBoolPref(prefname, &defaultValue);

  if (NS_SUCCEEDED(rv) &&
      val == defaultValue)
    rv = m_prefs->ClearUserPref(fullPrefName);
  else
    rv = m_prefs->SetBoolPref(fullPrefName, val);
  
  PR_Free(fullPrefName);
  
  return rv;
}

nsresult
nsMsgIncomingServer::getIntPref(const char *prefname,
                                PRInt32 *val)
{
  char *fullPrefName = getPrefName(m_serverKey, prefname);
  nsresult rv = m_prefs->GetIntPref(fullPrefName, val);
  PR_Free(fullPrefName);

  if (NS_FAILED(rv))
    rv = getDefaultIntPref(prefname, val);
  
  return rv;
}

nsresult
nsMsgIncomingServer::getDefaultIntPref(const char *prefname,
                                        PRInt32 *val) {
  
  char *fullPrefName = getDefaultPrefName(prefname);
  nsresult rv = m_prefs->GetIntPref(fullPrefName, val);
  PR_Free(fullPrefName);

  if (NS_FAILED(rv)) {
    *val = 0;
    rv = NS_OK;
  }
  
  return rv;
}

nsresult
nsMsgIncomingServer::setIntPref(const char *prefname,
                                 PRInt32 val)
{
  nsresult rv;
  char *fullPrefName = getPrefName(m_serverKey, prefname);
  
  PRInt32 defaultVal;
  rv = getDefaultIntPref(prefname, &defaultVal);
  
  if (NS_SUCCEEDED(rv) && defaultVal == val)
    rv = m_prefs->ClearUserPref(fullPrefName);
  else
    rv = m_prefs->SetIntPref(fullPrefName, val);
  
  PR_Free(fullPrefName);
  
  return rv;
}

nsresult
nsMsgIncomingServer::getCharPref(const char *prefname,
                                 char  **val)
{
  char *fullPrefName = getPrefName(m_serverKey, prefname);
  nsresult rv = m_prefs->CopyCharPref(fullPrefName, val);
  PR_Free(fullPrefName);
  
  if (NS_FAILED(rv))
    rv = getDefaultCharPref(prefname, val);
  
  return rv;
}

nsresult
nsMsgIncomingServer::getDefaultCharPref(const char *prefname,
                                        char **val) {
  
  char *fullPrefName = getDefaultPrefName(prefname);
  nsresult rv = m_prefs->CopyCharPref(fullPrefName, val);
  PR_Free(fullPrefName);

  if (NS_FAILED(rv)) {
    *val = nsnull;              // null is ok to return here
    rv = NS_OK;
  }
  return rv;
}

nsresult
nsMsgIncomingServer::setCharPref(const char *prefname,
                                 const char * val)
{
  nsresult rv;
  char *fullPrefName = getPrefName(m_serverKey, prefname);

  char *defaultVal=nsnull;
  rv = getDefaultCharPref(prefname, &defaultVal);
  if (NS_SUCCEEDED(rv) &&
      PL_strcmp(defaultVal, val) == 0)
    rv = m_prefs->ClearUserPref(fullPrefName);
  else
    rv = m_prefs->SetCharPref(fullPrefName, val);
  
  PR_FREEIF(defaultVal);
  PR_Free(fullPrefName);
  
  return rv;
}

// pretty name is the display name to show to the user
NS_IMETHODIMP
nsMsgIncomingServer::GetPrettyName(PRUnichar **retval) {

  char *val=nsnull;
  nsresult rv = getCharPref("name", &val);
  if (NS_FAILED(rv)) return rv;

  nsString prettyName;
  
  // if there's no name, then just return the hostname
  if (val) {
    prettyName = val;
  } else {
    
    nsXPIDLCString username;
    rv = GetUsername(getter_Copies(username));
    if (NS_FAILED(rv)) return rv;
    if ((const char*)username &&
        PL_strcmp((const char*)username, "")!=0) {
      prettyName = username;
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
nsMsgIncomingServer::SetPrettyName(PRUnichar *value) {
  // this is lossy. Not sure what to do.
  nsCString str(value);
  return setCharPref("name", str.GetBuffer());
}

// use the convenience macros to implement the accessors
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, HostName, "hostname");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, Username, "userName");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, Password, "password");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, DoBiff, "check_new_mail");
NS_IMPL_SERVERPREF_INT(nsMsgIncomingServer, BiffMinutes, "check_time");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, RememberPassword, "remember_password");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, LocalPath, "directory");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, Type, "type");

/* what was this called in 4.x? */
// pref("mail.pop3_gets_new_mail",				true);

NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, DownloadOnBiff, "download_on_biff");
