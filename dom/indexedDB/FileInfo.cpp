/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "IndexedDatabaseManager.h"
#include "FileInfo.h"

USING_INDEXEDDB_NAMESPACE

// static
FileInfo*
FileInfo::Create(FileManager* aFileManager, PRInt64 aId)
{
  NS_ASSERTION(aId > 0, "Wrong id!");

  if (aId <= PR_INT16_MAX) {
    return new FileInfo16(aFileManager, aId);
  }

  if (aId <= PR_INT32_MAX) {
    return new FileInfo32(aFileManager, aId);
  }

  return new FileInfo64(aFileManager, aId);
}

void
FileInfo::GetReferences(PRInt32* aRefCnt, PRInt32* aDBRefCnt,
                        PRInt32* aSliceRefCnt)
{
  if (IndexedDatabaseManager::IsClosed()) {
    NS_ERROR("Shouldn't be called after shutdown!");

    if (aRefCnt) {
      *aRefCnt = -1;
    }

    if (aDBRefCnt) {
      *aDBRefCnt = -1;
    }

    if (aSliceRefCnt) {
      *aSliceRefCnt = -1;
    }

    return;
  }

  MutexAutoLock lock(IndexedDatabaseManager::FileMutex());

  if (aRefCnt) {
    *aRefCnt = mRefCnt;
  }

  if (aDBRefCnt) {
    *aDBRefCnt = mDBRefCnt;
  }

  if (aSliceRefCnt) {
    *aSliceRefCnt = mSliceRefCnt;
  }
}

void
FileInfo::UpdateReferences(nsAutoRefCnt& aRefCount, PRInt32 aDelta,
                           bool aClear)
{
  if (IndexedDatabaseManager::IsClosed()) {
    NS_ERROR("Shouldn't be called after shutdown!");
    return;
  }

  {
    MutexAutoLock lock(IndexedDatabaseManager::FileMutex());

    aRefCount = aClear ? 0 : aRefCount + aDelta;

    if (mRefCnt + mDBRefCnt + mSliceRefCnt > 0) {
      return;
    }

    mFileManager->mFileInfos.Remove(Id());
  }

  Cleanup();

  delete this;
}

void
FileInfo::Cleanup()
{
  if (IndexedDatabaseManager::IsShuttingDown() ||
      mFileManager->Invalidated()) {
    return;
  }

  nsRefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "Shouldn't be null!");

  if (NS_FAILED(mgr->AsyncDeleteFile(mFileManager, Id()))) {
    NS_WARNING("Failed to delete file asynchronously!");
  }
}
