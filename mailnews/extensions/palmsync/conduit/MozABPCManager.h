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

#ifndef _MOZAB_PCMANAGER_H_
#define _MOZAB_PCMANAGER_H_

#include <CPString.h>
#include <CPDbBMgr.h>

#include "IPalmSync.h"

class MozABPCManager
{
public:
    MozABPCManager() { }
    ~MozABPCManager() { }

	// this will return the list of ABs in Mozilla and if they were synced before
    long GetPCABList(DWORD * pCategoryCount, DWORD ** pCategoryIdList, 
                        CPString *** pCategoryNameList, CPString *** pCategoryURLList, BOOL ** pIsFirstTimeSyncList);
	// this will update a Mozilla AB with updated Palm records and 
	// return updated records in a Mozilla AB after the last sync
    // this will take care of first time sync also in which case 
    // updatedPalmRecList is the list of all palm records and 
    // updatedPCRecList is the list of unique Moz AB cards / records.
	long SynchronizePCAB(DWORD categoryId, CPString & categoryName,
						DWORD updatedPalmRecCount, CPalmRecord ** updatedPalmRecList,
						DWORD * pUpdatedPCRecList, CPalmRecord *** updatedPCRecList);
	// this will add all records in a Palm category into a new Mozilla AB 
	long AddRecords(DWORD categoryId, CPString & categoryName,
						DWORD updatedPalmRecCount, CPalmRecord ** updatedPalmRecList);
    // this load all records in an Moz AB
	long LoadAllRecords(CPString & ABName, DWORD * pPCRecListCount, CPalmRecord *** pPCRecList);

  long NotifySyncDone(BOOL success, DWORD catID=-1, DWORD newRecCount=0, DWORD * newRecIDList=NULL);

  // Update/Reset category id and mod time in an Moz AB
  long UpdatePCABSyncInfo(DWORD categoryId, CPString & categoryName);
  // Delete an Moz AB
  long DeletePCAB(DWORD categoryId, CPString & categoryName, CPString & categoryUrl);

private:
  	// this will initiate the communication with Mozilla
	BOOL InitMozPalmSyncInstance(IPalmSync **aRetValue);
									
};

#endif
