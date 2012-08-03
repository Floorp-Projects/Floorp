/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"

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

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(PRUint64 aStart, PRUint64 aLength,
              const nsAString& aContentType) = 0;

  virtual const nsTArray<nsCOMPtr<nsIDOMBlob> >*
  GetSubBlobs() const { return nullptr; }

  NS_DECL_NSIDOMBLOB
  NS_DECL_NSIDOMFILE
  NS_DECL_NSIXHRSENDABLE
  NS_DECL_NSIMUTABLE

  void
  SetLazyData(const nsAString& aName, const nsAString& aContentType,
              PRUint64 aLength)
  {
    NS_ASSERTION(aLength, "must have length");

    mName = aName;
    mContentType = aContentType;
    mLength = aLength;

    mIsFile = !aName.IsVoid();
  }

  bool IsSizeUnknown() const
  {
    return mLength == UINT64_MAX;
  }

protected:
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

  nsDOMFileBase(const nsAString& aContentType, PRUint64 aStart,
                PRUint64 aLength)
    : mIsFile(false), mImmutable(false), mContentType(aContentType),
      mStart(aStart), mLength(aLength)
  {
    NS_ASSERTION(aLength != UINT64_MAX,
                 "Must know length when creating slice");
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  virtual ~nsDOMFileBase() {}

  virtual bool IsStoredFile() const
  {
    return false;
  }

  virtual bool IsWholeFile() const
  {
    NS_NOTREACHED("Should only be called on dom blobs backed by files!");
    return false;
  }

  virtual bool IsSnapshot() const
  {
    return false;
  }

  FileInfo* GetFileInfo() const
  {
    NS_ASSERTION(IsStoredFile(), "Should only be called on stored files!");
    NS_ASSERTION(!mFileInfos.IsEmpty(), "Must have at least one file info!");

    return mFileInfos.ElementAt(0);
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

class nsDOMFile : public nsDOMFileBase
{
public:
  nsDOMFile(const nsAString& aName, const nsAString& aContentType,
            PRUint64 aLength)
  : nsDOMFileBase(aName, aContentType, aLength)
  { }

  nsDOMFile(const nsAString& aContentType, PRUint64 aLength)
  : nsDOMFileBase(aContentType, aLength)
  { }

  nsDOMFile(const nsAString& aContentType, PRUint64 aStart, PRUint64 aLength)
  : nsDOMFileBase(aContentType, aStart, aLength)
  { }

  NS_DECL_ISUPPORTS
};

class nsDOMFileCC : public nsDOMFileBase
{
public:
  nsDOMFileCC(const nsAString& aName, const nsAString& aContentType,
              PRUint64 aLength)
  : nsDOMFileBase(aName, aContentType, aLength)
  { }

  nsDOMFileCC(const nsAString& aContentType, PRUint64 aLength)
  : nsDOMFileBase(aContentType, aLength)
  { }

  nsDOMFileCC(const nsAString& aContentType, PRUint64 aStart, PRUint64 aLength)
  : nsDOMFileBase(aContentType, aStart, aLength)
  { }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMFileCC, nsIDOMFile)
};

class nsDOMFileFile : public nsDOMFile,
                      public nsIJSNativeInitializer
{
public:
  // Create as a file
  nsDOMFileFile(nsIFile *aFile)
    : nsDOMFile(EmptyString(), EmptyString(), UINT64_MAX),
      mFile(aFile), mWholeFile(true), mStoredFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
    mFile->GetLeafName(mName);
  }

  // Create as a file
  nsDOMFileFile(const nsAString& aName, const nsAString& aContentType,
                PRUint64 aLength, nsIFile *aFile)
    : nsDOMFile(aName, aContentType, aLength),
      mFile(aFile), mWholeFile(true), mStoredFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
  }

  // Create as a blob
  nsDOMFileFile(nsIFile *aFile, const nsAString& aContentType,
                nsISupports *aCacheToken)
    : nsDOMFile(aContentType, UINT64_MAX),
      mFile(aFile), mWholeFile(true), mStoredFile(false),
      mCacheToken(aCacheToken)
  {
    NS_ASSERTION(mFile, "must have file");
  }

  // Create as a file with custom name
  nsDOMFileFile(nsIFile *aFile, const nsAString& aName)
    : nsDOMFile(aName, EmptyString(), UINT64_MAX),
      mFile(aFile), mWholeFile(true), mStoredFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
  }

  // Create as a stored file
  nsDOMFileFile(const nsAString& aName, const nsAString& aContentType,
                PRUint64 aLength, nsIFile* aFile,
                FileInfo* aFileInfo)
    : nsDOMFile(aName, aContentType, aLength),
      mFile(aFile), mWholeFile(true), mStoredFile(true)
  {
    NS_ASSERTION(mFile, "must have file");
    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a stored blob
  nsDOMFileFile(const nsAString& aContentType, PRUint64 aLength,
                nsIFile* aFile, FileInfo* aFileInfo)
    : nsDOMFile(aContentType, aLength),
      mFile(aFile), mWholeFile(true), mStoredFile(true)
  {
    NS_ASSERTION(mFile, "must have file");
    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a file to be later initialized
  nsDOMFileFile()
    : nsDOMFile(EmptyString(), EmptyString(), UINT64_MAX),
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
  NS_IMETHOD GetLastModifiedDate(JSContext* cx, JS::Value *aLastModifiedDate);
  NS_IMETHOD GetMozFullPathInternal(nsAString& aFullPath);
  NS_IMETHOD GetInternalStream(nsIInputStream**);

  // DOMClassInfo constructor (for File("foo"))
  static nsresult
  NewFile(nsISupports* *aNewObject);

protected:
  // Create slice
  nsDOMFileFile(const nsDOMFileFile* aOther, PRUint64 aStart, PRUint64 aLength,
                const nsAString& aContentType)
    : nsDOMFile(aContentType, aOther->mStart + aStart, aLength),
      mFile(aOther->mFile), mWholeFile(false),
      mStoredFile(aOther->mStoredFile), mCacheToken(aOther->mCacheToken)
  {
    NS_ASSERTION(mFile, "must have file");
    mImmutable = aOther->mImmutable;

    if (mStoredFile) {
      FileInfo* fileInfo;

      using mozilla::dom::indexedDB::IndexedDatabaseManager;

      if (IndexedDatabaseManager::IsClosed()) {
        fileInfo = aOther->GetFileInfo();
      }
      else {
        mozilla::MutexAutoLock lock(IndexedDatabaseManager::FileMutex());
        fileInfo = aOther->GetFileInfo();
      }

      mFileInfos.AppendElement(fileInfo);
    }
  }

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(PRUint64 aStart, PRUint64 aLength,
              const nsAString& aContentType);

  virtual bool IsStoredFile() const
  {
    return mStoredFile;
  }

  virtual bool IsWholeFile() const
  {
    return mWholeFile;
  }

  nsCOMPtr<nsIFile> mFile;
  bool mWholeFile;
  bool mStoredFile;
  nsCOMPtr<nsISupports> mCacheToken;
};

class nsDOMMemoryFile : public nsDOMFile
{
public:
  // Create as file
  nsDOMMemoryFile(void *aMemoryBuffer,
                  PRUint64 aLength,
                  const nsAString& aName,
                  const nsAString& aContentType)
    : nsDOMFile(aName, aContentType, aLength),
      mDataOwner(new DataOwner(aMemoryBuffer))
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
  }

  // Create as blob
  nsDOMMemoryFile(void *aMemoryBuffer,
                  PRUint64 aLength,
                  const nsAString& aContentType)
    : nsDOMFile(aContentType, aLength),
      mDataOwner(new DataOwner(aMemoryBuffer))
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
  }

  NS_IMETHOD GetInternalStream(nsIInputStream**);

protected:
  // Create slice
  nsDOMMemoryFile(const nsDOMMemoryFile* aOther, PRUint64 aStart,
                  PRUint64 aLength, const nsAString& aContentType)
    : nsDOMFile(aContentType, aOther->mStart + aStart, aLength),
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

class nsDOMFileList MOZ_FINAL : public nsIDOMFileList,
                                public nsWrapperCache
{
public:
  nsDOMFileList(nsISupports *aParent) : mParent(aParent)
  {
    SetIsDOMBinding();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMFileList)

  NS_DECL_NSIDOMFILELIST

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap);

  nsISupports* GetParentObject()
  {
    return mParent;
  }

  void Disconnect()
  {
    mParent = nullptr;
  }

  bool Append(nsIDOMFile *aFile) { return mFiles.AppendObject(aFile); }

  bool Remove(PRUint32 aIndex) { return mFiles.RemoveObjectAt(aIndex); }
  void Clear() { return mFiles.Clear(); }

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
  nsISupports *mParent;
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
