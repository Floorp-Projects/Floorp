/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h" // for pre-compiled headers
#include "nsMsgIdentity.h"
#include "nsIPrefService.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

#include "nsMsgCompCID.h"
#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsRDFCID.h"
#include "nsMsgFolderFlags.h"
#include "nsIMsgFolder.h"
#include "nsIMsgIncomingServer.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "prprf.h"
#include "nsISupportsObsolete.h"
#include "nsISupportsPrimitives.h"
#include "nsMsgUtils.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

#define REL_FILE_PREF_SUFFIX NS_LITERAL_CSTRING("-rel")

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMsgIdentity,
                   nsIMsgIdentity)

nsMsgIdentity::nsMsgIdentity():
  m_signature(0),
  m_identityKey(0),
  m_prefBranch(0)
{
}

nsMsgIdentity::~nsMsgIdentity()
{
  PR_FREEIF(m_identityKey);
  NS_IF_RELEASE(m_prefBranch);
}

nsresult
nsMsgIdentity::getPrefService()
{
  if (m_prefBranch)
    return NS_OK;

  return CallGetService(NS_PREFSERVICE_CONTRACTID, &m_prefBranch);
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
  rv = m_prefBranch->GetBoolPref(fullPrefName, val);
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
  rv = m_prefBranch->GetBoolPref(fullPrefName, val);
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
  rv = m_prefBranch->SetBoolPref(prefName, val);
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
  nsCOMPtr<nsISupportsString> supportsString;
  rv = m_prefBranch->GetComplexValue(fullPrefName,
                                     NS_GET_IID(nsISupportsString),
                                     getter_AddRefs(supportsString));
  PR_Free(fullPrefName);

  if (NS_FAILED(rv))
    rv = getDefaultUnicharPref(prefname, val);

  if (supportsString)
    rv = supportsString->ToString(val);

  return rv;
}

nsresult
nsMsgIdentity::getCharPref(const char *prefname,
                           char **val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *fullPrefName = getPrefName(m_identityKey, prefname);
  rv = m_prefBranch->GetCharPref(fullPrefName, val);
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
  nsCOMPtr<nsISupportsString> supportsString;
  rv = m_prefBranch->GetComplexValue(fullPrefName,
                                     NS_GET_IID(nsISupportsString),
                                     getter_AddRefs(supportsString));
  PR_Free(fullPrefName);

  if (NS_FAILED(rv) || !supportsString) {
    *val = nsnull;              // null is ok to return here
    return NS_OK;
  }

  return supportsString->ToString(val);
}

nsresult
nsMsgIdentity::getDefaultCharPref(const char *prefname,
                                        char **val)
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *fullPrefName = getDefaultPrefName(prefname);
  rv = m_prefBranch->GetCharPref(fullPrefName, val);
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
  if (val) {
    nsCOMPtr<nsISupportsString> supportsString =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    if (supportsString) {
      supportsString->SetData(nsDependentString(val));
      rv = m_prefBranch->SetComplexValue(prefName,
                                         NS_GET_IID(nsISupportsString),
                                         supportsString);
    }
  }
  else {
    m_prefBranch->ClearUserPref(prefName);
  }
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
    rv = m_prefBranch->SetCharPref(prefName, val);
  else
    m_prefBranch->ClearUserPref(prefName);
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
  rv = m_prefBranch->GetIntPref(fullPrefName, val);
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
  rv = m_prefBranch->GetIntPref(fullPrefName, val);
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
  rv = m_prefBranch->SetIntPref(prefName, val);
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
    str.AppendLiteral(" <");
    str.AppendWithConversion((const char*)email);
    str.AppendLiteral(">");
    *idName = ToNewUnicode(str);
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
  nsString idname(NS_LITERAL_STRING("[nsIMsgIdentity: "));
  idname.AppendWithConversion(m_identityKey);
  idname.AppendLiteral("]");

  *aResult = ToNewUnicode(idname);
  return NS_OK;
}

/* Identity attribute accessors */

