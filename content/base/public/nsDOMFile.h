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
#include <stdint.h>
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/indexedDB/FileInfo.h"
#include "mozilla/dom/indexedDB/FileManager.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"

class nsDOMMultipartFile;
class nsIFile;
class nsIInputStream;
class nsIClassInfo;

#define PIDOMFILEIMPL_IID \
  { 0x218ee173, 0xf44f, 0x4d30, \
    { 0xab, 0x0c, 0xd6, 0x66, 0xea, 0xc2, 0x84, 0x47 } }

class PIDOMFileImpl : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(PIDOMFILEIMPL_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(PIDOMFileImpl, PIDOMFILEIMPL_IID)

namespace mozilla {
namespace dom {

namespace indexedDB {
class FileInfo;
};

class DOMFileImpl;

class DOMFile MOZ_FINAL : public nsIDOMFile
                        , public nsIXHRSendable
                        , public nsIMutable
                        , public nsIJSNativeInitializer
{
public:
  NS_DECL_NSIDOMBLOB
  NS_DECL_NSIDOMFILE
  NS_DECL_NSIXHRSENDABLE
  NS_DECL_NSIMUTABLE

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(DOMFile, nsIDOMFile)

  static already_AddRefed<DOMFile>
  Create(const nsAString& aName, const nsAString& aContentType,
         uint64_t aLength, uint64_t aLastModifiedDate);

  static already_AddRefed<DOMFile>
  Create(const nsAString& aName, const nsAString& aContentType,
         uint64_t aLength);

  static already_AddRefed<DOMFile>
  Create(const nsAString& aContentType, uint64_t aLength);

  static already_AddRefed<DOMFile>
  Create(const nsAString& aContentType, uint64_t aStart,
         uint64_t aLength);

  static already_AddRefed<DOMFile>
  CreateMemoryFile(void* aMemoryBuffer, uint64_t aLength,
                   const nsAString& aName, const nsAString& aContentType,
                   uint64_t aLastModifiedDate);

  static already_AddRefed<DOMFile>
  CreateMemoryFile(void* aMemoryBuffer, uint64_t aLength,
                   const nsAString& aContentType);

  static already_AddRefed<DOMFile>
  CreateTemporaryFileBlob(PRFileDesc* aFD, uint64_t aStartPos,
                          uint64_t aLength,
                          const nsAString& aContentType);

  static already_AddRefed<DOMFile>
  CreateFromFile(nsIFile* aFile);

  static already_AddRefed<DOMFile>
  CreateFromFile(const nsAString& aContentType, uint64_t aLength,
                 nsIFile* aFile, indexedDB::FileInfo* aFileInfo);

  static already_AddRefed<DOMFile>
  CreateFromFile(const nsAString& aName, const nsAString& aContentType,
                 uint64_t aLength, nsIFile* aFile,
                    indexedDB::FileInfo* aFileInfo);

  static already_AddRefed<DOMFile>
  CreateFromFile(nsIFile* aFile, indexedDB::FileInfo* aFileInfo);

  static already_AddRefed<DOMFile>
  CreateFromFile(nsIFile* aFile, const nsAString& aName,
                 const nsAString& aContentType);

  explicit DOMFile(DOMFileImpl* aImpl)
    : mImpl(aImpl)
  {
    MOZ_ASSERT(mImpl);
  }

  DOMFileImpl* Impl() const
  {
    return mImpl;
  }

  const nsTArray<nsRefPtr<DOMFileImpl>>* GetSubBlobImpls() const;

  bool IsSizeUnknown() const;

  bool IsDateUnknown() const;

  bool IsFile() const;

  void SetLazyData(const nsAString& aName, const nsAString& aContentType,
                   uint64_t aLength, uint64_t aLastModifiedDate);

  already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType);

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* aCx, JSObject* aObj,
                        const JS::CallArgs& aArgs) MOZ_OVERRIDE;

private:
  ~DOMFile() {};

  // The member is the real backend implementation of this DOMFile/DOMBlob.
  // It's thread-safe and not CC-able and it's the only element that is moved
  // between threads.
  // Note: we should not store any other state in this class!
  const nsRefPtr<DOMFileImpl> mImpl;
};

// This is the abstract class for any DOMFile backend. It must be nsISupports
// because this class must be ref-counted and it has to work with IPC.
class DOMFileImpl : public PIDOMFileImpl
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  DOMFileImpl() {}

  virtual nsresult GetName(nsAString& aName) = 0;

  virtual nsresult GetPath(nsAString& aName) = 0;

  virtual nsresult
  GetLastModifiedDate(JSContext* aCx,
                      JS::MutableHandle<JS::Value> aDate) = 0;

  virtual nsresult GetMozFullPath(nsAString& aName) = 0;

  virtual nsresult GetMozFullPathInternal(nsAString &aFileName) = 0;

  virtual nsresult GetSize(uint64_t* aSize) = 0;

  virtual nsresult GetType(nsAString& aType) = 0;

  virtual nsresult GetMozLastModifiedDate(uint64_t* aDate) = 0;

  nsresult Slice(int64_t aStart, int64_t aEnd, const nsAString& aContentType,
                 uint8_t aArgc, nsIDOMBlob **aBlob);

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType) = 0;

  virtual const nsTArray<nsRefPtr<DOMFileImpl>>*
  GetSubBlobImpls() const = 0;

  virtual nsresult GetInternalStream(nsIInputStream** aStream) = 0;

  virtual nsresult
  GetInternalUrl(nsIPrincipal* aPrincipal, nsAString& aURL) = 0;

  virtual int64_t GetFileId() = 0;

  virtual void AddFileInfo(indexedDB::FileInfo* aFileInfo) = 0;

  virtual indexedDB::FileInfo*
  GetFileInfo(indexedDB::FileManager* aFileManager) = 0;

  virtual nsresult GetSendInfo(nsIInputStream** aBody,
                               uint64_t* aContentLength,
                               nsACString& aContentType,
                               nsACString& aCharset) = 0;

  virtual nsresult GetMutable(bool* aMutable) const = 0;

  virtual nsresult SetMutable(bool aMutable) = 0;

  virtual void SetLazyData(const nsAString& aName,
                           const nsAString& aContentType,
                           uint64_t aLength,
                           uint64_t aLastModifiedDate) = 0;

  virtual bool IsMemoryFile() const = 0;

  virtual bool IsSizeUnknown() const = 0;

  virtual bool IsDateUnknown() const = 0;

  virtual bool IsFile() const = 0;

  virtual nsresult Initialize(nsISupports* aOwner, JSContext* aCx,
                              JSObject* aObj, const JS::CallArgs& aArgs) = 0;

  // These 2 methods are used when the implementation has to CC something.
  virtual void Unlink() = 0;
  virtual void Traverse(nsCycleCollectionTraversalCallback &aCb) = 0;

  virtual bool IsCCed() const
  {
    return false;
  }

