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

#include "MozABHHManager.h"

MozABHHManager::MozABHHManager(DWORD dwGenericFlags, 
               char *pName, 
               DWORD dwCreator, 
               DWORD dwType, 
               WORD dbFlags, 
               WORD dbVersion, 
			   int  iCardNum,
               eSyncTypes syncType)
{
    m_hhDB = 0;
    
    m_rInfo.m_pBytes = NULL;
    m_rInfo.m_TotalBytes = 0;

    m_dwMaxRecordCount = 0;
    m_pCatMgr = NULL;


    m_szName[0] = '\0';
    SetName( pName);
    m_dwCreator = dwCreator;
    m_dwType    = dwType;
    m_wFlags    = dbFlags;
    m_wVersion  = dbVersion;
	m_CardNum = iCardNum;

    memset(&m_appInfo, 0, sizeof(CDbGenInfo));
}

MozABHHManager::~MozABHHManager()
{
    if (m_rInfo.m_pBytes != NULL) {
        delete m_rInfo.m_pBytes;
    }
    m_rInfo.m_pBytes = NULL;
    m_rInfo.m_TotalBytes = 0;

    if (m_appInfo.m_pBytes) {
        delete m_appInfo.m_pBytes;
        m_appInfo.m_pBytes = NULL;
    }

    if (m_pCatMgr) {
        delete m_pCatMgr;
        m_pCatMgr = NULL;
    }

    CloseDB(TRUE);
}


long MozABHHManager::AllocateDBInfo(CDbGenInfo &info, BOOL bClearData)
{
    BOOL bNew = FALSE;

    if (!info.m_pBytes) {
        info.m_pBytes = (BYTE *) new char[MAX_RECORD_SIZE];  // deleted in d'ctor or by caller
        if (!info.m_pBytes)
            return GEN_ERR_LOW_MEMORY;
        info.m_TotalBytes = MAX_RECORD_SIZE;
        bNew = TRUE;
    } 

    if ((bClearData) || (bNew)) {
        memset(info.m_pBytes, 0, info.m_TotalBytes);
    }
    return 0;
}

long MozABHHManager::GetInfoBlock(CDbGenInfo &info, BOOL bSortInfo)
{
    long  retval=0;

    // Allocate storage for app/sort info blocks
    retval = AllocateDBInfo(info);
    if (retval)
        return retval;

    _tcscpy((char*)info.m_FileName, m_szName);
    memset(info.m_pBytes, 0, info.m_TotalBytes);

    if (!bSortInfo) {
        // Read the AppInfo block
        retval = SyncReadDBAppInfoBlock(m_hhDB, info);
    } else {
        // Read the SortInfo block
        retval = SyncReadDBSortInfoBlock(m_hhDB, info);
    }
    if (retval) {// if error then clean up
        delete info.m_pBytes;
        info.m_pBytes = NULL;
        info.m_TotalBytes = 0;
    }

    return retval;
}


long MozABHHManager::GetAppInfo(CDbGenInfo &rInfo)
{
    long retval = 0;
    BYTE *pBuffer;

    pBuffer = rInfo.m_pBytes; 
    WORD wTotalBytes = rInfo.m_TotalBytes;
    memset(&rInfo, 0, sizeof(CDbGenInfo));
    if ((wTotalBytes > 0) && (pBuffer)) {
        memset(pBuffer, 0, wTotalBytes);
        rInfo.m_pBytes       = pBuffer; 
        rInfo.m_TotalBytes   = wTotalBytes;
    }

    retval = GetInfoBlock(rInfo);

    if (retval) { // if error cleanup
        if (rInfo.m_pBytes) {
            delete rInfo.m_pBytes;
            rInfo.m_pBytes = NULL;
            rInfo.m_TotalBytes   = 0;
        }
    }

    return retval;
}

long MozABHHManager::SetName( char *pName)
{
    if (m_hhDB)
        return GEN_ERR_DB_ALREADY_OPEN;

    WORD wLen;
    if (!pName){
        *m_szName = '\0';
        return 0;
    } 
    wLen = (WORD)strlen(pName);
    if (!wLen) {
        *m_szName = '\0';
        return 0;
    }
    if (wLen >= sizeof(m_szName))
        return GEN_ERR_DATA_TOO_LARGE;

    strcpy(m_szName, pName);
    return 0;
}

