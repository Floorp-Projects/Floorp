/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMFile_h__
#define nsDOMFile_h__

#include "mozilla/Attributes.h"
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
#include "nsAutoPtr.h"
#include "nsFileStreams.h"
#include "nsTemporaryFileInputStream.h"

#include "mozilla/GuardObjects.h"
#include "mozilla/LinkedList.h"
#include "mozilla/StandardInteger.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/indexedDB/FileInfo.h"
#include "mozilla/dom/indexedDB/FileManager.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"

class nsIFile;
class nsIInputStream;
class nsIClassInfo;

class nsDOMFileBase : public nsIDOMFile,
                      public nsIXHRSendable,
                      public nsIMutable
{
public:
  typedef mozilla::dom::indexedDB::FileInfo FileInfo;

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType) = 0;

  virtual const nsTArray<nsCOMPtr<nsIDOMBlob> >*
  GetSubBlobs() const { return nullptr; }

  NS_DECL_NSIDOMBLOB
  NS_DECL_NSIDOMFILE
  NS_DECL_NSIXHRSENDABLE
  NS_DECL_NSIMUTABLE

  void
  SetLazyData(const nsAString& aName, const nsAString& aContentType,
              uint64_t aLength, uint64_t aLastModifiedDate)
  {
    NS_ASSERTION(aLength, "must have length");

    mName = aName;
    mContentType = aContentType;
    mLength = aLength;
    mLastModificationDate = aLastModifiedDate;
    mIsFile = !aName.IsVoid();
  }

  bool IsSizeUnknown() const
  {
    return mLength == UINT64_MAX;
  }

  bool IsDateUnknown() const
  {
    return mIsFile && mLastModificationDate == UINT64_MAX;
  }