protected:
  virtual ~DOMFileImpl() {}
};

class DOMFileImplBase : public DOMFileImpl
{
public:
  DOMFileImplBase(const nsAString& aName, const nsAString& aContentType,
                  uint64_t aLength, uint64_t aLastModifiedDate)
    : mIsFile(true)
    , mImmutable(false)
    , mContentType(aContentType)
    , mName(aName)
    , mStart(0)
    , mLength(aLength)
    , mLastModificationDate(aLastModifiedDate)
  {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  DOMFileImplBase(const nsAString& aName, const nsAString& aContentType,
                  uint64_t aLength)
    : mIsFile(true)
    , mImmutable(false)
    , mContentType(aContentType)
    , mName(aName)
    , mStart(0)
    , mLength(aLength)
    , mLastModificationDate(UINT64_MAX)
  {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  DOMFileImplBase(const nsAString& aContentType, uint64_t aLength)
    : mIsFile(false)
    , mImmutable(false)
    , mContentType(aContentType)
    , mStart(0)
    , mLength(aLength)
    , mLastModificationDate(UINT64_MAX)
  {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  DOMFileImplBase(const nsAString& aContentType, uint64_t aStart,
                  uint64_t aLength)
    : mIsFile(false)
    , mImmutable(false)
    , mContentType(aContentType)
    , mStart(aStart)
    , mLength(aLength)
    , mLastModificationDate(UINT64_MAX)
  {
    NS_ASSERTION(aLength != UINT64_MAX,
                 "Must know length when creating slice");
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  virtual nsresult GetName(nsAString& aName) MOZ_OVERRIDE;

  virtual nsresult GetPath(nsAString& aName) MOZ_OVERRIDE;

  virtual nsresult GetLastModifiedDate(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aDate) MOZ_OVERRIDE;

  virtual nsresult GetMozFullPath(nsAString& aName) MOZ_OVERRIDE;

  virtual nsresult GetMozFullPathInternal(nsAString& aFileName) MOZ_OVERRIDE;

  virtual nsresult GetSize(uint64_t* aSize) MOZ_OVERRIDE;

  virtual nsresult GetType(nsAString& aType) MOZ_OVERRIDE;

  virtual nsresult GetMozLastModifiedDate(uint64_t* aDate) MOZ_OVERRIDE;

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType) MOZ_OVERRIDE;

  virtual const nsTArray<nsRefPtr<DOMFileImpl>>*
  GetSubBlobImpls() const MOZ_OVERRIDE
  {
    return nullptr;
  }

  virtual nsresult GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

  virtual nsresult GetInternalUrl(nsIPrincipal* aPrincipal, nsAString& aURL) MOZ_OVERRIDE;

  virtual int64_t GetFileId() MOZ_OVERRIDE;

  virtual void AddFileInfo(indexedDB::FileInfo* aFileInfo) MOZ_OVERRIDE;

  virtual indexedDB::FileInfo*
  GetFileInfo(indexedDB::FileManager* aFileManager) MOZ_OVERRIDE;

  virtual nsresult GetSendInfo(nsIInputStream** aBody,
                               uint64_t* aContentLength,
                               nsACString& aContentType,
                               nsACString& aCharset) MOZ_OVERRIDE;

  virtual nsresult GetMutable(bool* aMutable) const MOZ_OVERRIDE;

  virtual nsresult SetMutable(bool aMutable) MOZ_OVERRIDE;

  virtual void
  SetLazyData(const nsAString& aName, const nsAString& aContentType,
              uint64_t aLength, uint64_t aLastModifiedDate) MOZ_OVERRIDE
  {
    NS_ASSERTION(aLength, "must have length");

    mName = aName;
    mContentType = aContentType;
    mLength = aLength;
    mLastModificationDate = aLastModifiedDate;
    mIsFile = !aName.IsVoid();
  }

  virtual bool IsMemoryFile() const MOZ_OVERRIDE
  {
    return false;
  }

  virtual bool IsDateUnknown() const MOZ_OVERRIDE
  {
    return mIsFile && mLastModificationDate == UINT64_MAX;
  }

  virtual bool IsFile() const
  {
    return mIsFile;
  }

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

  virtual bool IsSizeUnknown() const
  {
    return mLength == UINT64_MAX;
  }

  virtual nsresult Initialize(nsISupports* aOwner, JSContext* aCx,
                              JSObject* aObj, const JS::CallArgs& aArgs)
  {
    return NS_OK;
  }

  virtual void Unlink() {}
  virtual void Traverse(nsCycleCollectionTraversalCallback &aCb) {}

protected:
  virtual ~DOMFileImplBase() {}

  indexedDB::FileInfo* GetFileInfo() const
  {
    NS_ASSERTION(IsStoredFile(), "Should only be called on stored files!");
    NS_ASSERTION(!mFileInfos.IsEmpty(), "Must have at least one file info!");

    return mFileInfos.ElementAt(0);
  }

  bool mIsFile;
  bool mImmutable;

  nsString mContentType;
  nsString mName;
  nsString mPath; // The path relative to a directory chosen by the user

  uint64_t mStart;
  uint64_t mLength;

  uint64_t mLastModificationDate;

  // Protected by IndexedDatabaseManager::FileMutex()
  nsTArray<nsRefPtr<indexedDB::FileInfo>> mFileInfos;
};

/**
 * This class may be used off the main thread, and in particular, its
 * constructor and destructor may not run on the same thread.  Be careful!
 */
class DOMFileImplMemory MOZ_FINAL : public DOMFileImplBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  DOMFileImplMemory(void* aMemoryBuffer, uint64_t aLength,
                    const nsAString& aName,
                    const nsAString& aContentType,
                    uint64_t aLastModifiedDate)
    : DOMFileImplBase(aName, aContentType, aLength, aLastModifiedDate)
    , mDataOwner(new DataOwner(aMemoryBuffer, aLength))
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
  }

  DOMFileImplMemory(void* aMemoryBuffer,
                    uint64_t aLength,
                    const nsAString& aContentType)
    : DOMFileImplBase(aContentType, aLength)
    , mDataOwner(new DataOwner(aMemoryBuffer, aLength))
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
  }

