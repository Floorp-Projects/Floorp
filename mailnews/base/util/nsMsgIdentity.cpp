/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "msgCore.h" // for pre-compiled headers
#include "nsMsgIdentity.h"
#include "nsIPref.h"
#include "nsXPIDLString.h"

#include "nsISmtpService.h"
#include "nsMsgCompCID.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMsgIdentity,
                   nsIMsgIdentity)

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
  if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID, m_prefs);
}

nsresult
nsMsgIdentity::getPrefService()
{
  if (m_prefs) return NS_OK;
  return nsServiceManager::GetService(kPrefServiceCID,
                                      NS_GET_IID(nsIPref),
                                      (nsISupports**)&m_prefs);
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
nsMsgIdentity::getUnicharPref(const char *prefname,
                              PRUnichar **val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *fullPrefName = getPrefName(m_identityKey, prefname);
  rv = m_prefs->CopyUnicharPref(fullPrefName, val);
  PR_Free(fullPrefName);

  if (NS_FAILED(rv))
    rv = getDefaultUnicharPref(prefname, val);

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
nsMsgIdentity::getDefaultUnicharPref(const char *prefname,
                                     PRUnichar **val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *fullPrefName = getDefaultPrefName(prefname);
  rv = m_prefs->CopyUnicharPref(fullPrefName, val);
  PR_Free(fullPrefName);

  if (NS_FAILED(rv)) {
    *val = nsnull;              // null is ok to return here
    rv = NS_OK;
  }
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
nsMsgIdentity::setUnicharPref(const char *prefname,
                              const PRUnichar *val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  rv = NS_OK;
  char *prefName = getPrefName(m_identityKey, prefname);
  if (val) 
    rv = m_prefs->SetUnicharPref(prefName, val);
  else
    m_prefs->ClearUserPref(prefName);
  PR_Free(prefName);
  return rv;
}

nsresult
nsMsgIdentity::setCharPref(const char *prefname,
                           const char *val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  rv = NS_OK;
  char *prefName = getPrefName(m_identityKey, prefname);
  if (val) 
    rv = m_prefs->SetCharPref(prefName, val);
  else
    m_prefs->ClearUserPref(prefName);
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
nsMsgIdentity::SetKey(const char* identityKey)
{
  PR_FREEIF(m_identityKey);
  m_identityKey = PL_strdup(identityKey);
  return NS_OK;
}

nsresult
nsMsgIdentity::GetIdentityName(PRUnichar **idName) {
  if (!idName) return NS_ERROR_NULL_POINTER;

  *idName = nsnull;
  nsresult rv = getUnicharPref("identityName",idName);
  if (NS_FAILED(rv)) return rv;

  if (!(*idName)) {
    nsXPIDLString fullName;
    rv = GetFullName(getter_Copies(fullName));
    if (NS_FAILED(rv)) return rv;
    
    nsXPIDLCString email;
    rv = GetEmail(getter_Copies(email));
    if (NS_FAILED(rv)) return rv;

    nsAutoString str;
    str += (const PRUnichar*)fullName;
    str += " <";
    str += (const char*)email;
    str += ">";
    *idName = str.ToNewUnicode();
    rv = NS_OK;
  }

  return rv;
}

nsresult nsMsgIdentity::SetIdentityName(const PRUnichar *idName) {
  return setUnicharPref("identityName", idName);
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
//NS_IMPL_GETSET(nsMsgIdentity, Signature, nsIMsgSignature*, m_signature);
NS_IMETHODIMP
nsMsgIdentity::GetSignature(nsIFileSpec **sig) {
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *prefName = getPrefName(m_identityKey, "sig_file");
  rv = m_prefs->GetFilePref(prefName, sig);
  if (NS_FAILED(rv))
    *sig = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIdentity::SetSignature(nsIFileSpec *sig)
{

  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  rv = NS_OK;
  char *prefName = getPrefName(m_identityKey, "sig_file");
  if (sig) 
    rv = m_prefs->SetFilePref(NS_CONST_CAST(const char*,prefName), sig,
                              PR_FALSE);
  /*
  else
    m_prefs->ClearFilePref(prefName);
  */
  PR_Free(prefName);
  return rv;
  
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIdentity::ClearAllValues()
{
    nsresult rv;
    nsCAutoString rootPref("mail.identity.");
    rootPref += m_identityKey;

    rv = m_prefs->EnumerateChildren(rootPref, clearPrefEnum, (void *)m_prefs);

    return rv;
}

void
nsMsgIdentity::clearPrefEnum(const char *aPref, void *aClosure)
{
    nsIPref *prefs = (nsIPref *)aClosure;
    prefs->ClearUserPref(aPref);
}

NS_IMETHODIMP
nsMsgIdentity::GetSmtpServer(nsISmtpServer **aResult)
{
  nsresult rv;

  nsCOMPtr<nsISmtpServer> smtpServer =
    do_QueryReferent(m_smtpServer, &rv);

  // try to load, but ignore the error and return null if nothing
  if (!smtpServer)
    loadSmtpServer(getter_AddRefs(smtpServer));

  *aResult = smtpServer;
  NS_IF_ADDREF(*aResult);
  
  return NS_OK;
}

nsresult
nsMsgIdentity::loadSmtpServer(nsISmtpServer** aResult)
{
  nsresult rv;
  
  nsXPIDLCString smtpServerKey;
  rv = getCharPref("smtpServer", getter_Copies(smtpServerKey));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISmtpService> smtpService =
    do_GetService(NS_SMTPSERVICE_PROGID, &rv);
  if (NS_FAILED(rv)) return rv;

  return smtpService->GetServerByKey(smtpServerKey, aResult);
}

NS_IMETHODIMP
nsMsgIdentity::SetSmtpServer(nsISmtpServer *aServer)
{
  nsresult rv;
  
  m_smtpServer = NS_GetWeakReference(aServer, &rv);

  if (aServer) {
    nsXPIDLCString smtpServerKey;
    rv = aServer->GetKey(getter_Copies(smtpServerKey));
    if (NS_SUCCEEDED(rv)) {
      setCharPref("smtpServer", smtpServerKey);
    }
  }

  return NS_OK;
}


NS_IMPL_GETSET(nsMsgIdentity, VCard, nsIMsgVCard*, m_vCard);
  
NS_IMPL_GETTER_STR(nsMsgIdentity::GetKey, m_identityKey);

NS_IMPL_IDPREF_WSTR(FullName, "fullName");
NS_IMPL_IDPREF_STR(Email, "useremail");
NS_IMPL_IDPREF_STR(ReplyTo, "reply_to");
NS_IMPL_IDPREF_WSTR(Organization, "organization");
NS_IMPL_IDPREF_BOOL(ComposeHtml, "compose_html");
NS_IMPL_IDPREF_BOOL(AttachVCard, "attach_vcard");
NS_IMPL_IDPREF_BOOL(AttachSignature, "attach_signature");

NS_IMPL_IDPREF_INT(SignatureDate,"sig_date");

NS_IMPL_IDPREF_BOOL(DoFcc, "fcc");
NS_IMPL_IDPREF_STR(FccFolder, "fcc_folder");

NS_IMPL_IDPREF_BOOL(BccSelf, "bcc_self");
NS_IMPL_IDPREF_BOOL(BccOthers, "bcc_other");
NS_IMPL_IDPREF_STR (BccList, "bcc_other_list");

NS_IMPL_IDPREF_STR (DraftFolder, "draft_folder");
NS_IMPL_IDPREF_STR (StationeryFolder, "stationery_folder");
NS_IMPL_IDPREF_STR (JunkMailFolder, "spam_folder");

NS_IMPL_IDPREF_BOOL(Valid, "valid");