long MozABHHManager::LoadCategories(void)
{
    long retval=0;

    if (m_pCatMgr) {  // remove existing manager
        delete m_pCatMgr;
        m_pCatMgr = NULL;
    }

    if (m_appInfo.m_BytesRead == 0){
        retval = GetAppInfo(m_appInfo);
        if (retval)
            return retval;
    }

    if ((m_appInfo.m_BytesRead > 0) && (m_appInfo.m_pBytes)){
        m_pCatMgr = new CPCategoryMgr(m_appInfo.m_pBytes, m_appInfo.m_BytesRead); // deleted in d'ctor
        if (!m_pCatMgr)
            return GEN_ERR_LOW_MEMORY;
    } else {
        retval = GEN_ERR_NO_CATEGORIES;
    }
    return retval;
}

long MozABHHManager::AddCategory(CPCategory & cat)
{
    long retval=0;

    if(!m_pCatMgr)
        retval = LoadCategories();

    // generate a new ID and Add the category
    if(!retval) {
        for(int id=0; id<=MAX_CATEGORIES; id++)
            if(!m_pCatMgr->FindID(id))
                break;
        // if ID doesnot already exist
        if(id<MAX_CATEGORIES) {
            cat.SetID(MAKELONG(id, 0));
            retval = m_pCatMgr->Add(cat);
        }
        else
            retval = CAT_NO_ID;
    }

    if(!retval)
        m_pCatMgr->SetChanged();

    return retval;    
}

long MozABHHManager::DeleteCategory(DWORD dwCategory, BOOL bMoveToUnfiled)
{
    long retval=0;

    if ((dwCategory < 0) || (dwCategory >= MAX_CATEGORIES))
        return CAT_ERR_INDEX_OUT_OF_RANGE;

    BYTE sCategory = LOBYTE(LOWORD(dwCategory));

    if (!bMoveToUnfiled)
        retval = SyncPurgeAllRecsInCategory(m_hhDB, sCategory);
    else 
        retval = SyncChangeCategory(m_hhDB, sCategory, 0);
    return retval;
}

long MozABHHManager::ChangeCategory(DWORD dwOldCatIndex, DWORD dwNewCatIndex)
{
    long retval=0;

    if ((dwOldCatIndex < 0) || (dwOldCatIndex >= MAX_CATEGORIES))
        return CAT_ERR_INDEX_OUT_OF_RANGE;

    BYTE sCategory = LOBYTE(LOWORD(dwOldCatIndex));

    if ((dwNewCatIndex < 0) || (dwNewCatIndex >= MAX_CATEGORIES))
        return CAT_ERR_INDEX_OUT_OF_RANGE;

    BYTE sNewCategory = LOBYTE(LOWORD(dwNewCatIndex));

    retval = SyncChangeCategory(m_hhDB, sCategory, sNewCategory);
    return retval;
}

long MozABHHManager::CompactCategoriesToHH()
{
    long retval = 0;

    if ((!m_pCatMgr) || (!m_pCatMgr->IsChanged()))
        return 0;

    retval = AllocateDBInfo(m_appInfo);
    if (retval)
        return retval;

    DWORD dwRecSize;

    dwRecSize = m_appInfo.m_TotalBytes;

    retval = m_pCatMgr->Compact(m_appInfo.m_pBytes, &dwRecSize);
    if (!retval) {
        // more than just the categories may be stored in the app info structure
        // This code only replaces the category area.
        if (!m_appInfo.m_BytesRead)
            m_appInfo.m_BytesRead = LOWORD(dwRecSize);
    }

    if(!retval)
        retval = SyncWriteDBAppInfoBlock(m_hhDB, m_appInfo);

    return retval;
}

