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

#include "msgCore.h" // for pre-compiled headers
#include "nsMsgIdentity.h"
#include "nsIPref.h"
#include "nsXPIDLString.h"

#include "nsMsgCompCID.h"
#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsRDFCID.h"
#include "nsMsgFolderFlags.h"
#include "nsIMsgFolder.h"
#include "prprf.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

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
    str.AppendWithConversion(" <");
    str.AppendWithConversion((const char*)email);
    str.AppendWithConversion(">");
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
  nsString idname; idname.AssignWithConversion("[nsIMsgIdentity: ");
  idname.AppendWithConversion(m_identityKey);
  idname.AppendWithConversion("]");

  *aResult = idname.ToNewUnicode();
  return NS_OK;
}

/* Identity attribute accessors */

// XXX - these are a COM objects, use NS_ADDREF
//NS_IMPL_GETSET(nsMsgIdentity, Signature, nsIMsgSignature*, m_signature);
NS_IMETHODIMP
nsMsgIdentity::GetSignature(nsILocalFile **sig) {
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *prefName = getPrefName(m_identityKey, "sig_file");
  rv = m_prefs->GetFileXPref(prefName, sig);
  if (NS_FAILED(rv))
    *sig = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIdentity::SetSignature(nsILocalFile *sig)
{

  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  rv = NS_OK;
  char *prefName = getPrefName(m_identityKey, "sig_file");
  if (sig) 
      rv = m_prefs->SetFileXPref(prefName, sig);
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


NS_IMPL_GETSET(nsMsgIdentity, VCard, nsIMsgVCard*, m_vCard);
  
NS_IMPL_GETTER_STR(nsMsgIdentity::GetKey, m_identityKey);

NS_IMPL_IDPREF_STR(SmtpServerKey, "smtpServer");
NS_IMPL_IDPREF_WSTR(FullName, "fullName");
NS_IMPL_IDPREF_STR(Email, "useremail");
NS_IMPL_IDPREF_STR(ReplyTo, "reply_to");
NS_IMPL_IDPREF_WSTR(Organization, "organization");
NS_IMPL_IDPREF_BOOL(ComposeHtml, "compose_html");
NS_IMPL_IDPREF_BOOL(AttachVCard, "attach_vcard");
NS_IMPL_IDPREF_BOOL(AttachSignature, "attach_signature");

NS_IMPL_IDPREF_INT(SignatureDate,"sig_date");

NS_IMPL_IDPREF_BOOL(DoFcc, "fcc");
NS_IMPL_FOLDERPREF_STR(FccFolder, "fcc_folder");

NS_IMPL_IDPREF_STR(FccFolderPickerMode, "fcc_folder_picker_mode");
NS_IMPL_IDPREF_STR(DraftsFolderPickerMode, "drafts_folder_picker_mode");
NS_IMPL_IDPREF_STR(TmplFolderPickerMode, "tmpl_folder_picker_mode");

NS_IMPL_IDPREF_BOOL(BccSelf, "bcc_self");
NS_IMPL_IDPREF_BOOL(BccOthers, "bcc_other");
NS_IMPL_IDPREF_STR (BccList, "bcc_other_list");

NS_IMPL_FOLDERPREF_STR (DraftFolder, "draft_folder");
NS_IMPL_FOLDERPREF_STR (StationeryFolder, "stationery_folder");
NS_IMPL_FOLDERPREF_STR (JunkMailFolder, "spam_folder");

NS_IMPL_IDPREF_BOOL(ShowSaveMsgDlg, "showSaveMsgDlg");
NS_IMPL_IDPREF_STR (DirectoryServer, "directoryServer");
NS_IMPL_IDPREF_BOOL(OverrideGlobalPref, "overrideGlobal_Pref");

NS_IMPL_IDPREF_BOOL(Valid, "valid");

nsresult 
nsMsgIdentity::getFolderPref(const char *prefname, char **retval, PRBool mustHaveDefault)
{
  nsresult rv = getCharPref(prefname, retval);
  if (!mustHaveDefault) return rv;

  // Use default value if fail to get or not set
  if (NS_FAILED(rv) || !*retval || !nsCRT::strlen(*retval))
  {
    PR_FREEIF(*retval);	// free the empty string
    rv = getDefaultCharPref(prefname, retval);
    if (NS_SUCCEEDED(rv) && *retval)
    {
      rv = setFolderPref(prefname, (const char *)*retval);
    }
  }
  return rv;
}

nsresult 
nsMsgIdentity::setFolderPref(const char *prefname, const char *value)
{
    nsXPIDLCString oldpref;
    nsresult rv;
    nsCOMPtr<nsIRDFResource> res;
    nsCOMPtr<nsIMsgFolder> folder;
    PRUint32 folderflag;
    nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
    
    if (nsCRT::strcmp(prefname, "fcc_folder") == 0)
        folderflag = MSG_FOLDER_FLAG_SENTMAIL;
    else if (nsCRT::strcmp(prefname, "draft_folder") == 0)
        folderflag = MSG_FOLDER_FLAG_DRAFTS;
    else if (nsCRT::strcmp(prefname, "stationery_folder") == 0)
        folderflag = MSG_FOLDER_FLAG_TEMPLATES;
    else
        return NS_ERROR_FAILURE;

    // get the old folder, and clear the special folder flag on it
    rv = getFolderPref(prefname, getter_Copies(oldpref), PR_FALSE);
    if (NS_SUCCEEDED(rv) && (const char*)oldpref)
    {
        rv = rdf->GetResource(oldpref, getter_AddRefs(res));
        if (NS_SUCCEEDED(rv) && res)
        {
            folder = do_QueryInterface(res, &rv);
            if (NS_SUCCEEDED(rv))
                rv = folder->ClearFlag(folderflag);
        }
    }
   
    // set the new folder, and set the special folder flags on it
    rv = setCharPref(prefname, value);
    if (NS_SUCCEEDED(rv))
    {
        rv = rdf->GetResource(value, getter_AddRefs(res));
        if (NS_SUCCEEDED(rv) && res)
        {
            folder = do_QueryInterface(res, &rv);
            if (NS_SUCCEEDED(rv))
                rv = folder->SetFlag(folderflag);
        }
    }
    return rv;
}

#define COPY_IDENTITY_FILE_VALUE(SRC_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
		nsresult macro_rv;	\
		nsCOMPtr <nsILocalFile>macro_spec;   \
        	macro_rv = SRC_ID->MACRO_GETTER(getter_AddRefs(macro_spec)); \
        	if (NS_FAILED(macro_rv)) return macro_rv;	\
        	this->MACRO_SETTER(macro_spec);     \
	}

#define COPY_IDENTITY_INT_VALUE(SRC_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
		    nsresult macro_rv;	\
        	PRInt32 macro_oldInt;	\
        	macro_rv = SRC_ID->MACRO_GETTER(&macro_oldInt);	\
        	if (NS_FAILED(macro_rv)) return macro_rv;	\
        	this->MACRO_SETTER(macro_oldInt);     \
	}

