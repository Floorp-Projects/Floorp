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

#include "msgCore.h" // for pre-compiled headers
#include "nsMsgIdentity.h"
#include "nsIPref.h"


static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

NS_IMPL_ISUPPORTS(nsMsgIdentity, nsIMsgIdentity::GetIID());


nsMsgIdentity::nsMsgIdentity():
  m_identityKey(0),
  m_prefs(0)
{
	NS_INIT_REFCNT();
}

nsMsgIdentity::~nsMsgIdentity()
{
  PR_FREEIF(m_identityKey);
  if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID,
                                                m_prefs,
                                                nsnull);
}

/*
 * accessors for pulling values directly out of preferences
 * instead of member variables, etc
 */

/* convert an identity key and preference name
   to mail.identity.<identityKey>.<prefName>
*/
char *
nsMsgIdentity::getPrefName(const char *identityKey,
                           const char *prefName)
{
  return PR_smprintf("mail.identity.%s.%s", identityKey, prefName);
}

/* The following are equivalent to the nsIPref's Get/CopyXXXPref
   except they construct the preference name with the above function
*/
nsresult
nsMsgIdentity::getBoolPref(const char *prefname,
                           PRBool *val)
{
  char *prefName = getPrefName(m_identityKey, prefname);
  nsresult rv = m_prefs->GetBoolPref(prefName, val);
  PR_Free(prefName);
  return rv;
}

nsresult
nsMsgIdentity::setBoolPref(const char *prefname,
                           PRBool val)
{
  char *prefName = getPrefName(m_identityKey, prefname);
  nsresult rv = m_prefs->SetBoolPref(prefName, val);
  PR_Free(prefName);
  return rv;
}

nsresult
nsMsgIdentity::getCharPref(const char *prefname,
                           char **val)
{
  char *prefName = getPrefName(m_identityKey, prefname);
  nsresult rv = m_prefs->CopyCharPref(prefName, val);
  PR_Free(prefName);
  return rv;
}

nsresult
nsMsgIdentity::setCharPref(const char *prefname,
                           char *val)
{
  char *prefName = getPrefName(m_identityKey, prefname);
  nsresult rv = m_prefs->SetCharPref(prefName, val);
  PR_Free(prefName);
  return rv;
}

nsresult
nsMsgIdentity::SetKey(char* identityKey)
{
  nsresult rv = NS_OK;
  // in order to actually make use of the key, we need the prefs
  if (!m_prefs)
    rv = nsServiceManager::GetService(kPrefServiceCID,
                                      nsIPref::GetIID(),
                                      (nsISupports**)&m_prefs);
  
  m_identityKey = PL_strdup(identityKey);
  return NS_OK;
}

/* Identity attribute accessors */

// XXX - these are a COM objects, use NS_ADDREF
NS_IMPL_GETSET(nsMsgIdentity, Signature, nsIMsgSignature*, m_signature);
NS_IMPL_GETSET(nsMsgIdentity, VCard, nsIMsgVCard*, m_vCard);
  
NS_IMPL_GETTER_STR(nsMsgIdentity::GetKey, m_identityKey);

NS_IMPL_IDPREF_STR(IdentityName, "identityName");
NS_IMPL_IDPREF_STR(FullName, "fullName");
NS_IMPL_IDPREF_STR(Email, "useremail");
NS_IMPL_IDPREF_STR(ReplyTo, "reply_to");
NS_IMPL_IDPREF_STR(Organization, "organization");
NS_IMPL_IDPREF_BOOL(UseHtml, "send_html");
NS_IMPL_IDPREF_STR(SmtpHostname, "smtp_server");
NS_IMPL_IDPREF_STR(SmtpUsername, "smtp_name");

