/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

nsMsgIncomingServer::nsMsgIncomingServer():
  m_prettyName(0),
  m_hostName(0),
  m_userName(0),
  m_password(0),
  m_doBiff(PR_FALSE),
  m_biffMinutes(0)
{
  NS_INIT_REFCNT();
}

nsMsgIncomingServer::~nsMsgIncomingServer()
{
  PR_FREEIF(m_prettyName);
  PR_FREEIF(m_hostName);
  PR_FREEIF(m_userName);
  PR_FREEIF(m_password);
}

NS_IMPL_ISUPPORTS(nsMsgIncomingServer, GetIID());

NS_IMPL_GETSET_STR(nsMsgIncomingServer, PrettyName, m_prettyName)
NS_IMPL_GETSET_STR(nsMsgIncomingServer, HostName, m_hostName)
NS_IMPL_GETSET_STR(nsMsgIncomingServer, UserName, m_userName)
NS_IMPL_GETSET_STR(nsMsgIncomingServer, Password, m_password)
NS_IMPL_GETSET(nsMsgIncomingServer, DoBiff, PRBool, m_doBiff)
NS_IMPL_GETSET(nsMsgIncomingServer, BiffMinutes, PRInt32, m_biffMinutes)

char *
nsMsgIncomingServer::getPrefName(const char *serverKey,
                                 const char *prefName)
{
  return PR_smprintf("mail.server.%s.%s", serverKey, prefName);
}

PRBool
nsMsgIncomingServer::getBoolPref(nsIPref *prefs,
                                 const char *serverKey,
                                 const char *prefname)
{
  char *prefName = getPrefName(serverKey, prefname);
  PRBool val=PR_FALSE;
  prefs->GetBoolPref(prefName, &val);
  PR_Free(prefName);
  return val;
}

PRInt32
nsMsgIncomingServer::getIntPref(nsIPref *prefs,
                                 const char *serverKey,
                                 const char *prefname)
{
  char *prefName = getPrefName(serverKey, prefname);
  PRInt32 val=0;
  prefs->GetIntPref(prefName, &val);
  PR_Free(prefName);
  return val;
}

char *
nsMsgIncomingServer::getCharPref(nsIPref *prefs,
                           const char *serverKey,
                           const char *prefname)
{
  char *prefName = getPrefName(serverKey, prefname);
  char *val=nsnull;
  nsresult rv = prefs->CopyCharPref(prefName, &val);
  PR_Free(prefName);
  if (NS_FAILED(rv)) return nsnull;
  return val;
}

NS_IMETHODIMP
nsMsgIncomingServer::LoadPreferences(nsIPref *prefs, const char *serverKey)
{
#ifdef DEBUG_alecf
    printf("Loading generic server prefs for %s\n", serverKey);
#endif
    
    m_prettyName = getCharPref(prefs, serverKey, "name");
    m_hostName = getCharPref(prefs, serverKey, "hostname");
    m_userName = getCharPref(prefs, serverKey, "userName");
    m_password = getCharPref(prefs, serverKey, "password");
    m_doBiff = getBoolPref(prefs, serverKey, "check_new_mail");
    m_biffMinutes = getIntPref(prefs, serverKey, "check_time");

    return NS_OK;
}

