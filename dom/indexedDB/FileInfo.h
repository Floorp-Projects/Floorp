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

#ifndef mozilla_dom_indexeddb_fileinfo_h__
#define mozilla_dom_indexeddb_fileinfo_h__

#include "IndexedDatabase.h"
#include "nsAtomicRefcnt.h"
#include "nsThreadUtils.h"
#include "FileManager.h"
#include "IndexedDatabaseManager.h"

BEGIN_INDEXEDDB_NAMESPACE

class FileInfo
{
  friend class FileManager;

public:
  FileInfo(FileManager* aFileManager)
  : mFileManager(aFileManager)
  { }

  virtual ~FileInfo()
  {
#ifdef DEBUG
    NS_ASSERTION(NS_IsMainThread(), "File info destroyed on wrong thread!");
#endif
  }

  static
  FileInfo* Create(FileManager* aFileManager, PRInt64 aId);

  void AddRef()
  {
    if (IndexedDatabaseManager::IsClosed()) {
      NS_AtomicIncrementRefcnt(mRefCnt);
    }
    else {
      UpdateReferences(mRefCnt, 1);
    }
  }

  void Release()
  {
    if (IndexedDatabaseManager::IsClosed()) {
      nsrefcnt count = NS_AtomicDecrementRefcnt(mRefCnt);
      if (count == 0) {
        mRefCnt = 1;
        delete this;
      }
    }
    else {
      UpdateReferences(mRefCnt, -1);
    }
  }

  void UpdateDBRefs(PRInt32 aDelta)
  {
    UpdateReferences(mDBRefCnt, aDelta);
  }

  void ClearDBRefs()
  {
    UpdateReferences(mDBRefCnt, 0, true);
  }

  void UpdateSliceRefs(PRInt32 aDelta)
  {
    UpdateReferences(mSliceRefCnt, aDelta);
  }

  void GetReferences(PRInt32* aRefCnt, PRInt32* aDBRefCnt,
                     PRInt32* aSliceRefCnt);

  FileManager* Manager() const
  {
    return mFileManager;
  }

  virtual PRInt64 Id() const = 0;

private:
  void UpdateReferences(nsAutoRefCnt& aRefCount, PRInt32 aDelta,
                        bool aClear = false);
  void Cleanup();

  nsAutoRefCnt mRefCnt;
  nsAutoRefCnt mDBRefCnt;
  nsAutoRefCnt mSliceRefCnt;

  nsRefPtr<FileManager> mFileManager;
};

#define FILEINFO_SUBCLASS(_bits)                                              \
class FileInfo##_bits : public FileInfo                                       \
{                                                                             \
public:                                                                       \
  FileInfo##_bits(FileManager* aFileManager, PRInt64 aId)                     \
  : FileInfo(aFileManager), mId(aId)                                          \
  { }                                                                         \
                                                                              \
  virtual PRInt64 Id() const                                                  \
  {                                                                           \
    return mId;                                                               \
  }                                                                           \
                                                                              \
private:                                                                      \
  PRInt##_bits mId;                                                           \
};

FILEINFO_SUBCLASS(16);
FILEINFO_SUBCLASS(32);
FILEINFO_SUBCLASS(64);

#undef FILEINFO_SUBCLASS

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_fileinfo_h__