NS_IMETHODIMP
nsMsgIdentity::GetSignature(nsILocalFile **sig) 
{
  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  char *prefName = getPrefName(m_identityKey, "sig_file");
  if (!prefName)
    return NS_ERROR_FAILURE;  
  nsCAutoString relPrefName(prefName);
  relPrefName.Append(REL_FILE_PREF_SUFFIX);
  
  PRBool gotRelPref;
  rv = NS_GetPersistentFile(relPrefName.get(), prefName, nsnull, gotRelPref, sig);
  if (NS_SUCCEEDED(rv) && !gotRelPref) 
  {
    rv = NS_SetPersistentFile(relPrefName.get(), prefName, *sig);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to write signature file pref.");
  }
  PR_Free(prefName);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgIdentity::SetSignature(nsILocalFile *sig)
{
  nsresult rv = NS_OK;
  if (sig) 
  {
    char *prefName = getPrefName(m_identityKey, "sig_file");
    if (!prefName)
      return NS_ERROR_FAILURE;  
  
    nsCAutoString relPrefName(prefName);
    relPrefName.Append(REL_FILE_PREF_SUFFIX);
    rv = NS_SetPersistentFile(relPrefName.get(), prefName, sig);
  }
  return rv;
}

NS_IMETHODIMP
nsMsgIdentity::ClearAllValues()
{
    nsresult rv = getPrefService();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString rootPref("mail.identity.");
    rootPref += m_identityKey;

    PRUint32 childCount;
    char**   childArray;
    rv = m_prefBranch->GetChildList(rootPref.get(), &childCount, &childArray);
    NS_ENSURE_SUCCESS(rv, rv);

    for (PRUint32 i = 0; i < childCount; ++i) {
        m_prefBranch->ClearUserPref(childArray[i]);
    }

    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(childCount, childArray);

    return NS_OK;
}


NS_IMPL_GETTER_STR(nsMsgIdentity::GetKey, m_identityKey)
NS_IMPL_IDPREF_STR(EscapedVCard, "escapedVCard")
NS_IMPL_IDPREF_STR(SmtpServerKey, "smtpServer")
NS_IMPL_IDPREF_WSTR(FullName, "fullName")
NS_IMPL_IDPREF_STR(Email, "useremail")
NS_IMPL_IDPREF_STR(ReplyTo, "reply_to")
NS_IMPL_IDPREF_WSTR(Organization, "organization")
NS_IMPL_IDPREF_BOOL(ComposeHtml, "compose_html")
NS_IMPL_IDPREF_BOOL(AttachVCard, "attach_vcard")
NS_IMPL_IDPREF_BOOL(AttachSignature, "attach_signature")

NS_IMPL_IDPREF_BOOL(AutoQuote, "auto_quote")
NS_IMPL_IDPREF_INT(ReplyOnTop, "reply_on_top")
NS_IMPL_IDPREF_BOOL(SigBottom, "sig_bottom")

NS_IMPL_IDPREF_INT(SignatureDate,"sig_date")

NS_IMPL_IDPREF_BOOL(DoFcc, "fcc")

NS_IMPL_FOLDERPREF_STR(FccFolder, "fcc_folder")
NS_IMPL_IDPREF_STR(FccFolderPickerMode, "fcc_folder_picker_mode")
NS_IMPL_IDPREF_STR(DraftsFolderPickerMode, "drafts_folder_picker_mode")
NS_IMPL_IDPREF_STR(TmplFolderPickerMode, "tmpl_folder_picker_mode")

NS_IMPL_IDPREF_BOOL(BccSelf, "bcc_self")
NS_IMPL_IDPREF_BOOL(BccOthers, "bcc_other")
NS_IMPL_IDPREF_STR (BccList, "bcc_other_list")

NS_IMETHODIMP
nsMsgIdentity::GetDoBcc(PRBool *aValue)
{
  nsresult rv = getPrefService();
  NS_ENSURE_SUCCESS(rv,rv);
  
  char *prefName = getPrefName(m_identityKey, "doBcc");
  rv = m_prefBranch->GetBoolPref(prefName, aValue);
  PR_Free(prefName);
  
  if (NS_SUCCEEDED(rv))
    return GetBoolAttribute("doBcc", aValue);

  PRBool bccSelf = PR_FALSE;
  rv = GetBccSelf(&bccSelf);
  NS_ENSURE_SUCCESS(rv,rv);

  PRBool bccOthers = PR_FALSE;
  rv = GetBccOthers(&bccOthers);
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString others;
  rv = GetBccList(getter_Copies(others));
  NS_ENSURE_SUCCESS(rv,rv);
    
  *aValue = bccSelf || (bccOthers && !others.IsEmpty());

  return SetDoBcc(*aValue);  
}

NS_IMETHODIMP
nsMsgIdentity::SetDoBcc(PRBool aValue)
{
  return SetBoolAttribute("doBcc", aValue);
}

NS_IMETHODIMP
nsMsgIdentity::GetDoBccList(char **aValue)
{
  nsresult rv = getPrefService();
  NS_ENSURE_SUCCESS(rv,rv);

  char *prefName = getPrefName(m_identityKey, "doBccList");
  rv = m_prefBranch->GetCharPref(prefName, aValue);
  PR_Free(prefName);

  if (NS_SUCCEEDED(rv))
    return GetCharAttribute("doBccList", aValue);
  
  nsCAutoString result;

  PRBool bccSelf = PR_FALSE;
  rv = GetBccSelf(&bccSelf);
  NS_ENSURE_SUCCESS(rv,rv);
  
  if (bccSelf) {
    nsXPIDLCString email;
    GetEmail(getter_Copies(email));
    result += email; 
  }
  
  PRBool bccOthers = PR_FALSE;
  rv = GetBccOthers(&bccOthers);
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString others;
  rv = GetBccList(getter_Copies(others));
  NS_ENSURE_SUCCESS(rv,rv);

  if (bccOthers && !others.IsEmpty()) {
    if (bccSelf)
      result += ",";
    result += others;
  }
  
  *aValue = ToNewCString(result);
  
  return SetDoBccList(*aValue);  
}

NS_IMETHODIMP
nsMsgIdentity::SetDoBccList(const char *aValue)
{
  return SetCharAttribute("doBccList", aValue);
}

NS_IMPL_FOLDERPREF_STR (DraftFolder, "draft_folder")
NS_IMPL_FOLDERPREF_STR (StationeryFolder, "stationery_folder")

NS_IMPL_IDPREF_BOOL(ShowSaveMsgDlg, "showSaveMsgDlg")
NS_IMPL_IDPREF_STR (DirectoryServer, "directoryServer")
NS_IMPL_IDPREF_BOOL(OverrideGlobalPref, "overrideGlobal_Pref")

NS_IMPL_IDPREF_BOOL(Valid, "valid")

nsresult 
nsMsgIdentity::getFolderPref(const char *prefname, char **retval, PRBool mustHaveDefault)
{
  nsresult rv = getCharPref(prefname, retval);
  if (!mustHaveDefault) return rv;

  // Use default value if fail to get or not set
  if (NS_FAILED(rv) || !*retval || !strlen(*retval))
  {
    PR_FREEIF(*retval);	// free the empty string
    rv = getDefaultCharPref(prefname, retval);
    if (NS_SUCCEEDED(rv) && *retval)
    {
      rv = setFolderPref(prefname, (const char *)*retval);
    }
  }
  // get the corresponding RDF resource
  // RDF will create the folder resource if it doesn't already exist
  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIRDFResource> resource;
  rv = rdf->GetResource(nsDependentCString(*retval), getter_AddRefs(resource));
  if (NS_FAILED(rv)) 
    return rv;

  nsCOMPtr <nsIMsgFolder> folderResource;
  folderResource = do_QueryInterface(resource, &rv);
  if (NS_SUCCEEDED(rv) && folderResource) 
  {
    // don't check validity of folder - caller will handle creating it
    nsCOMPtr<nsIMsgIncomingServer> server; 
    //make sure that folder hierarchy is built so that legitimate parent-child relationship is established
    rv = folderResource->GetServer(getter_AddRefs(server));
    if (server)
    {
      nsCOMPtr <nsIMsgFolder> msgFolder;
      rv = server->GetMsgFolderFromURI(folderResource, *retval, getter_AddRefs(msgFolder));
      PR_Free(*retval);
      if (NS_SUCCEEDED(rv))
        return msgFolder->GetURI(retval);
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
  {
    // Clear the temporary return receipt filter so that the new filter
    // rule can be recreated (by ConfigureTemporaryFilters()).
    nsCOMPtr<nsIMsgAccountManager> accountManager = 
      do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr<nsISupportsArray> servers; 
    rv = accountManager->GetServersForIdentity(this, getter_AddRefs(servers));
    NS_ENSURE_SUCCESS(rv,rv);
    PRUint32 cnt = 0;
    servers->Count(&cnt);
    if (cnt > 0)
    {
      nsCOMPtr<nsISupports> supports = getter_AddRefs(servers->ElementAt(0));
      nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(supports,&rv);
      if (NS_SUCCEEDED(rv))
        server->ClearTemporaryReturnReceiptsFilter(); // okay to fail; no need to check for return code
    }
    folderflag = MSG_FOLDER_FLAG_SENTMAIL;
  }
  else if (nsCRT::strcmp(prefname, "draft_folder") == 0)
    folderflag = MSG_FOLDER_FLAG_DRAFTS;
  else if (nsCRT::strcmp(prefname, "stationery_folder") == 0)
    folderflag = MSG_FOLDER_FLAG_TEMPLATES;
  else
    return NS_ERROR_FAILURE;
  
  // get the old folder, and clear the special folder flag on it
  rv = getFolderPref(prefname, getter_Copies(oldpref), PR_FALSE);
  if (NS_SUCCEEDED(rv) && !oldpref.IsEmpty())
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
  if (NS_SUCCEEDED(rv) && value && *value)
  {
    rv = rdf->GetResource(nsDependentCString(value), getter_AddRefs(res));
    if (NS_SUCCEEDED(rv) && res)
    {
      folder = do_QueryInterface(res, &rv);
      if (NS_SUCCEEDED(rv))
        rv = folder->SetFlag(folderflag);
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgIdentity::SetUnicharAttribute(const char *aName, const PRUnichar *val)
{
  return setUnicharPref(aName, val);
}

NS_IMETHODIMP nsMsgIdentity::GetUnicharAttribute(const char *aName, PRUnichar **val)
{
  return getUnicharPref(aName, val);
}

NS_IMETHODIMP nsMsgIdentity::SetCharAttribute(const char *aName, const char *val)
{
  return setCharPref(aName, val);
}

NS_IMETHODIMP nsMsgIdentity::GetCharAttribute(const char *aName, char **val)
{
  return getCharPref(aName, val);
}

NS_IMETHODIMP nsMsgIdentity::SetBoolAttribute(const char *aName, PRBool val)
{
  return setBoolPref(aName, val);
}

NS_IMETHODIMP nsMsgIdentity::GetBoolAttribute(const char *aName, PRBool *val)
{
  return getBoolPref(aName, val);
}

NS_IMETHODIMP nsMsgIdentity::SetIntAttribute(const char *aName, PRInt32 val)
{
  return setIntPref(aName, val);
}

NS_IMETHODIMP nsMsgIdentity::GetIntAttribute(const char *aName, PRInt32 *val)
{
  return getIntPref(aName, val);
}

#define COPY_IDENTITY_FILE_VALUE(SRC_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
		nsresult macro_rv;	\
		nsCOMPtr <nsILocalFile>macro_spec;   \
        	macro_rv = SRC_ID->MACRO_GETTER(getter_AddRefs(macro_spec)); \
        	if (NS_SUCCEEDED(macro_rv)) \
        	  this->MACRO_SETTER(macro_spec);     \
	}

#define COPY_IDENTITY_INT_VALUE(SRC_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
		    nsresult macro_rv;	\
        	PRInt32 macro_oldInt;	\
        	macro_rv = SRC_ID->MACRO_GETTER(&macro_oldInt);	\
        	if (NS_SUCCEEDED(macro_rv)) \
        	  this->MACRO_SETTER(macro_oldInt);     \
	}

#define COPY_IDENTITY_BOOL_VALUE(SRC_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
		    nsresult macro_rv;	\
        	PRBool macro_oldBool;	\
        	macro_rv = SRC_ID->MACRO_GETTER(&macro_oldBool);	\
        	if (NS_SUCCEEDED(macro_rv)) \
        	  this->MACRO_SETTER(macro_oldBool);     \
	}

#define COPY_IDENTITY_STR_VALUE(SRC_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
        	nsXPIDLCString macro_oldStr;	\
		    nsresult macro_rv;	\
        	macro_rv = SRC_ID->MACRO_GETTER(getter_Copies(macro_oldStr));	\
            if (NS_SUCCEEDED(macro_rv)) { \
        	  if (!macro_oldStr) {	\
                	this->MACRO_SETTER("");	\
              }	\
        	  else {	\
                  	this->MACRO_SETTER(macro_oldStr);	\
              }	\
            } \
	}

static const PRUnichar unicharEmptyString[] = { (PRUnichar)'\0' };

#define COPY_IDENTITY_WSTR_VALUE(SRC_ID,MACRO_GETTER,MACRO_SETTER) 	\
	{	\
        	nsXPIDLString macro_oldStr;	\
		    nsresult macro_rv;	\
        	macro_rv = SRC_ID->MACRO_GETTER(getter_Copies(macro_oldStr)); \
        	if (NS_SUCCEEDED(macro_rv)) { \
        	  if (!macro_oldStr) {	\
                	this->MACRO_SETTER(unicharEmptyString);	\
              }	\
        	  else {	\
                	this->MACRO_SETTER(macro_oldStr);	\
              }	\
            } \
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
    COPY_IDENTITY_STR_VALUE(identity,GetFccFolder,SetFccFolder)
    COPY_IDENTITY_STR_VALUE(identity,GetStationeryFolder,SetStationeryFolder)
    COPY_IDENTITY_BOOL_VALUE(identity,GetAttachSignature,SetAttachSignature)
    COPY_IDENTITY_FILE_VALUE(identity,GetSignature,SetSignature)
    COPY_IDENTITY_BOOL_VALUE(identity,GetAutoQuote,SetAutoQuote)
    COPY_IDENTITY_INT_VALUE(identity,GetReplyOnTop,SetReplyOnTop)
    COPY_IDENTITY_BOOL_VALUE(identity,GetSigBottom,SetSigBottom)
    COPY_IDENTITY_INT_VALUE(identity,GetSignatureDate,SetSignatureDate)
    COPY_IDENTITY_BOOL_VALUE(identity,GetAttachVCard,SetAttachVCard)
    COPY_IDENTITY_STR_VALUE(identity,GetEscapedVCard,SetEscapedVCard)
    COPY_IDENTITY_STR_VALUE(identity,GetSmtpServerKey,SetSmtpServerKey)
    return NS_OK;
}

NS_IMETHODIMP
nsMsgIdentity::GetRequestReturnReceipt(PRBool *aVal)
{
  NS_ENSURE_ARG_POINTER(aVal);

  PRBool useCustomPrefs = PR_FALSE;
  nsresult rv = GetBoolAttribute("use_custom_prefs", &useCustomPrefs);
  NS_ENSURE_SUCCESS(rv, rv);
  if (useCustomPrefs)
    return GetBoolAttribute("request_return_receipt_on", aVal);

  rv = getPrefService();
  NS_ENSURE_SUCCESS(rv, rv);
  return m_prefBranch->GetBoolPref("mail.receipt.request_return_receipt_on", aVal);
}

NS_IMETHODIMP
nsMsgIdentity::GetReceiptHeaderType(PRInt32 *aType)
{
  NS_ENSURE_ARG_POINTER(aType);

  PRBool useCustomPrefs = PR_FALSE;
  nsresult rv = GetBoolAttribute("use_custom_prefs", &useCustomPrefs);
  NS_ENSURE_SUCCESS(rv, rv);
  if (useCustomPrefs)
    return GetIntAttribute("request_receipt_header_type", aType);

  rv = getPrefService();
  NS_ENSURE_SUCCESS(rv, rv);
  return m_prefBranch->GetIntPref("mail.receipt.request_header_type", aType);
}