long MozABHHManager::OpenDB(BOOL bCreate)
{
    long retval=0;

    if (bCreate) {
       CDbCreateDB  createInfo;

        // Delete any existing DB with the same name
        SyncDeleteDB(m_szName, m_CardNum);

        memset(&createInfo, 0, sizeof(CDbCreateDB));

        createInfo.m_Creator     = m_dwCreator; 
        createInfo.m_Type        = m_dwType; 
        createInfo.m_Flags       = (eDbFlags) m_wFlags;
        createInfo.m_CardNo     = m_CardNum;  
        strcpy(createInfo.m_Name, m_szName);
        createInfo.m_Version    = m_wVersion;

        if ((retval = SyncCreateDB(createInfo)) == SYNCERR_NONE)
            m_hhDB = createInfo.m_FileHandle;
    }
    else 
    {
        retval = SyncOpenDB(m_szName, 
                            m_CardNum, 
                            m_hhDB,
                            eDbRead | eDbWrite);
        if ((retval) && (retval == SYNCERR_NOT_FOUND))
            retval = GEN_ERR_NO_HH_DB;
    } 

    return retval;
}

long MozABHHManager::CloseDB(BOOL bDontUpdate)
{
    long retval = 0;
    long retval2 = 0;

    if (m_hhDB == NULL) 
        return GEN_ERR_DB_NOT_OPEN;

    if (!bDontUpdate) {
        retval = SyncResetSyncFlags(m_hhDB);
    }
    retval2 = SyncCloseDB(m_hhDB);
    if (!retval)
        retval = retval2;
    m_hhDB = NULL;
    return retval;
}

long MozABHHManager::LoadAllUpdatedRecords(CPalmRecord ***ppRecordList, DWORD * pListSize)
{
    return LoadUpdatedRecords(0, ppRecordList, pListSize);
}

long MozABHHManager::LoadUpdatedRecordsInCategory(DWORD catIndex, CPalmRecord ***ppRecordList, DWORD * pListSize)
{
    return LoadUpdatedRecords(catIndex, ppRecordList, pListSize);
}

// this function allocates the list as well as the records, caller should free list and delete records
long MozABHHManager::LoadUpdatedRecords(DWORD catIndex, CPalmRecord ***ppRecordList, DWORD * pListSize)
{
    long retval=0;
    WORD dwRecCount=0;

    retval = SyncGetDBRecordCount(m_hhDB, dwRecCount);
    if (retval)
        return retval;

    // resset the iteration index
    retval = SyncResetRecordIndex(m_hhDB);
    if (retval)
        return retval;

    // allocate buffer initially for the total record size and adjust after getting records
    CPalmRecord ** palmRecordList = (CPalmRecord **) malloc(sizeof(CPalmRecord *) * dwRecCount);
    if (!palmRecordList)
        return GEN_ERR_LOW_MEMORY;
    memset(palmRecordList, 0, sizeof(CPalmRecord *) * dwRecCount);
    *ppRecordList = palmRecordList;

    CPalmRecord *pPalmRec;
    *pListSize = 0;
    while ((!retval) && (*pListSize < dwRecCount))  {
        retval = AllocateRawRecord();
        if(retval)
            break;
        m_rInfo.m_RecIndex = 0;
        if(catIndex >= 0) {
            m_rInfo.m_CatId = catIndex;
            retval = SyncReadNextModifiedRecInCategory(m_rInfo);
        }
        else
            retval = SyncReadNextModifiedRec(m_rInfo);
        if (!retval) {
            pPalmRec = new CPalmRecord(m_rInfo);
            if (pPalmRec) {
                *palmRecordList = pPalmRec;
                palmRecordList++;
                (*pListSize)++;
            } else {
                retval = GEN_ERR_LOW_MEMORY;
            }
        } 
    }

    // reallocate to the correct size
    if((*pListSize) != dwRecCount)
        *ppRecordList=(CPalmRecord **) realloc(*ppRecordList, sizeof(CPalmRecord *) * (*pListSize));

    if (retval == SYNCERR_FILE_NOT_FOUND) // if there are no more records
        retval = 0;

    return retval;
}

long MozABHHManager::AllocateRawRecord(void)
{
    if (m_rInfo.m_pBytes != NULL) {
        BYTE *pBytes;

        pBytes = m_rInfo.m_pBytes;
        memset(&m_rInfo, 0, sizeof(CRawRecordInfo));
        memset(pBytes, 0, MAX_RECORD_SIZE);

        m_rInfo.m_pBytes        = pBytes;
        m_rInfo.m_TotalBytes    = MAX_RECORD_SIZE;
        m_rInfo.m_FileHandle    = m_hhDB;
        return 0;
    }
    
    memset(&m_rInfo, 0, sizeof(CRawRecordInfo));
    m_rInfo.m_pBytes        = (BYTE *) new char[MAX_RECORD_SIZE];
    if (!m_rInfo.m_pBytes)
        return GEN_ERR_LOW_MEMORY;

    memset(m_rInfo.m_pBytes, 0, MAX_RECORD_SIZE);
    m_rInfo.m_TotalBytes    = MAX_RECORD_SIZE;
    m_rInfo.m_FileHandle    = m_hhDB;
    return 0;
}

