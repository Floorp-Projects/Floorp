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
 * Netscape Communications Corporation
 *
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *       Rajiv Dayal <rdayal@netscape.com>
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

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <memory.h>
#include <sys/stat.h>
#include <TCHAR.H>

#include <syncmgr.h>
#include <Pgenerr.h>

#include "MozABConduitRecord.h"
#include "MozABPCManager.h"

const IID IID_IPalmSync = {0xC8CE6FC1,0xCCF1,0x11d6,{0xB8,0xA5,0x00,0x00,0x64,0x65,0x73,0x74}};
const CLSID CLSID_CPalmSyncImp = { 0xb20b4521, 0xccf8, 0x11d6, { 0xb8, 0xa5, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } };

extern DWORD tId;

/* static */BOOL MozABPCManager::gUseHomeAddress;
/* static */ BOOL MozABPCManager::gPreferHomePhone;


BOOL MozABPCManager::InitMozPalmSyncInstance(IPalmSync **aRetValue)
{
    // Check wehther this thread has a valid Interface
    // by looking into thread-specific-data variable

    *aRetValue = (IPalmSync *)TlsGetValue(tId);

    // Check whether the pointer actually resolves to
    // a valid method call; otherwise mozilla is not running

    if ((*aRetValue) && (*aRetValue)->IsValid() == S_OK)
         return TRUE;

    HRESULT hRes = ::CoInitialize(NULL) ;

    hRes = ::CoCreateInstance(CLSID_CPalmSyncImp, NULL, CLSCTX_LOCAL_SERVER,
                                         IID_IPalmSync, (LPVOID *)aRetValue);

    if (hRes == S_OK)
        if (TlsSetValue(tId, (LPVOID)(*aRetValue)))
        {
            (*aRetValue)->nsUseABHomeAddressForPalmAddress(&gUseHomeAddress);
            (*aRetValue)->nsPreferABHomePhoneForPalmPhone(&gPreferHomePhone);
            return TRUE;
        }

    // Either CoCreate or TlsSetValue failed; so return FALSE

    if ((*aRetValue))
        (*aRetValue)->Release();

    ::CoUninitialize();
    return FALSE;
}

// this function allocates the list as well as the strings, caller should free list and delete strings
long MozABPCManager::GetPCABList(DWORD * pCategoryCount, LONG ** pCategoryIndexList, CPString *** pCategoryNameList, CPString *** pCategoryUrlList, BOOL ** pDirFlags)
{
    lpnsMozABDesc mozABNameList=NULL;

    short  dwMozABCount=0;
    long retval = 0;
    IPalmSync *pNsPalmSync = NULL;

    // get the interface 
    if (!InitMozPalmSyncInstance(&pNsPalmSync))
        return GEN_ERR_NOT_SUPPORTED;

    // get the ABList
    HRESULT hres = pNsPalmSync->nsGetABList(FALSE, &dwMozABCount, 
                                        &mozABNameList, pCategoryIndexList, pDirFlags);
    if (hres != S_OK) {
        retval = (long) hres;
        return retval;
    }

    *pCategoryCount = dwMozABCount;

    CPString ** abNameList = (CPString **) malloc(sizeof(CPString *) * dwMozABCount);
    if (!abNameList) {
        free(mozABNameList);
        return GEN_ERR_LOW_MEMORY;
    }
    memset(abNameList, 0, sizeof(CPString *) * dwMozABCount);
    *pCategoryNameList = abNameList;

    CPString ** abUrlList = (CPString **) malloc(sizeof(CPString *) * dwMozABCount);
    if (!abUrlList) {
        free(mozABNameList);
        free(abNameList);
        return GEN_ERR_LOW_MEMORY;
    }
    memset(abUrlList, 0, sizeof(CPString *) * dwMozABCount);
    *pCategoryUrlList = abUrlList;

    for (int i=0; i < dwMozABCount; i++) {
        CPString * pABName = new CPString((LPCTSTR) mozABNameList[i].lpszABName);
        if (pABName)
            *abNameList = pABName;
        else
            return GEN_ERR_LOW_MEMORY;
        CoTaskMemFree(mozABNameList[i].lpszABName);
        abNameList++;

        CPString * pABUrl = new CPString((LPCTSTR) mozABNameList[i].lpszABUrl);
        if (pABUrl)
            *abUrlList = pABUrl;
        else
            return GEN_ERR_LOW_MEMORY;
        CoTaskMemFree(mozABNameList[i].lpszABUrl);
        abUrlList++;
    }
    
    CoTaskMemFree(mozABNameList);
    return retval;
}