protected:
  nsDOMFileBase(const nsAString& aName, const nsAString& aContentType,
                uint64_t aLength, uint64_t aLastModifiedDate)
    : mIsFile(true), mImmutable(false), mContentType(aContentType),
      mName(aName), mStart(0), mLength(aLength), mLastModificationDate(aLastModifiedDate)
  {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  nsDOMFileBase(const nsAString& aName, const nsAString& aContentType,
                uint64_t aLength)
    : mIsFile(true), mImmutable(false), mContentType(aContentType),
      mName(aName), mStart(0), mLength(aLength), mLastModificationDate(UINT64_MAX)
  {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  nsDOMFileBase(const nsAString& aContentType, uint64_t aLength)
    : mIsFile(false), mImmutable(false), mContentType(aContentType),
      mStart(0), mLength(aLength), mLastModificationDate(UINT64_MAX)
  {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  nsDOMFileBase(const nsAString& aContentType, uint64_t aStart,
                uint64_t aLength)
    : mIsFile(false), mImmutable(false), mContentType(aContentType),
      mStart(aStart), mLength(aLength), mLastModificationDate(UINT64_MAX)
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

  uint64_t mStart;
  uint64_t mLength;

  uint64_t mLastModificationDate;

  // Protected by IndexedDatabaseManager::FileMutex()
  nsTArray<nsRefPtr<FileInfo> > mFileInfos;
};

class nsDOMFile : public nsDOMFileBase
{
public:
  nsDOMFile(const nsAString& aName, const nsAString& aContentType,
            uint64_t aLength, uint64_t aLastModifiedDate)
  : nsDOMFileBase(aName, aContentType, aLength, aLastModifiedDate)
  { }

  nsDOMFile(const nsAString& aName, const nsAString& aContentType,
            uint64_t aLength)
  : nsDOMFileBase(aName, aContentType, aLength)
  { }

  nsDOMFile(const nsAString& aContentType, uint64_t aLength)
  : nsDOMFileBase(aContentType, aLength)
  { }

  nsDOMFile(const nsAString& aContentType, uint64_t aStart, uint64_t aLength)
  : nsDOMFileBase(aContentType, aStart, aLength)
  { }

  NS_DECL_THREADSAFE_ISUPPORTS
};

class nsDOMFileCC : public nsDOMFileBase
{
public:
  nsDOMFileCC(const nsAString& aName, const nsAString& aContentType,
              uint64_t aLength)
  : nsDOMFileBase(aName, aContentType, aLength)
  { }

  nsDOMFileCC(const nsAString& aContentType, uint64_t aLength)
  : nsDOMFileBase(aContentType, aLength)
  { }

  nsDOMFileCC(const nsAString& aContentType, uint64_t aStart, uint64_t aLength)
  : nsDOMFileBase(aContentType, aStart, aLength)
  { }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMFileCC, nsIDOMFile)
};

class nsDOMFileFile : public nsDOMFile
{
public:
  // Create as a file
  nsDOMFileFile(nsIFile *aFile)
    : nsDOMFile(EmptyString(), EmptyString(), UINT64_MAX, UINT64_MAX),
      mFile(aFile), mWholeFile(true), mStoredFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
    mFile->GetLeafName(mName);
  }

  nsDOMFileFile(nsIFile *aFile, FileInfo *aFileInfo)
    : nsDOMFile(EmptyString(), EmptyString(), UINT64_MAX, UINT64_MAX),
      mFile(aFile), mWholeFile(true), mStoredFile(true)
  {
    NS_ASSERTION(mFile, "must have file");
    NS_ASSERTION(aFileInfo, "must have file info");
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
    mFile->GetLeafName(mName);

    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a file
  nsDOMFileFile(const nsAString& aName, const nsAString& aContentType,
                uint64_t aLength, nsIFile *aFile)
    : nsDOMFile(aName, aContentType, aLength, UINT64_MAX),
      mFile(aFile), mWholeFile(true), mStoredFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
  }

  nsDOMFileFile(const nsAString& aName, const nsAString& aContentType,
                uint64_t aLength, nsIFile *aFile, uint64_t aLastModificationDate)
    : nsDOMFile(aName, aContentType, aLength, aLastModificationDate),
      mFile(aFile), mWholeFile(true), mStoredFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
  }

  // Create as a file with custom name
  nsDOMFileFile(nsIFile *aFile, const nsAString& aName,
                const nsAString& aContentType)
    : nsDOMFile(aName, aContentType, UINT64_MAX, UINT64_MAX),
      mFile(aFile), mWholeFile(true), mStoredFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
    if (aContentType.IsEmpty()) {
      // Lazily get the content type and size
      mContentType.SetIsVoid(true);
    }
  }

  // Create as a stored file
  nsDOMFileFile(const nsAString& aName, const nsAString& aContentType,
                uint64_t aLength, nsIFile* aFile,
                FileInfo* aFileInfo)
    : nsDOMFile(aName, aContentType, aLength, UINT64_MAX),
      mFile(aFile), mWholeFile(true), mStoredFile(true)
  {
    NS_ASSERTION(mFile, "must have file");
    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a stored blob
  nsDOMFileFile(const nsAString& aContentType, uint64_t aLength,
                nsIFile* aFile, FileInfo* aFileInfo)
    : nsDOMFile(aContentType, aLength),
      mFile(aFile), mWholeFile(true), mStoredFile(true)
  {
    NS_ASSERTION(mFile, "must have file");
    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a file to be later initialized
  nsDOMFileFile()
    : nsDOMFile(EmptyString(), EmptyString(), UINT64_MAX, UINT64_MAX),
      mWholeFile(true), mStoredFile(false)
  {
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
    mName.SetIsVoid(true);
  }

  // Overrides
  NS_IMETHOD GetSize(uint64_t* aSize) MOZ_OVERRIDE;
  NS_IMETHOD GetType(nsAString& aType) MOZ_OVERRIDE;
  NS_IMETHOD GetLastModifiedDate(JSContext* cx, JS::Value* aLastModifiedDate) MOZ_OVERRIDE;
  NS_IMETHOD GetMozLastModifiedDate(uint64_t* aLastModifiedDate) MOZ_OVERRIDE;
  NS_IMETHOD GetMozFullPathInternal(nsAString& aFullPath) MOZ_OVERRIDE;
  NS_IMETHOD GetInternalStream(nsIInputStream**) MOZ_OVERRIDE;

protected:
  // Create slice
  nsDOMFileFile(const nsDOMFileFile* aOther, uint64_t aStart, uint64_t aLength,
                const nsAString& aContentType)
    : nsDOMFile(aContentType, aOther->mStart + aStart, aLength),
      mFile(aOther->mFile), mWholeFile(false),
      mStoredFile(aOther->mStoredFile)
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
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType) MOZ_OVERRIDE;

  virtual bool IsStoredFile() const MOZ_OVERRIDE
  {
    return mStoredFile;
  }

  virtual bool IsWholeFile() const MOZ_OVERRIDE
  {
    return mWholeFile;
  }

  nsCOMPtr<nsIFile> mFile;
  bool mWholeFile;
  bool mStoredFile;
};

/**
 * This class may be used off the main thread, and in particular, its
 * constructor and destructor may not run on the same thread.  Be careful!
 */
class nsDOMMemoryFile : public nsDOMFile
{
public:
  // Create as file
  nsDOMMemoryFile(void *aMemoryBuffer,
                  uint64_t aLength,
                  const nsAString& aName,
                  const nsAString& aContentType)
    : nsDOMFile(aName, aContentType, aLength, UINT64_MAX),
      mDataOwner(new DataOwner(aMemoryBuffer, aLength))
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
  }

  // Create as blob
  nsDOMMemoryFile(void *aMemoryBuffer,
                  uint64_t aLength,
                  const nsAString& aContentType)
    : nsDOMFile(aContentType, aLength),
      mDataOwner(new DataOwner(aMemoryBuffer, aLength))
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
  }

  NS_IMETHOD GetInternalStream(nsIInputStream**) MOZ_OVERRIDE;

