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
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

NS_IMPL_ISUPPORTS(nsMsgIdentity, nsIMsgIdentity::GetIID());


nsMsgIdentity::nsMsgIdentity():
  m_identityName(nsnull),
  m_fullName(nsnull),
  m_email(nsnull),
  m_replyTo(nsnull),
  m_organization(nsnull),
  m_useHtml(PR_FALSE),
  m_signature(nsnull),
  m_vCard(nsnull),
  m_smtpHostname(nsnull),
  m_smtpUsername(nsnull)
{
	NS_INIT_REFCNT();

}

nsMsgIdentity::~nsMsgIdentity()
{
  PR_FREEIF(m_identityName);
  PR_FREEIF(m_fullName);
  PR_FREEIF(m_email);
  PR_FREEIF(m_replyTo);
  PR_FREEIF(m_organization);
  NS_IF_RELEASE(m_signature);
  NS_IF_RELEASE(m_vCard);
  PR_FREEIF(m_smtpHostname);
  PR_FREEIF(m_smtpUsername);
}

NS_IMPL_GETSET_STR(nsMsgIdentity, IdentityName, m_identityName)
NS_IMPL_GETSET_STR(nsMsgIdentity, FullName, m_fullName)
NS_IMPL_GETSET_STR(nsMsgIdentity, Email, m_email)
NS_IMPL_GETSET_STR(nsMsgIdentity, ReplyTo, m_replyTo)
NS_IMPL_GETSET_STR(nsMsgIdentity, Organization, m_organization)
NS_IMPL_GETSET(nsMsgIdentity, UseHtml, PRBool, m_useHtml)
    
// XXX - these are a COM objects, use NS_ADDREF
NS_IMPL_GETSET(nsMsgIdentity, Signature, nsIMsgSignature*, m_signature);
NS_IMPL_GETSET(nsMsgIdentity, VCard, nsIMsgVCard*, m_vCard);
  
NS_IMPL_GETSET_STR(nsMsgIdentity, SmtpHostname, m_smtpHostname);
NS_IMPL_GETSET_STR(nsMsgIdentity, SmtpUsername, m_smtpUsername);
NS_IMPL_GETSET_STR(nsMsgIdentity, Key, m_key);

char *
nsMsgIdentity::getPrefName(const char *identityKey,
                           const char *prefName)
{
  return PR_smprintf("mail.identity.%s.%s", identityKey, prefName);
}

PRBool
nsMsgIdentity::getBoolPref(nsIPref *prefs,
                           const char *identityKey,
                           const char *prefname)
{
  char *prefName = getPrefName(identityKey, prefname);
  PRBool val=PR_FALSE;
  prefs->GetBoolPref(prefName, &val);
  PR_Free(prefName);
  return val;
}

char *
nsMsgIdentity::getCharPref(nsIPref *prefs,
                           const char *identityKey,
                           const char *prefname)
{
  char *prefName = getPrefName(identityKey, prefname);
  char *val=nsnull;
  nsresult rv = prefs->CopyCharPref(prefName, &val);
  PR_Free(prefName);
  if (NS_FAILED(rv)) return nsnull;
  return val;
}

nsresult
nsMsgIdentity::LoadPreferences(nsIPref *prefs, const char* identityKey)
{

#ifdef DEBUG_alecf
    printf("Loading identity for %s\n", identityKey);
#endif
    
    NS_ADDREF(prefs);
    m_identityName = getCharPref(prefs, identityKey, "name");
    m_fullName     = getCharPref(prefs, identityKey, "fullName");
    m_email        = getCharPref(prefs, identityKey, "useremail");
    m_replyTo      = getCharPref(prefs, identityKey, "reply_to");
    m_organization = getCharPref(prefs, identityKey, "organization");
    m_useHtml      = getBoolPref(prefs, identityKey, "send_html");
    m_smtpHostname = getCharPref(prefs, identityKey, "smtp_server");
    m_smtpUsername = getCharPref(prefs, identityKey, "smtp_name");
    NS_RELEASE(prefs);
    
    return NS_OK;
}