// this function allocates the mozlist as well as the mozRecs, caller should free list and delete recs
long MozABPCManager::SynchronizePCAB(LONG categoryIndex, LONG categoryId, CPString & categoryName,
					DWORD updatedPalmRecCount, CPalmRecord ** updatedPalmRecList,
					DWORD * pUpdatedPCRecListCount, CPalmRecord *** updatedPCRecList)
{
    lpnsABCOMCardStruct mozCardList=NULL;  // freed by MSCOM/Mozilla.
    int       dwMozCardCount=0;
    long        retval = 0;
    IPalmSync     *pNsPalmSync = NULL;

    // get the interface 
    if (!InitMozPalmSyncInstance(&pNsPalmSync))
        return GEN_ERR_NOT_SUPPORTED;

    CMozABConduitRecord ** tempMozABConduitRecList = new CMozABConduitRecord*[updatedPalmRecCount];
    nsABCOMCardStruct * palmCardList = new nsABCOMCardStruct[updatedPalmRecCount];
    if(palmCardList)
    {
        for(DWORD i=0; i<updatedPalmRecCount; i++) 
        {
            if(*updatedPalmRecList)
            {
                CMozABConduitRecord * pConduitRecord = new CMozABConduitRecord(**updatedPalmRecList);
                memcpy(&palmCardList[i], &pConduitRecord->m_nsCard, sizeof(nsABCOMCardStruct));
                tempMozABConduitRecList[i]=pConduitRecord;
            }
            updatedPalmRecList++;
        }
        // synchronize and get the updated cards in MozAB
        HRESULT hres = pNsPalmSync->nsSynchronizeAB(FALSE, categoryIndex, categoryId, categoryName.GetBuffer(0),
                                                updatedPalmRecCount, palmCardList,
                                                &dwMozCardCount, &mozCardList);
        if(hres == S_OK && mozCardList) 
        {
            *pUpdatedPCRecListCount = dwMozCardCount;
            CPalmRecord ** mozRecordList = (CPalmRecord **) malloc(sizeof(CPalmRecord *) * dwMozCardCount);
            *updatedPCRecList = mozRecordList;
            if (mozRecordList) 
            {
                memset(mozRecordList, 0, sizeof(CPalmRecord *) * dwMozCardCount);
                int i=0;
                for (i=0; i < dwMozCardCount; i++) 
                {
                    CMozABConduitRecord * pConduitRecord = new CMozABConduitRecord(mozCardList[i]);
                    CPalmRecord * pMozRecord = new CPalmRecord;
                    pConduitRecord->ConvertToGeneric(*pMozRecord);
                    *mozRecordList = pMozRecord;
                    mozRecordList++;
                    delete pConduitRecord;
                    CMozABConduitRecord::CleanUpABCOMCardStruct(&mozCardList[i]);
                }
            }
            else
                retval = GEN_ERR_LOW_MEMORY;
            CoTaskMemFree(mozCardList);
        }
        else
            retval = (long) hres;
    }
    else
        retval = GEN_ERR_LOW_MEMORY;


        delete palmCardList;
    if(tempMozABConduitRecList) 
    {
        for(DWORD j=0; j<updatedPalmRecCount; j++)
            delete tempMozABConduitRecList[j];
        delete tempMozABConduitRecList;
    }

    return retval;
}

// this will add all records in a Palm category into a new or existing Mozilla AB 
long MozABPCManager::AddRecords(BOOL replaceExisting, LONG categoryIndex, CPString & categoryName,
					DWORD updatedPalmRecCount, CPalmRecord ** updatedPalmRecList)
{
    long        retval = 0;
    IPalmSync     *pNsPalmSync = NULL;

    // get the interface 
    if (!InitMozPalmSyncInstance(&pNsPalmSync))
        return GEN_ERR_NOT_SUPPORTED;

    CMozABConduitRecord ** tempMozABConduitRecList = new CMozABConduitRecord*[updatedPalmRecCount];
    nsABCOMCardStruct * palmCardList = new nsABCOMCardStruct[updatedPalmRecCount];
    if(palmCardList) 
    {
        for(DWORD i=0; i<updatedPalmRecCount; i++) 
        {
            if(*updatedPalmRecList) 
            {
                CMozABConduitRecord * pConduitRecord = new CMozABConduitRecord(**updatedPalmRecList);
                memcpy(&palmCardList[i], &pConduitRecord->m_nsCard, sizeof(nsABCOMCardStruct));
                tempMozABConduitRecList[i]=pConduitRecord;
            }
            updatedPalmRecList++;
        }
        // get the ABList
        HRESULT hres = pNsPalmSync->nsAddAllABRecords(FALSE, replaceExisting, categoryIndex, categoryName.GetBuffer(0),
                                                updatedPalmRecCount, palmCardList);
        if (hres != S_OK)
            retval = (long) hres;
    }
    else
        retval = GEN_ERR_LOW_MEMORY;

        delete palmCardList;
    if(tempMozABConduitRecList) 
    {

        for(DWORD i=0; i<updatedPalmRecCount; i++)
            delete tempMozABConduitRecList[i];
        delete tempMozABConduitRecList;
    }
    return retval;
}

