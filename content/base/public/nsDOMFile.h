/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozila.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
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

#ifndef nsDOMFile_h__
#define nsDOMFile_h__

#include "nsICharsetDetectionObserver.h"
#include "nsIFile.h"
#include "nsIDOMFile.h"
#include "nsIDOMFileList.h"
#include "nsIInputStream.h"
#include "nsIJSNativeInitializer.h"
#include "nsIMutable.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIXMLHttpRequest.h"
#include "prmem.h"
#include "nsAutoPtr.h"

#include "mozilla/GuardObjects.h"
#include "mozilla/StandardInteger.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/indexedDB/FileInfo.h"
#include "mozilla/dom/indexedDB/FileManager.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"

class nsIFile;
class nsIInputStream;
class nsIClassInfo;
class nsIBlobBuilder;

nsresult NS_NewBlobBuilder(nsISupports* *aSupports);

class nsDOMFileBase : public nsIDOMFile,
                      public nsIXHRSendable,
                      public nsIMutable
{
public:
  typedef mozilla::dom::indexedDB::FileInfo FileInfo;

  nsDOMFileBase(const nsAString& aName, const nsAString& aContentType,
                PRUint64 aLength)
    : mIsFile(true), mImmutable(false), mContentType(aContentType),
      mName(aName), mStart(0), mLength(aLength)
  {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  nsDOMFileBase(const nsAString& aContentType, PRUint64 aLength)
    : mIsFile(false), mImmutable(false), mContentType(aContentType),
      mStart(0), mLength(aLength)
  {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  nsDOMFileBase(const nsAString& aContentType,
                PRUint64 aStart, PRUint64 aLength)
    : mIsFile(false), mImmutable(false), mContentType(aContentType),
      mStart(aStart), mLength(aLength)
  {
    NS_ASSERTION(aLength != UINT64_MAX,
                 "Must know length when creating slice");
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  virtual ~nsDOMFileBase() {}

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(PRUint64 aStart, PRUint64 aLength,
              const nsAString& aContentType) = 0;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMBLOB
  NS_DECL_NSIDOMFILE
  NS_DECL_NSIXHRSENDABLE
  NS_DECL_NSIMUTABLE

protected:
  bool IsSizeUnknown()
  {
    return mLength == UINT64_MAX;
  }

  virtual bool IsStoredFile()
  {
    return false;
  }

  virtual bool IsWholeFile()
  {
    NS_NOTREACHED("Should only be called on dom blobs backed by files!");
    return false;
  }

  bool mIsFile;
  bool mImmutable;
  nsString mContentType;
  nsString mName;

  PRUint64 mStart;
  PRUint64 mLength;

  // Protected by IndexedDatabaseManager::FileMutex()
  nsTArray<nsRefPtr<FileInfo> > mFileInfos;
};

class nsDOMFileFile : public nsDOMFileBase,
                      public nsIJSNativeInitializer
{
public:
  // Create as a file
  nsDOMFileFile(nsIFile *aFile)
    : nsDOMFileBase(EmptyString(), EmptyString(), UINT64_MAX),
      mFile(aFile), mWholeFile(true), mStoredFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
    mFile->GetLeafName(mName);
  }

  // Create as a blob
  nsDOMFileFile(nsIFile *aFile, const nsAString& aContentType,
                nsISupports *aCacheToken = nsnull)
    : nsDOMFileBase(aContentType, UINT64_MAX),
      mFile(aFile), mWholeFile(true), mStoredFile(false),
      mCacheToken(aCacheToken)
  {
    NS_ASSERTION(mFile, "must have file");
  }

  // Create as a stored file
  nsDOMFileFile(const nsAString& aName, const nsAString& aContentType,
                PRUint64 aLength, nsIFile* aFile,
                FileInfo* aFileInfo)
    : nsDOMFileBase(aName, aContentType, aLength),
      mFile(aFile), mWholeFile(true), mStoredFile(true)
  {
    NS_ASSERTION(mFile, "must have file");
    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a stored blob
  nsDOMFileFile(const nsAString& aContentType, PRUint64 aLength,
                nsIFile* aFile, FileInfo* aFileInfo)
    : nsDOMFileBase(aContentType, aLength),
      mFile(aFile), mWholeFile(true), mStoredFile(true)
  {
    NS_ASSERTION(mFile, "must have file");
    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a file to be later initialized
  nsDOMFileFile()
    : nsDOMFileBase(EmptyString(), EmptyString(), UINT64_MAX),
      mWholeFile(true), mStoredFile(false)
  {
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
    mName.SetIsVoid(true);
  }

  NS_DECL_ISUPPORTS_INHERITED

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner,
                        JSContext* aCx,
                        JSObject* aObj,
                        PRUint32 aArgc,
                        jsval* aArgv);

  // Overrides
  NS_IMETHOD GetSize(PRUint64* aSize);
  NS_IMETHOD GetType(nsAString& aType);
  NS_IMETHOD GetMozFullPathInternal(nsAString& aFullPath);
  NS_IMETHOD GetInternalStream(nsIInputStream**);

  // DOMClassInfo constructor (for File("foo"))
  static nsresult
  NewFile(nsISupports* *aNewObject);

protected:
  // Create slice
  nsDOMFileFile(const nsDOMFileFile* aOther, PRUint64 aStart, PRUint64 aLength,
                const nsAString& aContentType)
    : nsDOMFileBase(aContentType, aOther->mStart + aStart, aLength),
      mFile(aOther->mFile), mWholeFile(false),
      mStoredFile(aOther->mStoredFile), mCacheToken(aOther->mCacheToken)
  {
    NS_ASSERTION(mFile, "must have file");
    mImmutable = aOther->mImmutable;

    if (mStoredFile) {
      FileInfo* fileInfo;

      if (!mozilla::dom::indexedDB::IndexedDatabaseManager::IsClosed()) {
        mozilla::dom::indexedDB::IndexedDatabaseManager::FileMutex().Lock();
      }

      NS_ASSERTION(!aOther->mFileInfos.IsEmpty(),
                   "A stored file must have at least one file info!");

      fileInfo = aOther->mFileInfos.ElementAt(0);

      if (!mozilla::dom::indexedDB::IndexedDatabaseManager::IsClosed()) {
        mozilla::dom::indexedDB::IndexedDatabaseManager::FileMutex().Unlock();
      }

      mFileInfos.AppendElement(fileInfo);
    }
  }
  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(PRUint64 aStart, PRUint64 aLength,
              const nsAString& aContentType);

  virtual bool IsStoredFile()
  {
    return mStoredFile;
  }

  virtual bool IsWholeFile()
  {
    return mWholeFile;
  }

  nsCOMPtr<nsIFile> mFile;
  bool mWholeFile;
  bool mStoredFile;
  nsCOMPtr<nsISupports> mCacheToken;
};

class nsDOMMemoryFile : public nsDOMFileBase
{
public:
  // Create as file
  nsDOMMemoryFile(void *aMemoryBuffer,
                  PRUint64 aLength,
                  const nsAString& aName,
                  const nsAString& aContentType)
    : nsDOMFileBase(aName, aContentType, aLength),
      mDataOwner(new DataOwner(aMemoryBuffer))
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
  }

  // Create as blob
  nsDOMMemoryFile(void *aMemoryBuffer,
                  PRUint64 aLength,
                  const nsAString& aContentType)
    : nsDOMFileBase(aContentType, aLength),
      mDataOwner(new DataOwner(aMemoryBuffer))
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
  }

  NS_IMETHOD GetInternalStream(nsIInputStream**);

protected:
  // Create slice
  nsDOMMemoryFile(const nsDOMMemoryFile* aOther, PRUint64 aStart,
                  PRUint64 aLength, const nsAString& aContentType)
    : nsDOMFileBase(aContentType, aOther->mStart + aStart, aLength),
      mDataOwner(aOther->mDataOwner)
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
    mImmutable = aOther->mImmutable;
  }
  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(PRUint64 aStart, PRUint64 aLength,
              const nsAString& aContentType);

  friend class DataOwnerAdapter; // Needs to see DataOwner
  class DataOwner {
  public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DataOwner)
    DataOwner(void* aMemoryBuffer)
      : mData(aMemoryBuffer)
    {
    }
    ~DataOwner() {
      PR_Free(mData);
    }
    void* mData;
  };

  // Used when backed by a memory store
  nsRefPtr<DataOwner> mDataOwner;
};

class nsDOMFileList : public nsIDOMFileList
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMFILELIST

  bool Append(nsIDOMFile *aFile) { return mFiles.AppendObject(aFile); }

  bool Remove(PRUint32 aIndex) { return mFiles.RemoveObjectAt(aIndex); }
  void Clear() { return mFiles.Clear(); }

  nsIDOMFile* GetItemAt(PRUint32 aIndex)
  {
    return mFiles.SafeObjectAt(aIndex);
  }

  static nsDOMFileList* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMFileList> list_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMFileList pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(list_qi == static_cast<nsIDOMFileList*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<nsDOMFileList*>(aSupports);
  }

private:
  nsCOMArray<nsIDOMFile> mFiles;
};

class NS_STACK_CLASS nsDOMFileInternalUrlHolder {
public:
  nsDOMFileInternalUrlHolder(nsIDOMBlob* aFile, nsIPrincipal* aPrincipal
                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
  ~nsDOMFileInternalUrlHolder();
  nsAutoString mUrl;
private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

#endif