// this function allocates the list as well as the records, caller should free list and delete records
long MozABHHManager::LoadAllRecords(CPalmRecord ***ppRecordList, DWORD * pListSize)
{
    long retval=0;
    WORD dwRecCount, dwIndex;

    retval = SyncGetDBRecordCount(m_hhDB, dwRecCount);
    if (!retval)
        return retval;
    *pListSize = dwRecCount;

    *ppRecordList = (CPalmRecord **) malloc(sizeof(CPalmRecord *) * dwRecCount);
    if (!*ppRecordList)
        return GEN_ERR_LOW_MEMORY;
    memset(*ppRecordList, 0, sizeof(CPalmRecord *) * dwRecCount);

    CPalmRecord *pPalmRec;
    for (dwIndex = 0; (dwIndex < dwRecCount) && (!retval); dwIndex++){
        retval = AllocateRawRecord();
        if(retval)
            break;

        m_rInfo.m_RecIndex = LOWORD(dwIndex);

        retval = SyncReadRecordByIndex(m_rInfo);
        if (!retval) {
            pPalmRec = new CPalmRecord(m_rInfo);
            if (pPalmRec) {
                *ppRecordList[dwIndex] = pPalmRec;
            } else {
                retval = GEN_ERR_LOW_MEMORY;
            }
        } else if (retval != SYNCERR_FILE_NOT_FOUND) {
            pPalmRec = new CPalmRecord(m_rInfo);
            pPalmRec->Initialize();
            pPalmRec->SetIndex(dwIndex);
            *ppRecordList[dwIndex] = pPalmRec;
        }
    }
    m_dwMaxRecordCount = dwIndex + 1;

    if (retval == SYNCERR_FILE_NOT_FOUND) // if there are no more records
        retval = 0;

    return retval;
}

// this function allocates the list as well as the records, caller should free list and delete records
long MozABHHManager::LoadAllRecordsInCategory(DWORD catIndex, CPalmRecord ***ppRecordList, DWORD * pListSize)
{
    long retval=0;
    WORD dwRecCount=0;

    retval = SyncGetDBRecordCount(m_hhDB, dwRecCount);
    if (retval)
        return retval;

    // resset the iteration index
    retval = SyncResetRecordIndex(m_hhDB);
    if (retval)
        return retval;

    // allocate buffer initially for the total record size and adjust after getting records
    CPalmRecord ** palmRecordList = (CPalmRecord **) malloc(sizeof(CPalmRecord *) * dwRecCount);
    if (!palmRecordList)
        return GEN_ERR_LOW_MEMORY;
    memset(palmRecordList, 0, sizeof(CPalmRecord *) * dwRecCount);
    *ppRecordList = palmRecordList;

    CPalmRecord *pPalmRec;

    *pListSize = 0;
    while ((!retval) && (*pListSize < dwRecCount))  {
        retval = AllocateRawRecord();
        if (retval) 
            break;
        m_rInfo.m_RecIndex = 0;
        m_rInfo.m_CatId = catIndex;
        retval = SyncReadNextRecInCategory(m_rInfo);
        if (!retval) {
            pPalmRec = new CPalmRecord(m_rInfo);
            if (pPalmRec) {
                *palmRecordList = pPalmRec;
                palmRecordList++;
                (*pListSize)++;
            } else {
                retval = GEN_ERR_LOW_MEMORY;
            }
        } 
    }

    // reallocate to the correct size
    if((*pListSize) != dwRecCount)
        *ppRecordList=(CPalmRecord **) realloc(*ppRecordList, sizeof(CPalmRecord *) * (*pListSize));

    if (retval == SYNCERR_FILE_NOT_FOUND) // if there are no more records
        retval = 0;

    return retval;
}