  virtual nsresult GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType) MOZ_OVERRIDE;

  virtual bool IsMemoryFile() const MOZ_OVERRIDE
  {
    return true;
  }

  class DataOwner MOZ_FINAL : public mozilla::LinkedListElement<DataOwner> {
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

  private:
    // Private destructor, to discourage deletion outside of Release():
    ~DataOwner() {
      mozilla::StaticMutexAutoLock lock(sDataOwnerMutex);

      remove();
      if (sDataOwners->isEmpty()) {
        // Free the linked list if it's empty.
        sDataOwners = nullptr;
      }

      moz_free(mData);
    }

  public:
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

private:
  // Create slice
  DOMFileImplMemory(const DOMFileImplMemory* aOther, uint64_t aStart,
                    uint64_t aLength, const nsAString& aContentType)
    : DOMFileImplBase(aContentType, aOther->mStart + aStart, aLength)
    , mDataOwner(aOther->mDataOwner)
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
    mImmutable = aOther->mImmutable;
  }

  ~DOMFileImplMemory() {}

  // Used when backed by a memory store
  nsRefPtr<DataOwner> mDataOwner;
};

class DOMFileImplTemporaryFileBlob MOZ_FINAL : public DOMFileImplBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  DOMFileImplTemporaryFileBlob(PRFileDesc* aFD, uint64_t aStartPos,
                               uint64_t aLength, const nsAString& aContentType)
    : DOMFileImplBase(aContentType, aLength)
    , mLength(aLength)
    , mStartPos(aStartPos)
    , mContentType(aContentType)
  {
    mFileDescOwner = new nsTemporaryFileInputStream::FileDescOwner(aFD);
  }

  virtual nsresult GetInternalStream(nsIInputStream** aStream) MOZ_OVERRIDE;

  virtual already_AddRefed<nsIDOMBlob>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType) MOZ_OVERRIDE;

