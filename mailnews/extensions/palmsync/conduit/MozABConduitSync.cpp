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
#include "syncmgr.h"
#include <tchar.h>

#include "MozABConduitSync.h"
#include "MozABConduitRecord.h"


#define MAX_PROD_ID_TEXT	255
#define PERSONAL "Personal"

CMozABConduitSync::CMozABConduitSync(CSyncProperties& rProps)
{
    m_dbHH = NULL;
    m_dbPC = NULL;
    m_remoteDB = NULL;
    m_TotRemoteDBs = rProps.m_nRemoteCount; // Fixed 3/25/99 KRM
    m_ConduitHandle =  (CONDHANDLE)0;
    memcpy(&m_rSyncProperties, &rProps, sizeof(m_rSyncProperties));
    memset(&m_SystemInfo, 0, sizeof(m_SystemInfo));
}

CMozABConduitSync::~CMozABConduitSync()
{
    long retval;

    if (m_SystemInfo.m_ProductIdText) {
        delete m_SystemInfo.m_ProductIdText;
	    m_SystemInfo.m_ProductIdText = NULL;
	    m_SystemInfo.m_AllocedLen = 0; 
    }
    if ((!m_rSyncProperties.m_RemoteDbList) && (m_remoteDB)) {
        delete m_remoteDB;
        m_remoteDB = NULL;
    }
    if (m_ConduitHandle) {
        retval = SyncUnRegisterConduit(m_ConduitHandle);
        m_ConduitHandle = 0;
    }
}

long CMozABConduitSync::Perform(void)
{
    long retval = 0;

    if (m_rSyncProperties.m_SyncType > eProfileInstall)
        return GEN_ERR_BAD_SYNC_TYPE;

    if (m_rSyncProperties.m_SyncType == eDoNothing) {
        return 0;
    }

    // Obtain System Information
    m_SystemInfo.m_ProductIdText = (BYTE*) new char [MAX_PROD_ID_TEXT]; //deleted in d'ctor
    if (!m_SystemInfo.m_ProductIdText)
        return GEN_ERR_LOW_MEMORY;

    m_SystemInfo.m_AllocedLen = (BYTE) MAX_PROD_ID_TEXT; 
    retval = SyncReadSystemInfo(m_SystemInfo);
    if (retval)
        return retval;

    // Register this conduit with SyncMgr.DLL for communication to HH
    retval = SyncRegisterConduit(m_ConduitHandle);
    if (retval)
        return retval;

    for (int iCount=0; iCount < m_TotRemoteDBs && !retval; iCount++) {
        retval = GetRemoteDBInfo(iCount);
        if (retval) {
            retval = 0;
            break;
        }

        m_dbHH = new MozABHHManager(GENERIC_FLAG_APPINFO_SUPPORTED | GENERIC_FLAG_CATEGORY_SUPPORTED,
                    m_remoteDB->m_Name,
                    m_remoteDB->m_Creator,
                    m_remoteDB->m_DbType,
                    m_remoteDB->m_DbFlags,
                    0,
					m_remoteDB->m_CardNum,
                    m_rSyncProperties.m_SyncType);
        if(!m_dbHH)
            return GEN_ERR_LOW_MEMORY;

        m_dbPC = new MozABPCManager();
        if(!m_dbPC) {
            if (m_dbHH)
                delete m_dbHH;
            m_dbHH = NULL;
            return GEN_ERR_LOW_MEMORY;
        }

        switch (m_rSyncProperties.m_SyncType) {
            case eFast:
                retval = PerformFastSync();
                if ((retval) && (retval == GEN_ERR_CHANGE_SYNC_MODE)){
                    if (m_rSyncProperties.m_SyncType == eHHtoPC)
                        retval = CopyHHtoPC();
                    else if (m_rSyncProperties.m_SyncType == ePCtoHH)
                        retval = CopyPCtoHH();
                }
                break;
            case eSlow:
                retval = PerformSlowSync();  
                if ((retval) && (retval == GEN_ERR_CHANGE_SYNC_MODE)){
                    if (m_rSyncProperties.m_SyncType == eHHtoPC)
                        retval = CopyHHtoPC();
                    else if (m_rSyncProperties.m_SyncType == ePCtoHH)
                        retval = CopyPCtoHH();
                }
                break;
            case eHHtoPC:
            case eBackup: 
                retval = CopyHHtoPC();
                break;
            case eInstall:
            case ePCtoHH:
            case eProfileInstall:
                retval = CopyPCtoHH();
                break;
            case eDoNothing:
                break;
            default:
                retval = GEN_ERR_SYNC_TYPE_NOT_SUPPORTED;
                break;
        }

        // remove the handheld and PC manager
        if (m_dbHH)
            delete m_dbHH;
        m_dbHH = NULL;
        if (m_dbPC)
            delete m_dbPC;
        m_dbPC = NULL;
    }

    // Unregister the conduit
    long retval2 = 0;
    if (m_ConduitHandle) {
        retval2 = SyncUnRegisterConduit(m_ConduitHandle);
        m_ConduitHandle = 0;
    }

    if (!retval)
        return retval2;

    return retval;
}