bool MozABPCManager::PCABDeleted(CPString &ABName)
{
    IPalmSync     *pNsPalmSync = NULL;
    // get the interface 
    if (!InitMozPalmSyncInstance(&pNsPalmSync))
        return false;
    BOOL abDeleted;
    HRESULT hres = pNsPalmSync->nsGetABDeleted(ABName.GetBuffer(0), &abDeleted);
    return (hres == S_OK) ? abDeleted : false; // assume false;

}
// this load all records in an Moz AB
// this function allocates the mozlist as well as the mozRecs, caller should free list and delete recs
long MozABPCManager::LoadAllRecords(CPString & ABName, DWORD * pPCRecListCount, CPalmRecord *** pPCRecList)
{
    lpnsABCOMCardStruct mozCardList=NULL; // freed by MSCOM/Mozilla.
    int       dwMozCardCount=0;
    long        retval = 0;
    IPalmSync     *pNsPalmSync = NULL;

    // get the interface 
    if (!InitMozPalmSyncInstance(&pNsPalmSync))
        return GEN_ERR_NOT_SUPPORTED;

    // get the ABList
    HRESULT hres = pNsPalmSync->nsGetAllABCards(FALSE, -1, ABName.GetBuffer(0),
                                            &dwMozCardCount, &mozCardList);
    if (hres == S_OK && mozCardList) {
        *pPCRecListCount = dwMozCardCount;
        CPalmRecord ** mozRecordList = (CPalmRecord **) malloc(sizeof(CPalmRecord *) * dwMozCardCount);
        *pPCRecList = mozRecordList;
        if (mozRecordList) {
            memset(mozRecordList, 0, sizeof(CPalmRecord *) * dwMozCardCount);
            for (int i=0; i < dwMozCardCount; i++) {
                CMozABConduitRecord * pConduitRecord = new CMozABConduitRecord(mozCardList[i]);
                CPalmRecord * pMozRecord = new CPalmRecord;
                pConduitRecord->ConvertToGeneric(*pMozRecord);
                *mozRecordList = pMozRecord;
                mozRecordList++;
                delete pConduitRecord;
                CMozABConduitRecord::CleanUpABCOMCardStruct(&mozCardList[i]);
            }
        }
        else
            retval = GEN_ERR_LOW_MEMORY;
        CoTaskMemFree(mozCardList);
    }
    else
        retval = (long) hres;
    
    return retval;
}

long MozABPCManager::NotifySyncDone(BOOL success, LONG catIndex, DWORD newRecCount, DWORD * newRecIDList)
{
    IPalmSync     *pNsPalmSync = NULL;
    // get the interface 
    if (!InitMozPalmSyncInstance(&pNsPalmSync))
        return GEN_ERR_NOT_SUPPORTED;

    // MS COM Proxy stub Dll will not accept NULL for this address
    if(!newRecIDList)
        newRecIDList = &newRecCount;

    HRESULT hres = pNsPalmSync->nsAckSyncDone(success, catIndex, newRecCount, newRecIDList);
    long retval = (long) hres;

    return retval;
}

long MozABPCManager::UpdatePCABSyncInfo(LONG categoryIndex, CPString & categoryName)
{
    IPalmSync     *pNsPalmSync = NULL;
    // get the interface 
    if (!InitMozPalmSyncInstance(&pNsPalmSync))
        return GEN_ERR_NOT_SUPPORTED;

    HRESULT hres = pNsPalmSync->nsUpdateABSyncInfo(FALSE, categoryIndex, categoryName);
    long retval = (long) hres;

    return retval;
}

long MozABPCManager::DeletePCAB(LONG categoryIndex, CPString & categoryName, CPString & categoryUrl)
{
    IPalmSync     *pNsPalmSync = NULL;
    // get the interface 
    if (!InitMozPalmSyncInstance(&pNsPalmSync))
        return GEN_ERR_NOT_SUPPORTED;

    HRESULT hres = pNsPalmSync->nsDeleteAB(FALSE, categoryIndex, categoryName, categoryUrl);
    long retval = (long) hres;

    return retval;
}

long MozABPCManager::RenamePCAB(LONG categoryIndex, CPString & categoryName, CPString & categoryUrl)
{
    IPalmSync     *pNsPalmSync = NULL;
    // get the interface 
    if (!InitMozPalmSyncInstance(&pNsPalmSync))
        return GEN_ERR_NOT_SUPPORTED;

    HRESULT hres = pNsPalmSync->nsRenameAB(FALSE, categoryIndex, categoryName, categoryUrl);
    return (long) hres;
}
