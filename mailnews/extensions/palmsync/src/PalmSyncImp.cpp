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
 * The Original Code is Mozilla
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *                 Rajiv Dayal (rdayal@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"

#include "IPalmSync.h"
#include "PalmSyncImp.h"
#include "PalmSyncFactory.h"

#include "nsAbPalmSync.h"
#include "nsMsgI18N.h"
#include "nsUnicharUtils.h"
#include "nsRDFResource.h"
#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "nsIAbDirectory.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPref.h"
#include "nsIPrefBranchInternal.h"

#include "nspr.h"
PRLogModuleInfo *PALMSYNC;

#define kPABDirectory  2  // defined in nsDirPrefs.h
#define kMAPIDirectory 3

CPalmSyncImp::CPalmSyncImp()
: m_cRef(1),
  m_PalmHotSync(nsnull)
{
  if (!PALMSYNC)
    PALMSYNC = PR_NewLogModule("PALMSYNC");


}

CPalmSyncImp::~CPalmSyncImp() 
{ 

}

STDMETHODIMP CPalmSyncImp::QueryInterface(const IID& aIid, void** aPpv)
{    
    if (aIid == IID_IUnknown)
    {
        *aPpv = static_cast<IPalmSync*>(this); 
    }
    else if (aIid == IID_IPalmSync)
    {
        *aPpv = static_cast<IPalmSync*>(this);
    }
    else
    {
        *aPpv = nsnull;
        return E_NOINTERFACE;
    }

    reinterpret_cast<IUnknown*>(*aPpv)->AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CPalmSyncImp::AddRef()
{
    return PR_AtomicIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CPalmSyncImp::Release() 
{
    PRInt32 temp;
    temp = PR_AtomicDecrement(&m_cRef);
    if (m_cRef == 0)
    {
        delete this;
        return 0;
    }

    return temp;
}

STDMETHODIMP CPalmSyncImp::IsValid()
{
    return S_OK;
}

// Get the list of Address Books for the currently logged in user profile
STDMETHODIMP CPalmSyncImp::nsGetABList(BOOL aIsUnicode, short * aABListCount,
                        lpnsMozABDesc * aABList, long ** aABCatIndexList, BOOL ** aDirFlagsList)
{
  if (!aABListCount || !aABList || !aABCatIndexList ||!aDirFlagsList)
        return E_FAIL;
  *aABListCount = 0;

  nsresult rv;
  nsCOMPtr<nsIRDFService> rdfService = do_GetService (NS_RDF_CONTRACTID "/rdf-service;1", &rv);
  if(NS_FAILED(rv)) return E_FAIL;

  // Parent nsIABDirectory is "moz-abdirectory://".
  nsCOMPtr <nsIRDFResource> resource;
  rv = rdfService->GetResource(NS_LITERAL_CSTRING("moz-abdirectory://"), getter_AddRefs(resource));
  if(NS_FAILED(rv)) return E_FAIL;

  nsCOMPtr <nsIAbDirectory> directory = do_QueryInterface(resource, &rv);
  if(NS_FAILED(rv)) return E_FAIL;

  nsCOMPtr<nsISimpleEnumerator> subDirectories;
  if (NS_SUCCEEDED(directory->GetChildNodes(getter_AddRefs(subDirectories))) && subDirectories)
  {
    // Get the total number of addrbook.
    PRInt16 count=0;
    nsCOMPtr<nsISupports> item;
    PRBool hasMore;
    while (NS_SUCCEEDED(rv = subDirectories->HasMoreElements(&hasMore)) && hasMore)
    {
        if (NS_SUCCEEDED(subDirectories->GetNext(getter_AddRefs(item))))
        {
          nsCOMPtr <nsIAbDirectory> subDirectory = do_QueryInterface(item, &rv);
          if (NS_SUCCEEDED(rv))
          {
              nsCOMPtr <nsIAbDirectoryProperties> properties;
              nsXPIDLCString fileName;
              rv = subDirectory->GetDirectoryProperties(getter_AddRefs(properties));
              if(NS_FAILED(rv)) 
                continue;
              rv = properties->GetFileName(getter_Copies(fileName));
              if(NS_FAILED(rv)) 
                continue;
              PRUint32 dirType;
              rv = properties->GetDirType(&dirType);
              nsCAutoString prefName;
              subDirectory->GetDirPrefId(prefName);
              prefName.Append(".disablePalmSync");
              PRBool disableThisAB = GetBoolPref(prefName.get(), PR_FALSE);
              // Skip/Ignore 4.X addrbooks (ie, with ".na2" extension), and non personal AB's
              if (disableThisAB || ((fileName.Length() > kABFileName_PreviousSuffixLen) && 
                   strcmp(fileName.get() + fileName.Length() - kABFileName_PreviousSuffixLen, kABFileName_PreviousSuffix) == 0) ||
                    (dirType != kPABDirectory && dirType != kMAPIDirectory))
                    continue;
          }
        }
        count++;
    }

    if (!count)
      return E_FAIL;  // should not happen but just in case.

    lpnsMozABDesc serverDescList = (lpnsMozABDesc) CoTaskMemAlloc(sizeof(nsMozABDesc) * count);
    BOOL *dirFlagsList = (BOOL *) CoTaskMemAlloc(sizeof(BOOL) * count);
    long *catIndexList = (long *) CoTaskMemAlloc(sizeof(long) * count);

    *aABListCount = count;
    *aABList = serverDescList;
    *aDirFlagsList = dirFlagsList;
    *aABCatIndexList = catIndexList;

    directory->GetChildNodes(getter_AddRefs(subDirectories)); // reset enumerator
    // For each valid addrbook collect info.
    while (NS_SUCCEEDED(rv = subDirectories->HasMoreElements(&hasMore)) && hasMore)
    {
      if (NS_SUCCEEDED(subDirectories->GetNext(getter_AddRefs(item))))
      {
        directory = do_QueryInterface(item, &rv);
        if (NS_SUCCEEDED(rv))
        {
          // We don't have to skip mailing list since there's no mailing lists at the top level.
          nsCOMPtr <nsIAbDirectoryProperties> properties;
          rv = directory->GetDirectoryProperties(getter_AddRefs(properties));
          if(NS_FAILED(rv)) return E_FAIL;

          nsXPIDLCString fileName, uri;
          nsAutoString description;
          PRUint32 dirType, palmSyncTimeStamp;
          PRInt32 palmCategoryIndex;

          rv = properties->GetDescription(description);
          if(NS_FAILED(rv)) return E_FAIL;
          rv = properties->GetFileName(getter_Copies(fileName));
          if(NS_FAILED(rv)) return E_FAIL;
          rv = properties->GetURI(getter_Copies(uri));
          if(NS_FAILED(rv)) return E_FAIL;
          rv = properties->GetDirType(&dirType);
          if(NS_FAILED(rv)) return E_FAIL;
          rv = properties->GetSyncTimeStamp(&palmSyncTimeStamp);
          if(NS_FAILED(rv)) return E_FAIL;
          rv = properties->GetCategoryId(&palmCategoryIndex);
          if(NS_FAILED(rv)) return E_FAIL;
          nsCAutoString prefName;
          directory->GetDirPrefId(prefName);
          prefName.Append(".disablePalmSync");
          PRBool disableThisAB = GetBoolPref(prefName.get(), PR_FALSE);
          // Skip/Ignore 4.X addrbooks (ie, with ".na2" extension), and non personal AB's
          if (disableThisAB || ((fileName.Length() > kABFileName_PreviousSuffixLen) && 
               strcmp(fileName.get() + fileName.Length() - kABFileName_PreviousSuffixLen, kABFileName_PreviousSuffix) == 0) ||
                (dirType != kPABDirectory && dirType != kMAPIDirectory))
          {
            continue;
          }

          if(aIsUnicode) 
          {
            // convert uri to Unicode
            nsAutoString abUrl;
            rv = ConvertToUnicode("UTF-8", uri, abUrl);
            if (NS_FAILED(rv))
              break;
            // add to the list
            CopyUnicodeString(&(serverDescList->lpszABName), description);
            CopyUnicodeString(&(serverDescList->lpszABUrl), abUrl);
          }
          else {
            // we need to convert uri to Unicode and then to ASCII
            nsAutoString abUUrl;

            rv = ConvertToUnicode("UTF-8", uri, abUUrl);
            if (NS_FAILED(rv))
              break;

            CopyCString(&(serverDescList->lpszABName),
                        NS_ConvertUTF16toUTF8(description));
            CopyCString(&(serverDescList->lpszABUrl),
                        NS_ConvertUTF16toUTF8(abUUrl));
          }
          serverDescList++;

          PRUint32 dirFlag = 0;
          if (palmSyncTimeStamp <= 0)
            dirFlag |= kFirstTimeSyncDirFlag;
          // was this the pab?
          if (prefName.Equals("ldap_2.servers.pab.disablePalmSync"))
            dirFlag |= kIsPabDirFlag;
          *dirFlagsList = (BOOL) dirFlag;
          dirFlagsList++;

          *catIndexList = palmCategoryIndex;
          catIndexList++;
        }
      }
    }

    // assign member variables to the beginning of the list
    serverDescList = *aABList;
    dirFlagsList = *aDirFlagsList;
    catIndexList = *aABCatIndexList;

    if(NS_FAILED(rv))
      return E_FAIL;
  }
  return S_OK;
}

// Synchronize the Address Book represented by the aCategoryIndex and/or corresponding aABName in Mozilla
STDMETHODIMP CPalmSyncImp::nsSynchronizeAB(BOOL aIsUnicode, long aCategoryIndex,
                    long aCategoryId, LPTSTR aABName,
                    int aModRemoteRecCount, lpnsABCOMCardStruct aModRemoteRecList,
                    int * aModMozRecCount, lpnsABCOMCardStruct * aModMozRecList)
{
    // let the already continuing sync complete
    if(m_PalmHotSync)
        return E_FAIL;

    m_PalmHotSync = (nsAbPalmHotSync *) new nsAbPalmHotSync(aIsUnicode, aABName, (char*)aABName, aCategoryIndex, aCategoryId);
    if(!m_PalmHotSync)
        return E_FAIL;

    nsresult rv = ((nsAbPalmHotSync *)m_PalmHotSync)->Initialize();
    if (NS_SUCCEEDED(rv))
        rv = ((nsAbPalmHotSync *)m_PalmHotSync)->DoSyncAndGetUpdatedCards(aModRemoteRecCount, aModRemoteRecList, aModMozRecCount, aModMozRecList);

    if (NS_FAILED(rv))
        return E_FAIL;

    return S_OK;
}

// All records from a AB represented by aCategoryIndex and aABName into a new Mozilla AB
STDMETHODIMP CPalmSyncImp::nsAddAllABRecords(BOOL aIsUnicode, BOOL replaceExisting, long aCategoryIndex, LPTSTR aABName,
                            int aRemoteRecCount, lpnsABCOMCardStruct aRemoteRecList)
{
    // since we are not returning any data we don't need to keep the nsAbPalmHotSync reference
    // in order to free the returned data in its destructor. Just create a local nsAbPalmHotSync var.
    nsAbPalmHotSync palmHotSync(aIsUnicode, aABName, (const char*)aABName, aCategoryIndex, -1);

    nsresult rv = palmHotSync.AddAllRecordsToAB(replaceExisting, aRemoteRecCount, aRemoteRecList);

    if (NS_FAILED(rv))
        return E_FAIL;

    return S_OK;
}

STDMETHODIMP CPalmSyncImp::nsGetABDeleted(LPTSTR aABName, BOOL *abDeleted)
{
  nsCAutoString prefName("ldap_2.servers.");
  prefName.Append((const char *)aABName);
  prefName.Append(".position");
  PRInt32 position = GetIntPref(prefName.get(), -1);
  *abDeleted = (position == 0);
  return S_OK;
}


// Get All records from a Mozilla AB
STDMETHODIMP CPalmSyncImp::nsGetAllABCards(BOOL aIsUnicode, long aCategoryIndex, LPTSTR aABName,
                            int * aMozRecCount, lpnsABCOMCardStruct * aMozRecList)
{
    // if sync is already going on wait for AckSyncDone to be called
    if(m_PalmHotSync)
        return E_FAIL;

    m_PalmHotSync = (nsAbPalmHotSync *) new nsAbPalmHotSync(aIsUnicode, aABName, (char*)aABName, aCategoryIndex, -1);
    if(!m_PalmHotSync)
        return E_FAIL;

    nsresult rv = ((nsAbPalmHotSync *)m_PalmHotSync)->Initialize();
    if (NS_SUCCEEDED(rv))
        rv = ((nsAbPalmHotSync *)m_PalmHotSync)->GetAllCards(aMozRecCount, aMozRecList);

    if (NS_FAILED(rv))
        return E_FAIL;

    return S_OK;
}

// Send an ack for sync done on the remote AB to Mozilla, 
// now we delete the data returned to the calling app.
STDMETHODIMP CPalmSyncImp::nsAckSyncDone(BOOL aIsSuccess, long aCatIndex, int aNewRecCount, unsigned long * aNewPalmRecIDList)
{
    nsAbPalmHotSync * tempPalmHotSync = (nsAbPalmHotSync *) m_PalmHotSync;
    if(tempPalmHotSync) {
        if(aNewRecCount && (aCatIndex > -1))
            tempPalmHotSync->Done(aIsSuccess, aCatIndex, aNewRecCount, aNewPalmRecIDList);
        delete tempPalmHotSync;
    }
    m_PalmHotSync = nsnull;

    return S_OK;
}

// Update the category id and mod tiem for the Address Book in Mozilla
STDMETHODIMP CPalmSyncImp::nsUpdateABSyncInfo(BOOL aIsUnicode, long aCategoryIndex, LPTSTR aABName)
{
  nsresult rv;
  if(m_PalmHotSync)
    rv = ((nsAbPalmHotSync *)m_PalmHotSync)->UpdateSyncInfo(aCategoryIndex);
  else
  {
    // Launch another ABpalmHotSync session.
    nsAbPalmHotSync palmHotSync(aIsUnicode, aABName, (const char*)aABName, aCategoryIndex, -1);
    rv = palmHotSync.Initialize();
    if (NS_SUCCEEDED(rv))
      rv = palmHotSync.UpdateSyncInfo(aCategoryIndex);
  }

  if (NS_FAILED(rv))
    return E_FAIL;

  return S_OK;
}

// Delete an Address Book in Mozilla
STDMETHODIMP CPalmSyncImp::nsDeleteAB(BOOL aIsUnicode, long aCategoryIndex, LPTSTR aABName, LPTSTR aABUrl)
{
  // This is an independent operation so use a local nsAbPalmHotSync var
  // (ie the callers don't need to call AckSyncdone after this is done).
  nsAbPalmHotSync palmHotSync(aIsUnicode, aABName, (const char*)aABName, aCategoryIndex, -1);

  nsresult rv = palmHotSync.DeleteAB(aCategoryIndex, (const char*)aABUrl);

  if (NS_FAILED(rv))
      return E_FAIL;

  return S_OK;
}

// Rename an Address Book in Mozilla
STDMETHODIMP CPalmSyncImp::nsRenameAB(BOOL aIsUnicode, long aCategoryIndex, LPTSTR aABName, LPTSTR aABUrl)
{
  // This is an independent operation so use a local nsAbPalmHotSync var
  // (ie the callers don't need to call AckSyncdone after this is done).
  nsAbPalmHotSync palmHotSync(aIsUnicode, aABName, (const char*)aABName, aCategoryIndex, -1);

  nsresult rv = palmHotSync.RenameAB(aCategoryIndex, (const char*)aABUrl);

  if (NS_FAILED(rv))
    return E_FAIL;

  return S_OK;
}

void CPalmSyncImp::CopyUnicodeString(LPTSTR *destStr, const nsAFlatString& srcStr)
{
  if (!destStr)
    return;

  PRInt32 length = srcStr.Length()+1;
  *destStr = (LPTSTR) CoTaskMemAlloc(sizeof(PRUnichar) * length);
  wcsncpy(*destStr, srcStr.get(), length-1);
  (*destStr)[length-1] = '\0';
}

void CPalmSyncImp::CopyCString(LPTSTR *destStr, const nsAFlatCString& srcStr)
{
  if (!destStr)
    return;

  // these strings are defined as wide in the idl, so we need to add up to 3
  // bytes of 0 byte padding at the end (if the string is an odd number of
  // bytes long, we need one null byte to pad out the last char to a wide char
  // and then  two more nulls as a wide null terminator. 
  PRInt32 length = sizeof(char) * (srcStr.Length()+3);
  *destStr = (LPTSTR) CoTaskMemAlloc(length);
  char *sp = (char *)*destStr;
  strncpy(sp, srcStr.get(), length-1); // this will null fill the end of destStr
  sp[length-1] = '\0';
}


/* static */PRBool CPalmSyncImp::GetBoolPref(const char *prefName, PRBool defaultVal)
{
  PRBool boolVal = defaultVal;
  
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefBranch)
    prefBranch->GetBoolPref(prefName, &boolVal);
  return boolVal;
}

/* static */PRInt32 CPalmSyncImp::GetIntPref(const char *prefName, PRInt32 defaultVal)
{
  PRInt32 intVal = defaultVal;
  
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefBranch)
    prefBranch->GetIntPref(prefName, &intVal);
  return intVal;
}


STDMETHODIMP CPalmSyncImp::nsUseABHomeAddressForPalmAddress(BOOL *aUseHomeAddress)
{
  *aUseHomeAddress = nsUseABHomeAddressForPalmAddress();
  return S_OK;
}

STDMETHODIMP CPalmSyncImp::nsPreferABHomePhoneForPalmPhone(BOOL *aPreferHomePhone)
{
  *aPreferHomePhone = nsPreferABHomePhoneForPalmPhone();
  return S_OK;
}

/* static */ PRBool CPalmSyncImp::nsUseABHomeAddressForPalmAddress()
{
  static PRBool gGotAddressPref = PR_FALSE;
  static PRBool gUseHomeAddress;
  if (!gGotAddressPref)
  {
    gUseHomeAddress = GetBoolPref("mail.palmsync.useHomeAddress", PR_TRUE);
    gGotAddressPref = PR_TRUE;
  }
  return gUseHomeAddress;
}

/* static */ PRBool CPalmSyncImp::nsPreferABHomePhoneForPalmPhone()
{
  static PRBool gGotPhonePref = PR_FALSE;
  static PRBool gPreferHomePhone;
  if (!gGotPhonePref)
  {
    gPreferHomePhone = GetBoolPref("mail.palmsync.preferHomePhone", PR_TRUE);
    gGotPhonePref = PR_TRUE;
  }
  return gPreferHomePhone;
}