private:
  DOMFileImplTemporaryFileBlob(const DOMFileImplTemporaryFileBlob* aOther,
                               uint64_t aStart, uint64_t aLength,
                               const nsAString& aContentType)
    : DOMFileImplBase(aContentType, aLength)
    , mLength(aLength)
    , mStartPos(aStart)
    , mFileDescOwner(aOther->mFileDescOwner)
    , mContentType(aContentType) {}

  ~DOMFileImplTemporaryFileBlob() {}

  uint64_t mLength;
  uint64_t mStartPos;
  nsRefPtr<nsTemporaryFileInputStream::FileDescOwner> mFileDescOwner;
  nsString mContentType;
};

class DOMFileImplFile MOZ_FINAL : public DOMFileImplBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // Create as a file
  explicit DOMFileImplFile(nsIFile* aFile)
    : DOMFileImplBase(EmptyString(), EmptyString(), UINT64_MAX, UINT64_MAX)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
    mFile->GetLeafName(mName);
  }

  DOMFileImplFile(nsIFile* aFile, indexedDB::FileInfo* aFileInfo)
    : DOMFileImplBase(EmptyString(), EmptyString(), UINT64_MAX, UINT64_MAX)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(true)
  {
    NS_ASSERTION(mFile, "must have file");
    NS_ASSERTION(aFileInfo, "must have file info");
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
    mFile->GetLeafName(mName);

    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a file
  DOMFileImplFile(const nsAString& aName, const nsAString& aContentType,
                  uint64_t aLength, nsIFile* aFile)
    : DOMFileImplBase(aName, aContentType, aLength, UINT64_MAX)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
  }

  DOMFileImplFile(const nsAString& aName, const nsAString& aContentType,
                  uint64_t aLength, nsIFile* aFile,
                  uint64_t aLastModificationDate)
    : DOMFileImplBase(aName, aContentType, aLength, aLastModificationDate)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
  }

  // Create as a file with custom name
  DOMFileImplFile(nsIFile* aFile, const nsAString& aName,
                  const nsAString& aContentType)
    : DOMFileImplBase(aName, aContentType, UINT64_MAX, UINT64_MAX)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
    if (aContentType.IsEmpty()) {
      // Lazily get the content type and size
      mContentType.SetIsVoid(true);
    }
  }

  // Create as a stored file
  DOMFileImplFile(const nsAString& aName, const nsAString& aContentType,
                  uint64_t aLength, nsIFile* aFile,
                  indexedDB::FileInfo* aFileInfo)
    : DOMFileImplBase(aName, aContentType, aLength, UINT64_MAX)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(true)
  {
    NS_ASSERTION(mFile, "must have file");
    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a stored blob
  DOMFileImplFile(const nsAString& aContentType, uint64_t aLength,
                  nsIFile* aFile, indexedDB::FileInfo* aFileInfo)
    : DOMFileImplBase(aContentType, aLength)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(true)
  {
    NS_ASSERTION(mFile, "must have file");
    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a file to be later initialized
  DOMFileImplFile()
    : DOMFileImplBase(EmptyString(), EmptyString(), UINT64_MAX, UINT64_MAX)
    , mWholeFile(true)
    , mStoredFile(false)
  {
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
    mName.SetIsVoid(true);
  }

  // Overrides
  virtual nsresult GetSize(uint64_t* aSize) MOZ_OVERRIDE;
  virtual nsresult GetType(nsAString& aType) MOZ_OVERRIDE;
  virtual nsresult GetLastModifiedDate(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aLastModifiedDate) MOZ_OVERRIDE;
  virtual nsresult GetMozLastModifiedDate(uint64_t* aLastModifiedDate) MOZ_OVERRIDE;
  virtual nsresult GetMozFullPathInternal(nsAString& aFullPath) MOZ_OVERRIDE;
  virtual nsresult GetInternalStream(nsIInputStream**) MOZ_OVERRIDE;

  void SetPath(const nsAString& aFullPath);

private:
  // Create slice
  DOMFileImplFile(const DOMFileImplFile* aOther, uint64_t aStart,
                  uint64_t aLength, const nsAString& aContentType)
    : DOMFileImplBase(aContentType, aOther->mStart + aStart, aLength)
    , mFile(aOther->mFile)
    , mWholeFile(false)
    , mStoredFile(aOther->mStoredFile)
  {
    NS_ASSERTION(mFile, "must have file");
    mImmutable = aOther->mImmutable;

    if (mStoredFile) {
      indexedDB::FileInfo* fileInfo;

      using indexedDB::IndexedDatabaseManager;

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

  ~DOMFileImplFile() {}

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

} // dom namespace
} // file namespace

class nsDOMFileList MOZ_FINAL : public nsIDOMFileList,
                                public nsWrapperCache
{
  ~nsDOMFileList() {}

public:
  explicit nsDOMFileList(nsISupports *aParent) : mParent(aParent)
  {
    SetIsDOMBinding();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMFileList)

  NS_DECL_NSIDOMFILELIST

  virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;

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

#endif
