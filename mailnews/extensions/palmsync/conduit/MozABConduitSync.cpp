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
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "MozABConduitSync.h"
#include "MozABConduitRecord.h"


#define MAX_PROD_ID_TEXT	255
#define PERSONAL "Personal"

// Global var
FILE *gFD = NULL; // logging.

CMozABConduitSync::CMozABConduitSync(CSyncProperties& rProps)
{
    m_dbHH = NULL;
    m_dbPC = NULL;
    m_remoteDB = NULL;
    m_TotRemoteDBs = rProps.m_nRemoteCount; // Fixed 3/25/99 KRM
    m_ConduitHandle =  (CONDHANDLE)0;
    memcpy(&m_rSyncProperties, &rProps, sizeof(m_rSyncProperties));
    memset(&m_SystemInfo, 0, sizeof(m_SystemInfo));

    // See if logging is turned on (ie, check env variable "MOZ_CONDUIT_LOG"
    // for a logfile like "c:\\temp\\conduitlog.txt").
    char *envVar = getenv(ENV_VAR_CONDUTI_LOG);
    if(envVar != NULL )
      gFD = fopen(envVar, "a+");
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

    if (gFD)
      fclose(gFD);
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
            strncpy(m_remoteDB->m_Name, m_rSyncProperties.m_RemoteName[iIndex], sizeof(m_remoteDB->m_Name)-1);
            m_remoteDB->m_Name[sizeof(m_remoteDB->m_Name)-1] = '\0';
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

BOOL CMozABConduitSync::CategoryNameMatches(CPString &catName, CPString &cutOffMozABName, BOOL isPAB)
{
  if (!catName.CompareNoCase(cutOffMozABName.GetBuffer(0)))
    return TRUE;
  else 
    return (isPAB && !catName.CompareNoCase("Personal"));
}

BOOL CMozABConduitSync::CategoryExists(CPString &mozABName, BOOL isPAB)
{
    CPCategory * pCategory = m_dbHH->GetFirstCategory();
    // Palm only allows 15 chars for category names.
    CPString cutOffName;
    if (mozABName.Length() > 15)
        cutOffName = mozABName.Left(15);
    else
        cutOffName = mozABName;
    while (pCategory)
    {
        CPString catName(pCategory->GetName());
        if (CategoryNameMatches(catName, cutOffName, isPAB))
          return TRUE;
        // Process next category
        pCategory = m_dbHH->GetNextCategory();
    }
    return FALSE;
}

// this is done in separate thread since the main thread in this DLL
// needs to call SyncYieldCycles(1) atleast once every 7 seconds
DWORD WINAPI DoFastSync(LPVOID lpParameter)
{
    // Log the start time.
    time_t ltime;
    time( &ltime );
    CONDUIT_LOG1(gFD, "------------ START OF PALM SYNC ------------ at %s", ctime(&ltime));

    CMozABConduitSync * sync = (CMozABConduitSync * ) lpParameter;
    if(!sync || !sync->m_dbHH)
        return 0;
    
    BOOL initialHHSync = (sync->m_rSyncProperties.m_FirstDevice == eHH);
    if (initialHHSync) // if HH has been reset, copy everything on pc to hh.
      return sync->CopyPCtoHH();

    long retval=0;
    BOOL success = FALSE;

    DWORD mozABCount=0;
    LONG * mozCatIndexList = NULL; // freed by MSCOM/Mozilla
    CPString ** mozABNameList = NULL; // freed by MSCOM/Mozilla
    CPString ** mozABUrlList = NULL; // freed by MSCOM/Mozilla
    BOOL * mozDirFlagsList = NULL; // freed by MSCOM/Mozilla
    BOOL neverDidPalmSyncBefore; // 1st time palm sync?
    DWORD mozABIndex;

    retval = sync->m_dbHH->OpenDB(FALSE);
    if (!retval)
        retval = sync->m_dbHH->LoadCategories();

    CONDUIT_LOG0(gFD, "Getting moz AB List ... ");
    if(!retval)
        retval = sync->m_dbPC->GetPCABList(&mozABCount, &mozCatIndexList, &mozABNameList, &mozABUrlList, &mozDirFlagsList);

    if (retval)
      return retval;
    CONDUIT_LOG0(gFD, "Done getting moz AB List. \n");
    
    // Create an array to help us identify addrbooks that have been deleted on Palm.
    DWORD *mozABSeen = (DWORD *) CoTaskMemAlloc(sizeof(DWORD) * mozABCount);
    if (!mozABSeen)
      return GEN_ERR_LOW_MEMORY;
    else
      memset(mozABSeen, FALSE, sizeof(DWORD) * mozABCount);

    CONDUIT_LOG1(gFD, "first device = %lx\n", sync->m_rSyncProperties.m_FirstDevice);

    // See if palm sync was performed before.
    for(mozABIndex=0; mozABIndex<mozABCount; mozABIndex++)
      if (! (mozDirFlagsList[mozABIndex] & kFirstTimeSyncDirFlag))
      {
        neverDidPalmSyncBefore = FALSE;
        break;
      }
    
    // Log moz addrbooks.
    for (mozABIndex=0; mozABIndex<mozABCount; mozABIndex++)
    {
      CONDUIT_LOG5(gFD, "Moz AB[%d] category index/synced=%d/%d, name= '%s', url= '%s'\n",
                   mozABIndex, mozCatIndexList[mozABIndex], ! (mozDirFlagsList[mozABIndex] & kFirstTimeSyncDirFlag), 
                   mozABNameList[mozABIndex]->GetBuffer(0), mozABUrlList[mozABIndex]->GetBuffer(0));
    }

    // For each category, try to find the corresponding AB in the moz AB list
    // and see if it has been synchronized before and take action accordingly.
    CPCategory * pCategory = sync->m_dbHH->GetFirstCategory();
    while (pCategory)
    {
        CPalmRecord ** recordListHH = NULL;
        DWORD recordCountHH=0;

        DWORD catID = pCategory->GetID();
        DWORD catIndex = pCategory->GetIndex();
        DWORD catFlags = pCategory->GetFlags();
        CPString catName(pCategory->GetName());

        CONDUIT_LOG3(gFD, "\nProcessing Palm AB '%s' (catIndex/catId) = (%d/%d)... \n", catName.GetBuffer(0), catIndex, catID);
        BOOL abRenamed = FALSE;
        BOOL foundInABList=FALSE;
        for(mozABIndex=0; mozABIndex<mozABCount; mozABIndex++)
        {
            // Palm only allows 15 chars for category names.
            CPString cutOffName;
            if (mozABNameList[mozABIndex]->Length() > 15)
                cutOffName = mozABNameList[mozABIndex]->Left(15);
            else
                cutOffName = *mozABNameList[mozABIndex];

            // if this category has been synchronized before
            if(catIndex == mozCatIndexList[mozABIndex]) 
            {
                retval = sync->m_dbHH->LoadUpdatedRecordsInCategory(catIndex, &recordListHH, &recordCountHH);
                CONDUIT_LOG2(gFD, "Category index = %d, name = '%s' has been synced before\n", catID, catName.GetBuffer(0));
                foundInABList = TRUE;
                mozABSeen[mozABIndex] = TRUE; // mark it seen
                // See if the name has been changed on Palm side. Note that if both
                // Palm category and the corresponding moz addrbook are renamed then 
                // Palm category name takes precedence.
                if (!sync->CategoryNameMatches(catName, cutOffName, mozDirFlagsList[mozABIndex] & kIsPabDirFlag))
                {
                  abRenamed = TRUE;
                  CONDUIT_LOG3(gFD, "Category index = %d, name = '%s' was renamed (from '%s') on Palm so do the same on moz\n", 
                    catID, catName.GetBuffer(0), mozABNameList[mozABIndex]->GetBuffer(0));
                }
                break;
            }

            // if corresponding category exists but not synchronized before
            if(sync->CategoryNameMatches(catName, cutOffName, mozDirFlagsList[mozABIndex] & kIsPabDirFlag))
            {
                retval = sync->m_dbHH->LoadAllRecordsInCategory(catIndex, &recordListHH, &recordCountHH);
                CONDUIT_LOG2(gFD, "Category index = %d, name = '%s' has not been synced before\n", catID, catName.GetBuffer(0));
                foundInABList = TRUE;
                mozABSeen[mozABIndex] = TRUE; // mark it seen
                break;
            }
        } // end of for

        if(!retval && foundInABList)
        {
          CPalmRecord ** recordListPC=NULL;
          DWORD recordCountPC=0;
          DWORD newRecAddedCount = 0;
          DWORD * newRecIDList=NULL;
          CONDUIT_LOG1(gFD, "  Syncing with moz AB with %d modified Palm record(s) ... ", recordCountHH);
          retval = sync->m_dbPC->SynchronizePCAB(catIndex, catID, catName,
                                                 recordCountHH, recordListHH,
                                                 &recordCountPC, &recordListPC);
          CONDUIT_LOG1(gFD, "Done syncing AB. retval=%lx.\n", retval);

          // SynchronizePCAB() returns a list of modified moz records so update those on Palm.
          if (!retval) {
              unsigned long i;
              newRecIDList = (DWORD *) calloc(recordCountPC, sizeof(DWORD));
              if (recordCountPC > 0)
                CONDUIT_LOG1(gFD, "  Updating Palm AB with %d modified moz record(s) ...\n", recordCountPC);
              for (i=0; i < recordCountPC; i++) {
                  if(!recordListPC[i])
                      continue;
                  CPalmRecord palmRec = *recordListPC[i];
                  palmRec.SetCategory(catIndex);
                  if(palmRec.GetAttribs() == ATTR_DELETED)
                  {
                    CONDUIT_LOG0(gFD, "    - deleting a record\n");
							      retval = sync->m_dbHH->DeleteARecord(palmRec);
                  }
                  else if(palmRec.GetAttribs() == ATTR_MODIFIED)
                  {
                    CONDUIT_LOG0(gFD, "    - modifying a record\n");
							      retval = sync->m_dbHH->UpdateARecord(palmRec);
                  }
                  else if(palmRec.GetAttribs() == ATTR_NEW) {
                      CONDUIT_LOG0(gFD, "    - adding a record\n");
 							        retval = sync->m_dbHH->AddARecord(palmRec); // should we check existing recs?
                      if(!retval && (newRecAddedCount < recordCountPC)) {
                          newRecIDList[newRecAddedCount] = palmRec.GetID();
                          newRecAddedCount++;
                      }
                  }
                  delete recordListPC[i]; // once done with PC record, delete
					    }
              if (recordCountPC > 0)
                CONDUIT_LOG0(gFD, "  Done updating Palm AB.\n");
          }
          // the MozAB is now synchronized
          if(!retval)
              mozDirFlagsList[mozABIndex] &= ~kFirstTimeSyncDirFlag;
          // delete the PC recordList now that palm is updated
          if(recordListPC)
              free(recordListPC);  
          // notify Mozilla that sync is done so that memory can be freed 
          success = retval ? FALSE : TRUE;

          if(newRecAddedCount) {
              if(newRecAddedCount != recordCountPC)
                  newRecIDList = (DWORD *) realloc(newRecIDList, newRecAddedCount);
              retval = sync->m_dbPC->NotifySyncDone(success, catIndex, newRecAddedCount, newRecIDList);
          }
          else 
              retval = sync->m_dbPC->NotifySyncDone(success);

          if(newRecIDList)
              free(newRecIDList);

          // See if it was renamed on palm.
          if (abRenamed)
          {
            // We should not rename personal and collected address books here.
            if (! (mozDirFlagsList[mozABIndex] & kIsPabDirFlag) &&
              mozABUrlList[mozABIndex]->CompareNoCase(COLLECTED_ADDRBOOK_URL))
            {
              CONDUIT_LOG0(gFD, "  Renaming AB ... ");
              retval = sync->m_dbPC->RenamePCAB(catIndex, catName, *mozABUrlList[mozABIndex]);
              CONDUIT_LOG1(gFD, "Done renaming AB. retval=%d.\n", retval);
            }
          }
        }
        
        // If this category can't be found in the moz AB list then two cases here:
        //    1. If catFlags is not CAT_DIRTY then this category has been deleted
        //       on moz so remove it from palm side.
        //    2. else, if we never did palm sync on moz before then it's a new one
        //       on palm. So create a new AB in moz with all records in this category
        //       (even if it's an empty AB).
        if(!retval && !foundInABList) 
        {
            if (catFlags != CAT_DIRTY && !neverDidPalmSyncBefore && !initialHHSync)
            {
              BOOL abDeleted = sync->m_dbPC->PCABDeleted(catName);
              if (abDeleted)
              {
                CONDUIT_LOG2(gFD, "Category index = %d, name = '%s' is removed from moz and needs to be removed from palm\n", catID, catName.GetBuffer(0));
                CONDUIT_LOG0(gFD, "  Deleting AB from Palm ... ");
                retval = sync->m_dbHH->DeleteCategory(catIndex, FALSE);
                CONDUIT_LOG1(gFD, "Done deleting AB from Palm. retval=%d.\n", retval);
              }
            }
            else
            {
              CONDUIT_LOG2(gFD, "Category index = %d, name = '%s' is new on palm and needs to be added to moz\n", catID, catName.GetBuffer(0));
              retval = sync->m_dbHH->LoadAllRecordsInCategory(catIndex, &recordListHH, &recordCountHH);
              CONDUIT_LOG1(gFD, "  Creating new moz AB with %d Palm record(s) ... ", recordCountHH);
              if(!retval)
                retval = sync->m_dbPC->AddRecords(FALSE, catIndex, catName, recordCountHH, recordListHH);
              CONDUIT_LOG1(gFD, "Done creating new moz AB. retval=%d.\n", retval);
            }
        }

        // delete and free HH records and recordList once synced
        if(recordListHH) {
            CPalmRecord ** tempRecordListHH = recordListHH;
            for(DWORD i=0; i < recordCountHH; i++) 
            {
                if(*tempRecordListHH)
                    delete *tempRecordListHH;
                tempRecordListHH++;
            }
            free(recordListHH);
        }

        // Process next category
        pCategory = sync->m_dbHH->GetNextCategory();
    } // end of while

    // Deal with any Moz AB not existing in Palm, ones not sync'ed above,
    // and the case where Palm ABs have been deleted.
    for(mozABIndex=0; mozABIndex<mozABCount; mozABIndex++)
    {
        if((mozDirFlagsList[mozABIndex] & kFirstTimeSyncDirFlag || neverDidPalmSyncBefore || initialHHSync) && !sync->CategoryExists(*mozABNameList[mozABIndex],  mozDirFlagsList[mozABIndex] & kIsPabDirFlag))
        {
            CONDUIT_LOG3(gFD, "\nMoz AB[%d] category index = %d, name = '%s' doesn't exist on Palm so needs to be added to palm\n",
                    mozABIndex, mozCatIndexList[mozABIndex], mozABNameList[mozABIndex]->GetBuffer(0));
            CPalmRecord ** recordListPC=NULL;
            DWORD recordCountPC=0;
            DWORD * newRecIDList=NULL;
            CPCategory cat;
            retval = sync->m_dbPC->LoadAllRecords(*mozABNameList[mozABIndex],
						                      &recordCountPC, &recordListPC);
            if(!retval) 
            {
                if (! (mozDirFlagsList[mozABIndex] & kIsPabDirFlag))
                  cat.SetName(mozABNameList[mozABIndex]->GetBuffer(0));
                else
                  cat.SetName("Personal");

                CONDUIT_LOG1(gFD, "  Creating new Palm AB with %d record(s) ... ", recordCountPC);
                retval = sync->m_dbHH->AddCategory(cat);
                CONDUIT_LOG2(gFD, "Done creating new Palm AB, new category index=%d. retval=%d.\n", cat.GetIndex(), retval);
                if(!retval) 
                {
                    CONDUIT_LOG1(gFD, "  Adding %d record(s) to new Palm AB ... ", recordCountPC);
                    newRecIDList = (DWORD *) calloc(recordCountPC, sizeof(DWORD));
                    for (unsigned long i=0; i < recordCountPC; i++) 
                    {
                        if(!recordListPC[i])
                            continue;
                        CPalmRecord palmRec = *recordListPC[i];
                        palmRec.SetCategory(cat.GetIndex());
                        retval = sync->m_dbHH->AddARecord(palmRec); // should we check existing recs?
                        newRecIDList[i] = palmRec.GetID();
                        delete recordListPC[i]; // delete the record now that it is used
                    }
                    CONDUIT_LOG0(gFD, "Done adding new records to new Palm AB.\n");
                }
                else
                  CONDUIT_LOG1(gFD, "Creating new Palm AB failed. retval=%d.\n", retval);
            }
            else
              CONDUIT_LOG1(gFD, "  Loading moz AB records failed so can't create new Palm AB. retval=%d.\n", retval);


            // the MozAB is now synchronized
            if(!retval)
              mozDirFlagsList[mozABIndex] &= ~kFirstTimeSyncDirFlag;
            // delete the recordList now that palm is updated
            if(recordListPC)
                free(recordListPC);  
            // notify Mozilla that sync is done so that memory can be freed 
            if(!retval)
                success = TRUE;
            else
            {
                success = FALSE;
                recordCountPC=0;
            }
            retval = sync->m_dbPC->NotifySyncDone(success, cat.GetIndex(), recordCountPC, newRecIDList);
            if(newRecIDList)
                free(newRecIDList);

            // Lastly, update the AB with new category index and mod time.
            retval = sync->m_dbPC->UpdatePCABSyncInfo(success ? cat.GetIndex() : -1, *mozABNameList[mozABIndex]);
        }
        else if (!mozABSeen[mozABIndex] && !neverDidPalmSyncBefore && !initialHHSync)
        {
          // We should not delete personal and collected address books here. Rather,
          // reset the mod time so next time we can sync them back to Palm again.
          // Note: make sure the moz addrbook has been synced before to avoid the case
          // where early sync failed and we delete this un-synced addrbook by mistake.
          if (mozCatIndexList[mozABIndex] > -1 &&
              ! (mozDirFlagsList[mozABIndex] & kIsPabDirFlag) &&
              mozABUrlList[mozABIndex]->CompareNoCase(COLLECTED_ADDRBOOK_URL))
          {
            CONDUIT_LOG3(gFD, "\nMoz AB[%d] category index = %d, name = '%s' is removed on Palm and needs to be removed from moz\n", 
                    mozABIndex, mozCatIndexList[mozABIndex], mozABNameList[mozABIndex]->GetBuffer(0));
            CONDUIT_LOG0(gFD, "  Deleting AB from moz ... ");
            retval = sync->m_dbPC->DeletePCAB(mozCatIndexList[mozABIndex], *mozABNameList[mozABIndex], *mozABUrlList[mozABIndex]);
            CONDUIT_LOG1(gFD, "Done deleting AB from moz. retval=%d.\n", retval);
          }
          else
          {
            CONDUIT_LOG3(gFD, "\nMoz AB[%d] category index = %d, name = '%s', is removed on Palm but only need to reset sync info on moz\n", 
                    mozABIndex, mozCatIndexList[mozABIndex], mozABNameList[mozABIndex]->GetBuffer(0));
            // Reset category index and mod time.
            CONDUIT_LOG0(gFD, "  Resetting AB info on moz ... ");
            retval = sync->m_dbPC->UpdatePCABSyncInfo(-1, *mozABNameList[mozABIndex]);
            CONDUIT_LOG1(gFD, "  Done resetting AB info on moz. retval=%d.\n", retval);
          }
        }
    } // end of mozAB not existing in Palm for loop

    // Purge deleted Palm record permanently (in case they were logically deleted).
    sync->m_dbHH->PurgeDeletedRecs();

    // Free stuff we allocated.
    CoTaskMemFree(mozABSeen);
    
    // update category info in HH
    sync->m_dbHH->CompactCategoriesToHH();

    // close the HH DB once synced
    retval = sync->m_dbHH->CloseDB(FALSE);

    // Log the end time.
    time( &ltime );
    CONDUIT_LOG1(gFD, "------------ END OF PALM SYNC ------------ at %s\n", ctime(&ltime));
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
    // we take care of first time sync in fast sync in an optimized way
    return PerformFastSync();
}

long CMozABConduitSync::CopyHHtoPC()
{
    long retval=0;
    BOOL success = FALSE;
    // Log the start time.
    time_t ltime;
    time( &ltime );
    CONDUIT_LOG1(gFD, "------------ yyy START OF PALM SYNC Palm -> Destkop ------------ at %s", ctime(&ltime));

    if(!m_dbHH)
        return retval;
    
    DWORD mozABCount=0;
    LONG * mozCatIndexList = NULL; // freed by MSCOM/Mozilla
    CPString ** mozABNameList = NULL; // freed by MSCOM/Mozilla
    CPString ** mozABUrlList = NULL; // freed by MSCOM/Mozilla
    BOOL * mozDirFlagsList = NULL; // freed by MSCOM/Mozilla
    BOOL neverDidPalmSyncBefore = TRUE; // 1st time palm sync?
    DWORD mozABIndex;

    retval = m_dbHH->OpenDB(FALSE);
    if (!retval)
        retval = m_dbHH->LoadCategories();

    CONDUIT_LOG0(gFD, "Getting moz AB List ... ");
    if(!retval)
        retval = m_dbPC->GetPCABList(&mozABCount, &mozCatIndexList, &mozABNameList, &mozABUrlList, &mozDirFlagsList);

    CONDUIT_LOG1(gFD, "Done getting moz AB List. retval = %d\n", retval);
    if (retval)
      return retval;
    
    // Create an array to help us identify addrbooks that have been deleted on Palm.
    DWORD *mozABSeen = (DWORD *) CoTaskMemAlloc(sizeof(DWORD) * mozABCount);
    if (!mozABSeen)
      return GEN_ERR_LOW_MEMORY;
    else
      memset(mozABSeen, FALSE, sizeof(DWORD) * mozABCount);

    // See if palm sync was performed before.
    for(mozABIndex=0; mozABIndex<mozABCount; mozABIndex++)
      if (! (mozDirFlagsList[mozABIndex] & kFirstTimeSyncDirFlag))
      {
        neverDidPalmSyncBefore = FALSE;
        break;
      }
    
    // Log moz addrbooks.
    for (mozABIndex=0; mozABIndex<mozABCount; mozABIndex++)
    {
      CONDUIT_LOG5(gFD, "Moz AB[%d] category index/synced=%d/%d, name= '%s', url= '%s'\n",
                   mozABIndex, mozCatIndexList[mozABIndex], !(mozDirFlagsList[mozABIndex] & kFirstTimeSyncDirFlag), 
                   mozABNameList[mozABIndex]->GetBuffer(0), mozABUrlList[mozABIndex]->GetBuffer(0));
    }

    // For each category, try to find the corresponding AB in the moz AB list
    // and overwrite it with the HH category.
    CPCategory * pCategory = m_dbHH->GetFirstCategory();
    while (pCategory)
    {
        CPalmRecord ** recordListHH = NULL;
        DWORD recordCountHH=0;

        DWORD catID = pCategory->GetID();
        DWORD catIndex = pCategory->GetIndex();
        DWORD catFlags = pCategory->GetFlags();
        CPString catName(pCategory->GetName());

        CONDUIT_LOG3(gFD, "\nProcessing Palm AB '%s' (catIndex/catId) = (%d/%d)... \n", catName.GetBuffer(0), catIndex, catID);
        BOOL abRenamed = FALSE;
        BOOL foundInABList=FALSE;
        for(mozABIndex = 0; mozABIndex < mozABCount; mozABIndex++)
        {
            // Palm only allows 15 chars for category names.
            CPString cutOffName;
            if (mozABNameList[mozABIndex]->Length() > 15)
                cutOffName = mozABNameList[mozABIndex]->Left(15);
            else
                cutOffName = *mozABNameList[mozABIndex];

            // if this category has been synchronized before, check if it has been renamed
            if(catIndex == mozCatIndexList[mozABIndex]) 
            {
                  CONDUIT_LOG4(gFD, "Category index = %d, name = '%s' moz ab name = %s has been synced before uri = %s\n", 
                    catID, catName.GetBuffer(0), mozABNameList[mozABIndex]->GetBuffer(0), mozABUrlList[mozABIndex]->GetBuffer(0));
                foundInABList = TRUE;
                mozABSeen[mozABIndex] = TRUE; // mark it seen
                // See if the name has been changed on Palm side. Note that if both
                // Palm category and the corresponding moz addrbook are renamed then 
                // Palm category name takes precedence.
                if (!CategoryNameMatches(catName, cutOffName, mozDirFlagsList[mozABIndex] & kIsPabDirFlag))
                {
                  abRenamed = TRUE;
                  CONDUIT_LOG3(gFD, "Category index = %d, name = '%s' was renamed (from '%s') on Palm so do the same on moz\n", 
                    catID, catName.GetBuffer(0), mozABNameList[mozABIndex]->GetBuffer(0));
                }
                break;
            }
            // if corresponding category exists 
            if (CategoryNameMatches(catName, cutOffName, mozDirFlagsList[mozABIndex] & kIsPabDirFlag))
            {
                retval = m_dbHH->LoadAllRecordsInCategory(catIndex, &recordListHH, &recordCountHH);
                CONDUIT_LOG3(gFD, "Category index = %d, name = '%s' matches moz ab %s\n", catID, catName.GetBuffer(0), mozABUrlList[mozABIndex]->GetBuffer(0));
                foundInABList = TRUE;
                mozABSeen[mozABIndex] = TRUE; // mark it seen
                break;
            }
            else
                CONDUIT_LOG3(gFD, "Category index = %d, name = '%s' doesn't match moz ab %s\n", catID, catName.GetBuffer(0), mozABUrlList[mozABIndex]->GetBuffer(0));

        } // end of for

        // we've got a matching moz AB. So, just delete all the cards in the Moz AB and copy over all the cards
        // in the HH category.
        if(!retval && foundInABList)
        {
          CPalmRecord ** recordListPC=NULL;
          DWORD * newRecIDList=NULL;
//          retval = m_dbPC->DeletePCAB(mozCatIndexList[mozABIndex], *mozABNameList[mozABIndex], *mozABUrlList[mozABIndex]);
          retval = m_dbHH->LoadAllRecordsInCategory(catIndex, &recordListHH, &recordCountHH);
          CONDUIT_LOG2(gFD, "  Creating new moz AB %s with %d Palm record(s) ... ", mozABNameList[mozABIndex]->GetBuffer(0), recordCountHH);
          if(!retval)
            retval = m_dbPC->AddRecords(TRUE, catIndex, *mozABNameList[mozABIndex], recordCountHH, recordListHH);

          CONDUIT_LOG1(gFD, "Done creating new moz AB. retval=%d.\n", retval);
          // the MozAB is now synchronized
          if(!retval)
              mozDirFlagsList[mozABIndex] &= ~kFirstTimeSyncDirFlag;
          // delete the PC recordList now that palm is updated
          if(recordListPC)
              free(recordListPC);  
          // notify Mozilla that sync is done so that memory can be freed 
          success = retval ? FALSE : TRUE;

          retval = m_dbPC->NotifySyncDone(success);

          if(newRecIDList)
              free(newRecIDList);

          // See if it was renamed on palm.
          if (abRenamed)
          {
            // We should not rename personal and collected address books here.
            if (! (mozDirFlagsList[mozABIndex] & kIsPabDirFlag) &&
              mozABUrlList[mozABIndex]->CompareNoCase(COLLECTED_ADDRBOOK_URL))
            {
              CONDUIT_LOG0(gFD, "  Renaming AB ... ");
              retval = m_dbPC->RenamePCAB(catIndex, catName, *mozABUrlList[mozABIndex]);
              CONDUIT_LOG1(gFD, "Done renaming AB. retval=%d.\n", retval);
            }
          }
        }
        
        // If this category can't be found in the moz AB list, then
        // create a new AB in moz with all records in this category
        //  (even if it's an empty AB).
        if(!retval && !foundInABList) 
        {
            CONDUIT_LOG2(gFD, "Category index = %d, name = '%s' is new on palm and needs to be added to moz\n", catID, catName.GetBuffer(0));
            retval = m_dbHH->LoadAllRecordsInCategory(catIndex, &recordListHH, &recordCountHH);
            CONDUIT_LOG2(gFD, "  Creating new moz AB %s with %d Palm record(s) ... ", catName.GetBuffer(0), recordCountHH);
            if(!retval)
            {
              CPString mozABName;

              retval = m_dbPC->AddRecords(FALSE, catIndex, catName, recordCountHH, recordListHH);
            }
            CONDUIT_LOG2(gFD, "Done creating new moz AB %s - retval=%d.\n", catName.GetBuffer(0), retval);
        }

        // delete and free HH records and recordList once synced
        if(recordListHH) 
        {
            CPalmRecord ** tempRecordListHH = recordListHH;
            for(DWORD i=0; i < recordCountHH; i++) 
            {
                delete *tempRecordListHH;
                tempRecordListHH++;
            }
            free(recordListHH);
        }

        // Process next category
        pCategory = m_dbHH->GetNextCategory();
    } // end of while

    // Free stuff we allocated.
    CoTaskMemFree(mozABSeen);
    
    // update category info in HH
    m_dbHH->CompactCategoriesToHH();

    // close the HH DB once synced
    retval = m_dbHH->CloseDB(FALSE);

    // Log the end time.
    time( &ltime );
    CONDUIT_LOG1(gFD, "------------ END OF PALM SYNC ------------ at %s\n", ctime(&ltime));
    return retval;
}

long CMozABConduitSync::CopyPCtoHH()
{
    long retval=0;
    BOOL success = FALSE;

    // Log the start time.
    time_t ltime;
    time( &ltime );
    CONDUIT_LOG1(gFD, "------------ START OF PALM SYNC Destkop-> Palm   ------------ at %s", ctime(&ltime));

    if(!m_dbHH)
        return retval;
    
    DWORD mozABCount=0;
    LONG * mozCatIndexList = NULL; // freed by MSCOM/Mozilla
    CPString ** mozABNameList = NULL; // freed by MSCOM/Mozilla
    CPString ** mozABUrlList = NULL; // freed by MSCOM/Mozilla
    BOOL * mozDirFlagsList = NULL; // freed by MSCOM/Mozilla
    BOOL neverDidPalmSyncBefore = TRUE; // 1st time palm sync?
    DWORD mozABIndex;

    retval = m_dbHH->OpenDB(FALSE);
    if (!retval)
        retval = m_dbHH->LoadCategories();

    CONDUIT_LOG0(gFD, "Getting moz AB List ... ");
    if(!retval)
        retval = m_dbPC->GetPCABList(&mozABCount, &mozCatIndexList, &mozABNameList, &mozABUrlList, &mozDirFlagsList);

    if (retval)
      return retval;
    CONDUIT_LOG0(gFD, "Done getting moz AB List. \n");
    
    // Create an array to help us identify addrbooks that have been deleted on Palm.
    DWORD *mozABSeen = (DWORD *) CoTaskMemAlloc(sizeof(DWORD) * mozABCount);
    if (!mozABSeen)
      return GEN_ERR_LOW_MEMORY;
    else
      memset(mozABSeen, FALSE, sizeof(DWORD) * mozABCount);

    // See if palm sync was performed before.
    for(mozABIndex=0; mozABIndex<mozABCount; mozABIndex++)
      if (! (mozDirFlagsList[mozABIndex] & kFirstTimeSyncDirFlag))
      {
        neverDidPalmSyncBefore = FALSE;
        break;
      }
    
    // Log moz addrbooks.
    for (mozABIndex=0; mozABIndex<mozABCount; mozABIndex++)
    {
      CONDUIT_LOG5(gFD, "Moz AB[%d] category index/synced=%d/%d, name= '%s', url= '%s'\n",
                   mozABIndex, mozCatIndexList[mozABIndex], !(mozDirFlagsList[mozABIndex] & kFirstTimeSyncDirFlag), 
                   mozABNameList[mozABIndex]->GetBuffer(0), mozABUrlList[mozABIndex]->GetBuffer(0));
    }

    // For each category, try to find the corresponding AB in the moz AB list
    // and see if it has been synchronized before and take action accordingly.
    CPCategory * pCategory = m_dbHH->GetFirstCategory();
    while (pCategory)
    {
        CPalmRecord ** recordListHH = NULL;
        DWORD recordCountHH=0;

        DWORD catID = pCategory->GetID();
        DWORD catIndex = pCategory->GetIndex();
        DWORD catFlags = pCategory->GetFlags();
        CPString catName(pCategory->GetName());

        CONDUIT_LOG3(gFD, "\nProcessing Palm AB '%s' (catIndex/catId) = (%d/%d)... \n", catName.GetBuffer(0), catIndex, catID);
        BOOL abRenamed = FALSE;
        BOOL foundInABList=FALSE;

        retval = m_dbHH->DeleteCategory(catIndex, FALSE); // delete the category - we'll recreate from moz ab.

        // Process next category
        pCategory = m_dbHH->GetNextCategory();
    } // end of while

    // Deal with any Moz AB not existing in Palm, ones not sync'ed above,
    // and the case where Palm ABs have been deleted.
    for(mozABIndex=0; mozABIndex<mozABCount; mozABIndex++)
    {
        CONDUIT_LOG3(gFD, "\nMoz AB[%d] category index = %d, name = '%s' doesn't exist on Palm so needs to be added to palm\n",
                mozABIndex, mozCatIndexList[mozABIndex], mozABNameList[mozABIndex]->GetBuffer(0));
        CPalmRecord ** recordListPC=NULL;
        DWORD recordCountPC=0;
        DWORD * newRecIDList=NULL;
        CPCategory cat;
        retval = m_dbPC->LoadAllRecords(*mozABNameList[mozABIndex],
						                  &recordCountPC, &recordListPC);
        if(!retval) 
        {
            if ((mozDirFlagsList[mozABIndex] & kIsPabDirFlag))
              cat.SetName("Personal");
            else
              cat.SetName(mozABNameList[mozABIndex]->GetBuffer(0));
            CONDUIT_LOG1(gFD, "  Creating new Palm AB with %d record(s) ... ", recordCountPC);
            retval = m_dbHH->AddCategory(cat);
            CONDUIT_LOG2(gFD, "Done creating new Palm AB, new category index=%d. retval=%d.\n", cat.GetIndex(), retval);
            if(!retval) {
                CONDUIT_LOG1(gFD, "  Adding %d record(s) to new Palm AB ... ", recordCountPC);
                newRecIDList = (DWORD *) calloc(recordCountPC, sizeof(DWORD));
                for (unsigned long i=0; i < recordCountPC; i++) 
                {
                    if(!recordListPC[i])
                        continue;
                    CPalmRecord palmRec = *recordListPC[i];
                    palmRec.SetCategory(cat.GetIndex());
                    retval = m_dbHH->AddARecord(palmRec); // should we check existing recs?
                    newRecIDList[i] = palmRec.GetID();
                    delete recordListPC[i]; // delete the record now that it is used
                }
                CONDUIT_LOG0(gFD, "Done adding new records to new Palm AB.\n");
            }
            else
              CONDUIT_LOG1(gFD, "Creating new Palm AB failed. retval=%d.\n", retval);
        }
        else
          CONDUIT_LOG1(gFD, "  Loading moz AB records failed so can't create new Palm AB. retval=%d.\n", retval);


        // delete the recordList now that palm is updated
        if(recordListPC)
            free(recordListPC);  
        // notify Mozilla that sync is done so that memory can be freed 
        if(!retval)
            success = TRUE;
        else
        {
            success = FALSE;
            recordCountPC=0;
        }
        retval = m_dbPC->NotifySyncDone(success, cat.GetIndex(), recordCountPC, newRecIDList);
        if(newRecIDList)
            free(newRecIDList);

        // Lastly, update the AB with new category index and mod time.
        retval = m_dbPC->UpdatePCABSyncInfo(success ? cat.GetIndex() : -1, *mozABNameList[mozABIndex]);
    } // end of mozAB not existing in Palm for loop

    // Purge deleted Palm record permanently (in case they were logically deleted).
    m_dbHH->PurgeDeletedRecs();

    // Free stuff we allocated.
    CoTaskMemFree(mozABSeen);
    
    // update category info in HH
    m_dbHH->CompactCategoriesToHH();

    // close the HH DB once synced
    retval = m_dbHH->CloseDB(FALSE);

    // Log the end time.
    time( &ltime );
    CONDUIT_LOG1(gFD, "------------ END OF PALM SYNC ------------ at %s\n", ctime(&ltime));
    return retval;
}

