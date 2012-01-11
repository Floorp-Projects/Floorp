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

#ifndef mozilla_dom_indexeddb_filemanager_h__
#define mozilla_dom_indexeddb_filemanager_h__

#include "IndexedDatabase.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIDOMFile.h"
#include "nsDataHashtable.h"

class mozIStorageConnection;

BEGIN_INDEXEDDB_NAMESPACE

class FileInfo;

class FileManager
{
  friend class FileInfo;

public:
  FileManager(const nsACString& aOrigin,
              const nsAString& aDatabaseName)
  : mOrigin(aOrigin), mDatabaseName(aDatabaseName), mLastFileId(0),
    mLoaded(false), mInvalidated(false)
  { }

  ~FileManager()
  { }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileManager)

  const nsACString& Origin() const
  {
    return mOrigin;
  }

  const nsAString& DatabaseName() const
  {
    return mDatabaseName;
  }

  bool Inited() const
  {
    return !mDirectoryPath.IsEmpty();
  }

  bool Loaded() const
  {
    return mLoaded;
  }

  bool Invalidated() const
  {
    return mInvalidated;
  }

  nsresult Init(nsIFile* aDirectory,
                mozIStorageConnection* aConnection);

  nsresult Load(mozIStorageConnection* aConnection);

  nsresult Invalidate();

  already_AddRefed<nsIFile> GetDirectory();

  already_AddRefed<FileInfo> GetFileInfo(PRInt64 aId);

  already_AddRefed<FileInfo> GetNewFileInfo();

  static already_AddRefed<nsIFile> GetFileForId(nsIFile* aDirectory,
                                                PRInt64 aId);

private:
  nsCString mOrigin;
  nsString mDatabaseName;

  nsString mDirectoryPath;

  PRInt64 mLastFileId;

  // Protected by IndexedDatabaseManager::FileMutex()
  nsDataHashtable<nsUint64HashKey, FileInfo*> mFileInfos;

  bool mLoaded;
  bool mInvalidated;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_filemanager_h__
