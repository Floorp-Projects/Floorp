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

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

nsMsgIncomingServer::nsMsgIncomingServer():
    m_prefs(0),
    m_serverKey(0)
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
                                          nsIPref::GetIID(),
                                          (nsISupports**)&m_prefs);

    PR_FREEIF(m_serverKey);
    m_serverKey = PL_strdup(serverKey);
    return rv;
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
  
  char *fullPrefName = getDefaultPrefName(m_serverKey);
  nsresult rv = m_prefs->GetBoolPref(fullPrefName, val);
  PR_Free(fullPrefName);

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
  
  char *fullPrefName = getDefaultPrefName(m_serverKey);
  nsresult rv = m_prefs->GetIntPref(fullPrefName, val);
  PR_Free(fullPrefName);

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
  
  char *fullPrefName = getDefaultPrefName(m_serverKey);
  nsresult rv = m_prefs->CopyCharPref(fullPrefName, val);
  PR_Free(fullPrefName);

  return rv;
}

nsresult
nsMsgIncomingServer::setCharPref(const char *prefname,
                                 char * val)
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


// use the convenience macros to implement the accessors
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, PrettyName, "name")
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, HostName, "hostname");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, UserName, "userName");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, Password, "password");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, DoBiff, "check_new_mail");
NS_IMPL_SERVERPREF_INT(nsMsgIncomingServer, BiffMinutes, "check_time");
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, RememberPassword, "remember_password");
NS_IMPL_SERVERPREF_STR(nsMsgIncomingServer, LocalPath, "directory")


/* what was this called in 4.x? */
NS_IMPL_SERVERPREF_BOOL(nsMsgIncomingServer, DownloadOnBiff, "download_on_biff");
