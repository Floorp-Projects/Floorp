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
#include "nsXPIDLString.h"

static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

NS_IMPL_ADDREF(nsMsgIdentity)
NS_IMPL_RELEASE(nsMsgIdentity)

nsresult
nsMsgIdentity::QueryInterface(const nsIID& iid, void **result)
{
  nsresult rv = NS_NOINTERFACE;
  if (!result)
    return NS_ERROR_NULL_POINTER;

  void *res = nsnull;
  if (iid.Equals(nsCOMTypeInfo<nsIMsgIdentity>::GetIID()) ||
      iid.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
    res = NS_STATIC_CAST(nsIMsgIdentity*, this);
  else if (iid.Equals(nsCOMTypeInfo<nsIShutdownListener>::GetIID()))
    res = NS_STATIC_CAST(nsIShutdownListener*, this);

  if (res) {
    NS_ADDREF(this);
    *result = res;
    rv = NS_OK;
  }

  return rv;
}


nsMsgIdentity::nsMsgIdentity():
  m_signature(0),
  m_vCard(0),
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

nsresult
nsMsgIdentity::getPrefService()
{
  if (m_prefs) return NS_OK;
  return nsServiceManager::GetService(kPrefServiceCID,
                                      nsCOMTypeInfo<nsIPref>::GetIID(),
                                      (nsISupports**)&m_prefs,
                                      this);
}


/* called if the prefs service goes offline */
NS_IMETHODIMP
nsMsgIdentity::OnShutdown(const nsCID& aClass, nsISupports *service)
{
  if (aClass.Equals(kPrefServiceCID)) {
    if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID, m_prefs);
    m_prefs = nsnull;
  }

  return NS_OK;
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

// this will be slightly faster than the above, and allows
// the "default" identity preference root to be set in one place
char *
nsMsgIdentity::getDefaultPrefName(const char *fullPrefName)
{
  return PR_smprintf("mail.identity.default.%s", fullPrefName);
}

/* The following are equivalent to the nsIPref's Get/CopyXXXPref
   except they construct the preference name with the above function
*/
nsresult
nsMsgIdentity::getBoolPref(const char *prefname,
                           PRBool *val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *fullPrefName = getPrefName(m_identityKey, prefname);
  rv = m_prefs->GetBoolPref(fullPrefName, val);
  PR_Free(fullPrefName);

  if (NS_FAILED(rv))
    rv = getDefaultBoolPref(prefname, val);
  
  return rv;
}

nsresult
nsMsgIdentity::getDefaultBoolPref(const char *prefname,
                                        PRBool *val) {
  
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *fullPrefName = getDefaultPrefName(prefname);
  rv = m_prefs->GetBoolPref(fullPrefName, val);
  PR_Free(fullPrefName);

  if (NS_FAILED(rv)) {
    *val = PR_FALSE;
    rv = NS_OK;
  }
  return rv;
}

nsresult
nsMsgIdentity::setBoolPref(const char *prefname,
                           PRBool val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *prefName = getPrefName(m_identityKey, prefname);
  rv = m_prefs->SetBoolPref(prefName, val);
  PR_Free(prefName);
  return rv;
}

nsresult
nsMsgIdentity::getCharPref(const char *prefname,
                           char **val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *fullPrefName = getPrefName(m_identityKey, prefname);
  rv = m_prefs->CopyCharPref(fullPrefName, val);
  PR_Free(fullPrefName);

  if (NS_FAILED(rv))
    rv = getDefaultCharPref(prefname, val);

  return rv;
}

nsresult
nsMsgIdentity::getDefaultCharPref(const char *prefname,
                                        char **val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *fullPrefName = getDefaultPrefName(prefname);
  rv = m_prefs->CopyCharPref(fullPrefName, val);
  PR_Free(fullPrefName);

  if (NS_FAILED(rv)) {
    *val = nsnull;              // null is ok to return here
    rv = NS_OK;
  }
  return rv;
}

nsresult
nsMsgIdentity::setCharPref(const char *prefname,
                           char *val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *prefName = getPrefName(m_identityKey, prefname);
  rv = m_prefs->SetCharPref(prefName, val);
  PR_Free(prefName);
  return rv;
}

nsresult
nsMsgIdentity::getIntPref(const char *prefname,
                                PRInt32 *val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *fullPrefName = getPrefName(m_identityKey, prefname);
  rv = m_prefs->GetIntPref(fullPrefName, val);
  PR_Free(fullPrefName);

  if (NS_FAILED(rv))
	rv = getDefaultIntPref(prefname, val);

  return rv;
}

nsresult
nsMsgIdentity::getDefaultIntPref(const char *prefname,
                                        PRInt32 *val) {
  
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *fullPrefName = getDefaultPrefName(prefname);
  rv = m_prefs->GetIntPref(fullPrefName, val);
  PR_Free(fullPrefName);

  if (NS_FAILED(rv)) {
    *val = 0;
    rv = NS_OK;
  }
  
  return rv;
}

nsresult
nsMsgIdentity::setIntPref(const char *prefname,
                                 PRInt32 val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *prefName = getPrefName(m_identityKey, prefname);
  rv = m_prefs->SetIntPref(prefName, val);
  PR_Free(prefName);
  return rv;
}

nsresult
nsMsgIdentity::SetKey(char* identityKey)
{
  PR_FREEIF(m_identityKey);
  m_identityKey = PL_strdup(identityKey);
  return NS_OK;
}

nsresult
nsMsgIdentity::GetIdentityName(char **idName) {
  if (!idName) return NS_ERROR_NULL_POINTER;

  *idName = nsnull;
  nsresult rv = getCharPref("identityName",idName);
  if (NS_FAILED(rv)) return rv;

  // there's probably a better way of doing this
  // thats unicode friendly?
  if (!(*idName)) {
    nsXPIDLCString fullName;
    rv = GetFullName(getter_Copies(fullName));
    if (NS_FAILED(rv)) return rv;
    
    nsXPIDLCString email;
    rv = GetEmail(getter_Copies(email));
    if (NS_FAILED(rv)) return rv;
    
    *idName = PR_smprintf("%s <%s>", (const char*)fullName,
                          (const char*)email);
    rv = NS_OK;
  }

  return rv;
}

nsresult nsMsgIdentity::SetIdentityName(char *idName) {
  return setCharPref("identityName", idName);
}

NS_IMETHODIMP
nsMsgIdentity::ToString(PRUnichar **aResult)
{
  nsString idname("[nsIMsgIdentity: ");
  idname += m_identityKey;
  idname += "]";

  *aResult = idname.ToNewUnicode();
  return NS_OK;
}

/* Identity attribute accessors */

// XXX - these are a COM objects, use NS_ADDREF
NS_IMPL_GETSET(nsMsgIdentity, Signature, nsIMsgSignature*, m_signature);
NS_IMPL_GETSET(nsMsgIdentity, VCard, nsIMsgVCard*, m_vCard);
  
NS_IMPL_GETTER_STR(nsMsgIdentity::GetKey, m_identityKey);

NS_IMPL_IDPREF_STR(FullName, "fullName");
NS_IMPL_IDPREF_STR(Email, "useremail");
NS_IMPL_IDPREF_STR(ReplyTo, "reply_to");
NS_IMPL_IDPREF_STR(Organization, "organization");
NS_IMPL_IDPREF_BOOL(ComposeHtml, "compose_html");
NS_IMPL_IDPREF_STR(SmtpHostname, "smtp_server");
NS_IMPL_IDPREF_STR(SmtpUsername, "smtp_name");
NS_IMPL_IDPREF_BOOL(AttachVCard, "attach_vcard");
NS_IMPL_IDPREF_BOOL(AttachSignature, "attach_signature");

NS_IMPL_IDPREF_BOOL(DoFcc, "fcc");
NS_IMPL_IDPREF_STR(FccFolder, "fcc_folder");

NS_IMPL_IDPREF_BOOL(BccSelf, "bcc_self");
NS_IMPL_IDPREF_BOOL(BccOthers, "bcc_other");
NS_IMPL_IDPREF_STR (BccList, "bcc_other_list");

NS_IMPL_IDPREF_STR (DraftFolder, "draft_folder");
NS_IMPL_IDPREF_STR (StationaryFolder, "stationary_folder");
NS_IMPL_IDPREF_STR (JunkMailFolder, "spam_folder");


