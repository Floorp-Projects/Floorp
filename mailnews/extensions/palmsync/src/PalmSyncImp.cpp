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

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"

#include "IPalmSync.h"
#include "PalmSyncImp.h"
#include "PalmSyncFactory.h"

#include "nsAbPalmSync.h"
#include "nsMsgI18N.h"
#include "nsUnicharUtils.h"


CPalmSyncImp::CPalmSyncImp()
: m_cRef(1),
  m_PalmHotSync(nsnull),
  m_ServerDescList(nsnull),
  m_FirstTimeSyncList(nsnull),
  m_CatIDList(nsnull)
{

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
                        lpnsMozABDesc * aABList, long ** aABCatIDList, BOOL ** aFirstTimeSyncList)
{
    if (!DIR_GetDirectories())
        return E_FAIL;

    PRInt16 count = DIR_GetDirectories()->Count();
    // freed by MSCOM??
    m_ServerDescList = (lpnsMozABDesc) CoTaskMemAlloc(sizeof(nsMozABDesc) * count);
    m_FirstTimeSyncList = (BOOL *) CoTaskMemAlloc(sizeof(BOOL) * count);
    m_CatIDList = (long *) CoTaskMemAlloc(sizeof(long) * count);

    *aABListCount = count;
    *aABList = m_ServerDescList;
    *aFirstTimeSyncList = m_FirstTimeSyncList;
    *aABCatIDList = m_CatIDList;

    nsresult rv=NS_OK;
    for (PRInt16 i = 0; i < count; i++)
    {
        DIR_Server *server = (DIR_Server *)(DIR_GetDirectories()->ElementAt(i));

        // if this is a 4.x, local .na2 addressbook (PABDirectory)
        // we must skip it.
        PRUint32 fileNameLen = strlen(server->fileName);
        if (((fileNameLen > kABFileName_PreviousSuffixLen) && 
                strcmp(server->fileName + fileNameLen - kABFileName_PreviousSuffixLen, kABFileName_PreviousSuffix) == 0) &&
                (server->dirType == PABDirectory))
            continue;

        // server->description is represented in UTF8, we need to do some conversion...
        if(aIsUnicode) {
            // convert to Unicode
            nsAutoString abName;
            rv = ConvertToUnicode("UTF-8", server->description, abName);
            if (NS_FAILED(rv))
                break;
            // add to the list
            m_ServerDescList->lpszABDesc = (LPTSTR) CoTaskMemAlloc(sizeof(PRUnichar) * (abName.Length()+1));
            wcscpy(m_ServerDescList->lpszABDesc, abName.get());
        }
        else {
            // we need to convert the description from UTF-8 to Unicode and then to ASCII
            nsAutoString abUName;
            rv = ConvertToUnicode("UTF-8", server->description, abUName);
            if (NS_FAILED(rv))
                break;
            nsCAutoString abName = NS_LossyConvertUCS2toASCII(abUName);

            m_ServerDescList->lpszABDesc = (LPTSTR) CoTaskMemAlloc(sizeof(char) * (abName.Length()+1));
            strcpy((char*)m_ServerDescList->lpszABDesc, abName.get());
        }
        m_ServerDescList++;

        *m_FirstTimeSyncList = (server->PalmSyncTimeStamp <= 0);
        m_FirstTimeSyncList++;

        *m_CatIDList = server->PalmCategoryId;
        m_CatIDList++;
    }

    // assign member variables to the beginning of the list
    m_ServerDescList = *aABList;
    m_FirstTimeSyncList = *aFirstTimeSyncList;
    m_CatIDList = *aABCatIDList;

    if(NS_FAILED(rv))
        return E_FAIL;
    
    return S_OK;
}


// Synchronize the Address Book represented by the aCategoryId and/or corresponding aABName in Mozilla
STDMETHODIMP CPalmSyncImp::nsSynchronizeAB(BOOL aIsUnicode, unsigned long aCategoryId, LPTSTR aABName,
                    int aModRemoteRecCount, lpnsABCOMCardStruct aModRemoteRecList,
                    int * aModMozRecCount, lpnsABCOMCardStruct * aModMozRecList)
{
    // let the already continuing sync complete
    if(m_PalmHotSync)
        return E_FAIL;

    m_PalmHotSync = (nsAbPalmHotSync *) new nsAbPalmHotSync(aIsUnicode, aABName, (char*)aABName, aCategoryId);
    if(!m_PalmHotSync)
        return E_FAIL;

    nsresult rv = ((nsAbPalmHotSync *)m_PalmHotSync)->Initialize();
    if (NS_SUCCEEDED(rv))
        rv = ((nsAbPalmHotSync *)m_PalmHotSync)->DoSyncAndGetUpdatedCards(aModRemoteRecCount, aModRemoteRecList, aModMozRecCount, aModMozRecList);

    if (NS_FAILED(rv))
        return E_FAIL;

    return S_OK;
}

// All records from a AB represented by aCategoryId and aABName into a new Mozilla AB
STDMETHODIMP CPalmSyncImp::nsAddAllABRecords(BOOL aIsUnicode, unsigned long aCategoryId, LPTSTR aABName,
                            int aRemoteRecCount, lpnsABCOMCardStruct aRemoteRecList)
{
    // since we are not returning any data we donot need to keep the nsAbPalmHotSync reference
    // in order to free the returned data in its destructor. Just create a local nsAbPalmHotSync var.
    nsAbPalmHotSync palmHotSync(aIsUnicode, aABName, (char*)aABName, aCategoryId);

    nsresult rv = palmHotSync.AddAllRecordsInNewAB(aRemoteRecCount, aRemoteRecList);

    if (NS_FAILED(rv))
        return E_FAIL;

    return S_OK;
}

// Get All records from a Mozilla AB
STDMETHODIMP CPalmSyncImp::nsGetAllABCards(BOOL aIsUnicode, unsigned long aCategoryId, LPTSTR aABName,
                            int * aMozRecCount, lpnsABCOMCardStruct * aMozRecList)
{
    // if sync is already going on wait for AckSyncDone to be called
    if(m_PalmHotSync)
        return E_FAIL;

    m_PalmHotSync = (nsAbPalmHotSync *) new nsAbPalmHotSync(aIsUnicode, aABName, (char*)aABName, aCategoryId);
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
STDMETHODIMP CPalmSyncImp::nsAckSyncDone(BOOL aIsSuccess, int aCatID, int aNewRecCount, unsigned long * aNewPalmRecIDList)
{
    nsAbPalmHotSync * tempPalmHotSync = (nsAbPalmHotSync *) m_PalmHotSync;
    if(tempPalmHotSync) {
        if(aNewRecCount && (aCatID > -1))
            tempPalmHotSync->Done(aIsSuccess, aCatID, aNewRecCount, aNewPalmRecIDList);
        delete tempPalmHotSync;
    }
    m_PalmHotSync = nsnull;

    return S_OK;
}