long CMozABConduitSync::GetRemoteDBInfo(int iIndex)
{
    long retval = 0;
    if (m_rSyncProperties.m_RemoteDbList) {
        m_remoteDB = m_rSyncProperties.m_RemoteDbList[iIndex];
    } else {
        if (m_remoteDB)
            delete m_remoteDB;

        m_remoteDB = new CDbList; // deleted in d'ctor
        if (m_remoteDB) {
            m_remoteDB->m_Creator  = m_rSyncProperties.m_Creator; 
            m_remoteDB->m_DbFlags  = eRecord; // todo
            m_remoteDB->m_DbType   = m_rSyncProperties.m_DbType; 
            strcpy(m_remoteDB->m_Name, m_rSyncProperties.m_RemoteName[iIndex]);
            m_remoteDB->m_Version   = 0;
            m_remoteDB->m_CardNum   = 0;
            m_remoteDB->m_ModNumber = 0;
            m_remoteDB->m_CreateDate= 0;
            m_remoteDB->m_ModDate   = 0;
            m_remoteDB->m_BackupDate= 0;
        } else {
            retval = GEN_ERR_LOW_MEMORY;
        }
    }
    return retval;
}

// this is done in separate thread since the main thread in this DLL
// needs to call SyncYieldCycles(1) atleast once every 7 seconds
DWORD WINAPI DoFastSync(LPVOID lpParameter)
{
    long retval=0;
    BOOL success = FALSE;

    CMozABConduitSync * sync = (CMozABConduitSync * ) lpParameter;
    if(!sync)
        return retval;

    if(sync->m_dbHH) {
        DWORD mozABCount=0;
        DWORD * mozCatIDList = NULL; // freed by MSCOM/Mozilla
        CPString ** mozABNameList = NULL; // freed by MSCOM/Mozilla
        BOOL * mozIsFirstSyncList = NULL; // freed by MSCOM/Mozilla

        retval = sync->m_dbHH->OpenDB(FALSE);
        if (!retval)
            retval = sync->m_dbHH->LoadCategories();

        if(!retval)
            retval = sync->m_dbPC->GetPCABList(&mozABCount, &mozCatIDList, &mozABNameList, &mozIsFirstSyncList);

        // for each category synchronize
        if (!retval) {
            CPCategory * pCategory = sync->m_dbHH->GetFirstCategory();
            while (pCategory) {
                CPalmRecord ** recordListHH = NULL;
                DWORD recordCountHH=0;

                DWORD catID = pCategory->GetID();
                DWORD catIndex = pCategory->GetIndex();
                CPString catName(pCategory->GetName());
                
                DWORD mozABIndex=0;
                BOOL foundInABList=FALSE;
                for(mozABIndex=0; mozABIndex<mozABCount; mozABIndex++) {
                    // if this category has been synchronized before
                    if(catID == mozCatIDList[mozABIndex]) {
                        retval = sync->m_dbHH->LoadUpdatedRecordsInCategory(catIndex, &recordListHH, &recordCountHH);
                        foundInABList = TRUE;
                        break;
                    }
                    // if corresponding category exists but not synchronized before
                    if(!catName.CompareNoCase(mozABNameList[mozABIndex]->GetBuffer(0))) {
                        retval = sync->m_dbHH->LoadAllRecordsInCategory(catIndex, &recordListHH, &recordCountHH);
                        foundInABList = TRUE;
                        break;
                    }
                    // if it is PAB, called 'Personal' on Palm and 'Personal Address Book' in Mozilla/Netscape
                    CPString personal(PERSONAL);
                    if( (catName.Find(personal) != -1) &&
                        (mozABNameList[mozABIndex]->Find(personal) != -1) ) {
                        retval = sync->m_dbHH->LoadAllRecordsInCategory(catIndex, &recordListHH, &recordCountHH);
                        foundInABList = TRUE;
                        break;
                    }
                } // end of for

                if(!retval && foundInABList) {
                    CPalmRecord ** recordListPC=NULL;
                    DWORD recordCountPC=0;
                    DWORD newRecAddedCount = 0;
                    DWORD * newRecIDList=NULL;
                    retval = sync->m_dbPC->SynchronizePCAB(catID, catName,
						                              recordCountHH, recordListHH,
						                              &recordCountPC, &recordListPC);
                    if (!retval) {
                        newRecIDList = (DWORD *) calloc(recordCountPC, sizeof(DWORD));
                        for (unsigned long i=0; i < recordCountPC; i++) {
                            if(!recordListPC[i])
                                continue;
                            CPalmRecord palmRec = *recordListPC[i];
                            palmRec.SetCategory(catIndex);
                            if(palmRec.GetAttribs() == ATTR_DELETED)
							    retval = sync->m_dbHH->DeleteARecord(palmRec);
                            else if(palmRec.GetAttribs() == ATTR_MODIFIED)
							    retval = sync->m_dbHH->UpdateARecord(palmRec);
                            else if(palmRec.GetAttribs() == ATTR_NEW) {
 							    retval = sync->m_dbHH->AddARecord(palmRec); // should we check existing recs?
                                if(!retval && (newRecAddedCount < recordCountPC)) {
                                    newRecIDList[newRecAddedCount] = palmRec.GetID();
                                    newRecAddedCount++;
                                }
                            }
                            delete recordListPC[i]; // once done with PC record, delete
					    }
                    }
                    // the MozAB is now synchronized
                    if(!retval)
                        mozIsFirstSyncList[mozABIndex] = FALSE;
                    // delete the PC recordList now that palm is updated
                    if(recordListPC)
                        free(recordListPC);  
                    // notify Mozilla that sync is done so that memory can be freed 
                    if(!retval)
                        success = TRUE;

                    if(newRecAddedCount) {
                        if(newRecAddedCount != recordCountPC)
                            newRecIDList = (DWORD *) realloc(newRecIDList, newRecAddedCount);
                        retval = sync->m_dbPC->NotifySyncDone(success, catID, newRecAddedCount, newRecIDList);
                    }
                    else 
                        retval = sync->m_dbPC->NotifySyncDone(success);

                    if(newRecIDList)
                        free(newRecIDList);
                }
                
                if(!retval && !foundInABList) {
                    retval = sync->m_dbHH->LoadAllRecordsInCategory(catIndex, &recordListHH, &recordCountHH);
                    if(!retval && recordCountHH)
                        retval = sync->m_dbPC->AddRecords(catID, catName, recordCountHH, recordListHH);
                }
                // delete and free HH records and recordList once synced
                if(recordListHH) {
                    CPalmRecord ** tempRecordListHH = recordListHH;
                    for(DWORD i=0; i < recordCountHH; i++) {
                        if(*tempRecordListHH)
                            delete *tempRecordListHH;
                        tempRecordListHH++;
                    }
                    free(recordListHH);
                }

                pCategory = sync->m_dbHH->GetNextCategory();
            } // end of while

            // deal with any Moz AB not existing in Palm, ones not sync'ed above
            for(DWORD mozABIndex=0; mozABIndex<mozABCount; mozABIndex++) {
                if(mozIsFirstSyncList[mozABIndex]) {
                    CPalmRecord ** recordListPC=NULL;
                    DWORD recordCountPC=0;
                    DWORD * newRecIDList=NULL;
                    CPCategory cat;
                    retval = sync->m_dbPC->LoadAllRecords(*mozABNameList[mozABIndex],
						                              &recordCountPC, &recordListPC);
                    if(!retval && recordCountPC) {
                        cat.SetName(mozABNameList[mozABIndex]->GetBuffer(0));
                        retval = sync->m_dbHH->AddCategory(cat);
                    }

                    if(!retval) {
                        newRecIDList = (DWORD *) calloc(recordCountPC, sizeof(DWORD));
                        for (unsigned long i=0; i < recordCountPC; i++) {
                            if(!recordListPC[i])
                                continue;
                            CPalmRecord palmRec = *recordListPC[i];
                            palmRec.SetCategory(cat.GetIndex());
 						    retval = sync->m_dbHH->AddARecord(palmRec); // should we check existing recs?
                            newRecIDList[i] = palmRec.GetID();
                            delete recordListPC[i]; // delete the record now that it is used
                        }
                    }
                    // the MozAB is now synchronized
                    if(!retval)
                        mozIsFirstSyncList[mozABIndex] = FALSE;
                    // delete the recordList now that palm is updated
                    if(recordListPC)
                        free(recordListPC);  
                    // notify Mozilla that sync is done so that memory can be freed 
                    if(!retval)
                        success = TRUE;
                    else
                        recordCountPC=0;
                    retval = sync->m_dbPC->NotifySyncDone(success, cat.GetID(), recordCountPC, newRecIDList);
                    if(newRecIDList)
                        free(newRecIDList);
                }
            } // end of mozAB for loop
        }

        // update category info in HH
        sync->m_dbHH->CompactCategoriesToHH();

        // close the HH DB once synced
        retval = sync->m_dbHH->CloseDB(FALSE);
    }

    return retval;
}

long CMozABConduitSync::PerformFastSync()
{
    DWORD ThreadId;
    HANDLE hThread = CreateThread(NULL, 0, DoFastSync, this, 0, &ThreadId);
    while (WaitForSingleObject(hThread, 1000) == WAIT_TIMEOUT) {
        SyncYieldCycles(1); // every sec call this for feedback & keep Palm connection alive 
        continue;
    }
    
    return 0;
}


long CMozABConduitSync::PerformSlowSync()
{
    // we take care of fist time sync in fast sync in an optimized way
    return PerformFastSync();
}

long CMozABConduitSync::CopyHHtoPC()
{
    return 0;
}

long CMozABConduitSync::CopyPCtoHH()
{
    return 0;
}