long MozABHHManager::AddRecords(CPalmRecord **pRecordList, DWORD pCount)
{
    long retval=0;

    for (unsigned long i=0; i < pCount; i++)
    {
        CPalmRecord palmRec = *pRecordList[i];

        palmRec.ResetAttribs();

        retval = AllocateRawRecord();
        if (retval) 
            return retval;

        retval = palmRec.ConvertToHH(m_rInfo);
        if (!retval) {
            m_rInfo.m_FileHandle = m_hhDB;
            m_rInfo.m_RecId = 0;
            if (m_bRecordDB)
                retval = SyncWriteRec(m_rInfo);
            else 
                retval = SyncWriteResourceRec(m_rInfo);
            if (!retval) {
                palmRec.SetIndex(m_rInfo.m_RecIndex);
                palmRec.SetID(m_rInfo.m_RecId);
            }
        }
        else
            return retval;
    }

    return retval;
}

long MozABHHManager::UpdateRecords(CPalmRecord **pRecordList, DWORD pCount)
{
    long retval=0;

    for (unsigned long i=0; i < pCount; i++)
    {
        CPalmRecord palmRec = *pRecordList[i];

        retval = AllocateRawRecord();
        if (retval) 
            return retval;

        retval = palmRec.ConvertToHH(m_rInfo);
        if (!retval) {
            m_rInfo.m_FileHandle = m_hhDB;
            if (m_bRecordDB)
                retval = SyncWriteRec(m_rInfo);
            else 
                retval = SyncWriteResourceRec(m_rInfo);
            if (!retval) {
                palmRec.SetIndex(m_rInfo.m_RecIndex);
                palmRec.SetID(m_rInfo.m_RecId);
            }
        }
        else
            return retval;
    }

    return retval;
}

long MozABHHManager::DeleteRecords(CPalmRecord **pRecordList, DWORD pCount)
{
    long retval=0;

    for (unsigned long i=0; i < pCount; i++)
    {
        CPalmRecord palmRec = *pRecordList[i];

        retval = AllocateRawRecord();
        if (retval) 
            return retval;

        retval = palmRec.ConvertToHH(m_rInfo);
        if (!retval) {
            m_rInfo.m_FileHandle = m_hhDB;
            if (m_bRecordDB)
                retval = SyncDeleteRec(m_rInfo);
            else 
                retval = SyncDeleteResourceRec(m_rInfo);
        }
        else
            return retval;
    }

    return retval;
}


long MozABHHManager::AddARecord(CPalmRecord & palmRec)
{
    long retval=0;

    palmRec.ResetAttribs();

    retval = AllocateRawRecord();
    if (retval) 
        return retval;

    retval = palmRec.ConvertToHH(m_rInfo);
    if (!retval) {
        m_rInfo.m_FileHandle = m_hhDB;
        m_rInfo.m_RecId = 0;
        if (m_bRecordDB)
            retval = SyncWriteRec(m_rInfo);
        else 
            retval = SyncWriteResourceRec(m_rInfo);
        if (!retval) {
            palmRec.SetIndex(m_rInfo.m_RecIndex);
            palmRec.SetID(m_rInfo.m_RecId);
        }
    }

    return retval;
}

long MozABHHManager::UpdateARecord(CPalmRecord & palmRec)
{
    long retval=0;

    retval = AllocateRawRecord();
    if (retval) 
        return retval;

    retval = palmRec.ConvertToHH(m_rInfo);
    if (!retval) {
        m_rInfo.m_FileHandle = m_hhDB;
        m_rInfo.m_Attribs = eRecAttrDirty;
        if (m_bRecordDB)
            retval = SyncWriteRec(m_rInfo);
        else 
            retval = SyncWriteResourceRec(m_rInfo);
        if (!retval) {
            palmRec.SetIndex(m_rInfo.m_RecIndex);
            palmRec.SetID(m_rInfo.m_RecId);
        }
    }

    return retval;
}

long MozABHHManager::DeleteARecord(CPalmRecord & palmRec)
{
    long retval=0;

    retval = AllocateRawRecord();
    if (retval) 
        return retval;

    retval = palmRec.ConvertToHH(m_rInfo);
    if (!retval) {
        m_rInfo.m_FileHandle = m_hhDB;
        if (m_bRecordDB)
            retval = SyncDeleteRec(m_rInfo);
        else 
            retval = SyncDeleteResourceRec(m_rInfo);
    }

    return retval;
}