#define COPY_IDENTITY_BOOL_VALUE(SRC_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
		    nsresult macro_rv;	\
        	PRBool macro_oldBool;	\
        	macro_rv = SRC_ID->MACRO_GETTER(&macro_oldBool);	\
        	if (NS_FAILED(macro_rv)) return macro_rv;	\
        	this->MACRO_SETTER(macro_oldBool);     \
	}

#define COPY_IDENTITY_STR_VALUE(SRC_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
        	nsXPIDLCString macro_oldStr;	\
		    nsresult macro_rv;	\
        	macro_rv = SRC_ID->MACRO_GETTER(getter_Copies(macro_oldStr));	\
        	if (NS_FAILED(macro_rv)) return macro_rv;	\
        	if (!macro_oldStr) {	\
                	this->MACRO_SETTER("");	\
        	}	\
        	else {	\
                	this->MACRO_SETTER(macro_oldStr);	\
        	}	\
	}

static const PRUnichar unicharEmptyString[] = { (PRUnichar)'\0' };

#define COPY_IDENTITY_WSTR_VALUE(SRC_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
        	nsXPIDLString macro_oldStr;	\
		    nsresult macro_rv;	\
        	macro_rv = SRC_ID->MACRO_GETTER(getter_Copies(macro_oldStr));	\
        	if (NS_FAILED(macro_rv)) return macro_rv;	\
        	if (!macro_oldStr) {	\
                	this->MACRO_SETTER(unicharEmptyString);	\
        	}	\
        	else {	\
                	this->MACRO_SETTER(macro_oldStr);	\
        	}	\
	}

NS_IMETHODIMP
nsMsgIdentity::Copy(nsIMsgIdentity *identity)
{
    COPY_IDENTITY_BOOL_VALUE(identity,GetComposeHtml,SetComposeHtml)
    COPY_IDENTITY_STR_VALUE(identity,GetEmail,SetEmail)
    COPY_IDENTITY_STR_VALUE(identity,GetReplyTo,SetReplyTo)
    COPY_IDENTITY_WSTR_VALUE(identity,GetFullName,SetFullName)
    COPY_IDENTITY_WSTR_VALUE(identity,GetOrganization,SetOrganization)
    COPY_IDENTITY_STR_VALUE(identity,GetDraftFolder,SetDraftFolder)
    COPY_IDENTITY_STR_VALUE(identity,GetStationeryFolder,SetStationeryFolder)
    COPY_IDENTITY_BOOL_VALUE(identity,GetAttachSignature,SetAttachSignature)
    COPY_IDENTITY_FILE_VALUE(identity,GetSignature,SetSignature)
    COPY_IDENTITY_INT_VALUE(identity,GetSignatureDate,SetSignatureDate)

    return NS_OK;
}