protected:
  // Create slice
  nsDOMMemoryFile(const nsDOMMemoryFile* aOther, uint64_t aStart,
                  uint64_t aLength, const nsAString& aContentType)
    : nsDOMFile(aContentType, aOther->mStart + aStart, aLength),
      mDataOwner(aOther->mDataOwner)
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
    mImmutable = aOther->mImmutable;
  }
  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType) MOZ_OVERRIDE;

  // These classes need to see DataOwner.
  friend class DataOwnerAdapter;
  friend class nsDOMMemoryFileDataOwnerMemoryReporter;

  class DataOwner : public mozilla::LinkedListElement<DataOwner> {
  public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DataOwner)
    DataOwner(void* aMemoryBuffer, uint64_t aLength)
      : mData(aMemoryBuffer)
      , mLength(aLength)
    {
      mozilla::StaticMutexAutoLock lock(sDataOwnerMutex);

      if (!sDataOwners) {
        sDataOwners = new mozilla::LinkedList<DataOwner>();
        EnsureMemoryReporterRegistered();
      }
      sDataOwners->insertBack(this);
    }

    ~DataOwner() {
      mozilla::StaticMutexAutoLock lock(sDataOwnerMutex);

      remove();
      if (sDataOwners->isEmpty()) {
        // Free the linked list if it's empty.
        sDataOwners = nullptr;
      }

      moz_free(mData);
    }

    static void EnsureMemoryReporterRegistered();

    // sDataOwners and sMemoryReporterRegistered may only be accessed while
    // holding sDataOwnerMutex!  You also must hold the mutex while touching
    // elements of the linked list that DataOwner inherits from.
    static mozilla::StaticMutex sDataOwnerMutex;
    static mozilla::StaticAutoPtr<mozilla::LinkedList<DataOwner> > sDataOwners;
    static bool sMemoryReporterRegistered;

    void* mData;
    uint64_t mLength;
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

  virtual JSObject* WrapObject(JSContext *cx,
                               JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

  nsISupports* GetParentObject()
  {
    return mParent;
  }

  void Disconnect()
  {
    mParent = nullptr;
  }

  bool Append(nsIDOMFile *aFile) { return mFiles.AppendObject(aFile); }

  bool Remove(uint32_t aIndex) { return mFiles.RemoveObjectAt(aIndex); }
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

  nsIDOMFile* Item(uint32_t aIndex)
  {
    return mFiles.SafeObjectAt(aIndex);
  }
  nsIDOMFile* IndexedGetter(uint32_t aIndex, bool& aFound)
  {
    aFound = aIndex < static_cast<uint32_t>(mFiles.Count());
    return aFound ? mFiles.ObjectAt(aIndex) : nullptr;
  }
  uint32_t Length()
  {
    return mFiles.Count();
  }

private:
  nsCOMArray<nsIDOMFile> mFiles;
  nsISupports *mParent;
};

class MOZ_STACK_CLASS nsDOMFileInternalUrlHolder {
public:
  nsDOMFileInternalUrlHolder(nsIDOMBlob* aFile, nsIPrincipal* aPrincipal
                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
  ~nsDOMFileInternalUrlHolder();
  nsAutoString mUrl;
private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};
// This class would take the ownership of aFD and the caller must not close it.
class nsDOMTemporaryFileBlob : public nsDOMFile
{
public:
  nsDOMTemporaryFileBlob(PRFileDesc* aFD, uint64_t aStartPos, uint64_t aLength,
                         const nsAString& aContentType)
    : nsDOMFile(aContentType, aLength),
      mLength(aLength),
      mStartPos(aStartPos),
      mContentType(aContentType)
  {
    mFileDescOwner = new nsTemporaryFileInputStream::FileDescOwner(aFD);
  }

  ~nsDOMTemporaryFileBlob() { }
  NS_IMETHOD GetInternalStream(nsIInputStream**) MOZ_OVERRIDE;

protected:
  nsDOMTemporaryFileBlob(const nsDOMTemporaryFileBlob* aOther, uint64_t aStart, uint64_t aLength,
                         const nsAString& aContentType)
    : nsDOMFile(aContentType, aLength),
      mLength(aLength),
      mStartPos(aStart),
      mFileDescOwner(aOther->mFileDescOwner),
      mContentType(aContentType) { }

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType) MOZ_OVERRIDE;

private:
  uint64_t mLength;
  uint64_t mStartPos;
  nsRefPtr<nsTemporaryFileInputStream::FileDescOwner> mFileDescOwner;
  nsString mContentType;
};

#endif
