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

#ifndef _MOZAB_HHMANAGER_H_
#define _MOZAB_HHMANAGERT_H_

#include <CPDbBMgr.h>

class MozABHHManager
{
public:
    MozABHHManager(DWORD dwGenericFlags, 
               char *pName, 
               DWORD dwCreator, 
               DWORD dwType, 
               WORD dbFlags, 
               WORD dbVersion, 
			   int  iCardNum,
               eSyncTypes syncType);
    ~MozABHHManager();

    long OpenDB(BOOL bCreate);
    long CloseDB(BOOL bDontUpdate);

    // this will load categories in m_pCatMgr member
    long LoadCategories();
    CPCategory * GetFirstCategory() { return m_pCatMgr->FindFirst(); }
    CPCategory * GetNextCategory() { return m_pCatMgr->FindNext(); }
    long DeleteCategory(DWORD dwCategory, BOOL bMoveToUnfiled);
    long ChangeCategory(DWORD dwOldCatIndex, DWORD dwNewCatIndex);
    long AddCategory(CPCategory & cat);
    long CompactCategoriesToHH();

    long LoadAllRecords(CPalmRecord ***ppRecordList, DWORD * pListSize);
    long LoadAllRecordsInCategory(DWORD catIndex, CPalmRecord ***ppRecordList, DWORD * pListSize);
    long LoadAllUpdatedRecords(CPalmRecord ***ppRecordList, DWORD * pListSize);
    long LoadUpdatedRecordsInCategory(DWORD catIndex, CPalmRecord ***ppRecordList, DWORD * pListSize);

    long AddRecords(CPalmRecord **m_pRecordList, DWORD pCount);
    long DeleteRecords(CPalmRecord **m_pRecordList, DWORD pCount);
    long UpdateRecords(CPalmRecord **m_pRecordList, DWORD pCount);

    long AddARecord(CPalmRecord & palmRec);
    long DeleteARecord(CPalmRecord & palmRec);
    long UpdateARecord(CPalmRecord & palmRec);

protected:
    char  m_szName[SYNC_DB_NAMELEN];
    DWORD m_dwCreator;
    DWORD m_dwType;
    WORD  m_wFlags;
    WORD  m_wVersion;
    int   m_CardNum;
    BOOL  m_bRecordDB;

    DWORD m_dwMaxRecordCount;

    CRawRecordInfo  m_rInfo;
    CPCategoryMgr * m_pCatMgr;

    BYTE m_hhDB;
    CDbGenInfo m_appInfo;
    
    long AllocateRawRecord();
    long AllocateDBInfo(CDbGenInfo &info, BOOL bClearData=FALSE);
    long GetInfoBlock(CDbGenInfo &info, BOOL bSortInfo=FALSE);
    long GetAppInfo(CDbGenInfo &rInfo);
    long SetName( char *pName);
    long LoadUpdatedRecords(DWORD catIndex, CPalmRecord ***ppRecordList, DWORD * pListSize);

};

#endif