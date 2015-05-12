/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_File_h
#define mozilla_dom_File_h

#include "mozilla/Attributes.h"

#include "mozilla/GuardObjects.h"
#include "mozilla/LinkedList.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/Date.h"
#include "mozilla/dom/indexedDB/FileInfo.h"
#include "mozilla/dom/indexedDB/FileManager.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsIDOMFile.h"
#include "nsIDOMFileList.h"
#include "nsIFile.h"
#include "nsIMutable.h"
#include "nsIXMLHttpRequest.h"
#include "nsString.h"
#include "nsTemporaryFileInputStream.h"
#include "nsWrapperCache.h"
#include "nsWeakReference.h"

class nsIFile;
class nsIInputStream;

#define FILEIMPL_IID \
  { 0xbccb3275, 0x6778, 0x4ac5, \
    { 0xaf, 0x03, 0x90, 0xed, 0x37, 0xad, 0xdf, 0x5d } }

namespace mozilla {
namespace dom {

namespace indexedDB {
class FileInfo;
};

struct BlobPropertyBag;
struct ChromeFilePropertyBag;
struct FilePropertyBag;
class BlobImpl;
class File;
class OwningArrayBufferOrArrayBufferViewOrBlobOrString;

class Blob : public nsIDOMBlob
           , public nsIXHRSendable
           , public nsIMutable
           , public nsSupportsWeakReference
           , public nsWrapperCache
{
public:
  NS_DECL_NSIDOMBLOB
  NS_DECL_NSIXHRSENDABLE
  NS_DECL_NSIMUTABLE

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Blob, nsIDOMBlob)

  static Blob*
  Create(nsISupports* aParent, BlobImpl* aImpl);

  static already_AddRefed<Blob>
  Create(nsISupports* aParent, const nsAString& aContentType,
         uint64_t aLength);

  static already_AddRefed<Blob>
  Create(nsISupports* aParent, const nsAString& aContentType, uint64_t aStart,
         uint64_t aLength);

  // The returned Blob takes ownership of aMemoryBuffer. aMemoryBuffer will be
  // freed by free so it must be allocated by malloc or something
  // compatible with it.
  static already_AddRefed<Blob>
  CreateMemoryBlob(nsISupports* aParent, void* aMemoryBuffer, uint64_t aLength,
                   const nsAString& aContentType);

  static already_AddRefed<Blob>
  CreateTemporaryBlob(nsISupports* aParent, PRFileDesc* aFD,
                      uint64_t aStartPos, uint64_t aLength,
                      const nsAString& aContentType);

  BlobImpl* Impl() const
  {
    return mImpl;
  }

  bool IsFile() const;

  const nsTArray<nsRefPtr<BlobImpl>>* GetSubBlobImpls() const;

  // This method returns null if this Blob is not a File; it returns
  // the same object in case this Blob already implements the File interface;
  // otherwise it returns a new File object with the same BlobImpl.
  already_AddRefed<File> ToFile();

  // This method creates a new File object with the given name and the same
  // BlobImpl.
  already_AddRefed<File> ToFile(const nsAString& aName) const;

  already_AddRefed<Blob>
  CreateSlice(uint64_t aStart, uint64_t aLength, const nsAString& aContentType,
              ErrorResult& aRv);

  // WebIDL methods
  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  // Blob constructor
  static already_AddRefed<Blob>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  // Blob constructor
  static already_AddRefed<Blob>
  Constructor(const GlobalObject& aGlobal,
              const Sequence<OwningArrayBufferOrArrayBufferViewOrBlobOrString>& aData,
              const BlobPropertyBag& aBag,
              ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  uint64_t GetSize(ErrorResult& aRv);

  // XPCOM GetType is OK

  already_AddRefed<Blob> Slice(const Optional<int64_t>& aStart,
                               const Optional<int64_t>& aEnd,
                               const nsAString& aContentType,
                               ErrorResult& aRv);

protected:
  // File constructor should never be used directly. Use Blob::Create instead.
  Blob(nsISupports* aParent, BlobImpl* aImpl);
  virtual ~Blob() {};

  virtual bool HasFileInterface() const { return false; }

  // The member is the real backend implementation of this File/Blob.
  // It's thread-safe and not CC-able and it's the only element that is moved
  // between threads.
  // Note: we should not store any other state in this class!
  nsRefPtr<BlobImpl> mImpl;

private:
  nsCOMPtr<nsISupports> mParent;
};

class File final : public Blob
                 , public nsIDOMFile
{
  friend class Blob;

public:
  NS_DECL_NSIDOMFILE
  NS_FORWARD_NSIDOMBLOB(Blob::)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(File, Blob);

  // Note: BlobImpl must be a File in order to use this method.
  // Check impl->IsFile().
  static File*
  Create(nsISupports* aParent, BlobImpl* aImpl);

  static already_AddRefed<File>
  Create(nsISupports* aParent, const nsAString& aName,
         const nsAString& aContentType, uint64_t aLength,
         int64_t aLastModifiedDate);

  static already_AddRefed<File>
  Create(nsISupports* aParent, const nsAString& aName,
         const nsAString& aContentType, uint64_t aLength);

  // The returned File takes ownership of aMemoryBuffer. aMemoryBuffer will be
  // freed by free so it must be allocated by malloc or something
  // compatible with it.
  static already_AddRefed<File>
  CreateMemoryFile(nsISupports* aParent, void* aMemoryBuffer, uint64_t aLength,
                   const nsAString& aName, const nsAString& aContentType,
                   int64_t aLastModifiedDate);

  static already_AddRefed<File>
  CreateFromFile(nsISupports* aParent, nsIFile* aFile, bool aTemporary = false);

  static already_AddRefed<File>
  CreateFromFile(nsISupports* aParent, const nsAString& aContentType,
                 uint64_t aLength, nsIFile* aFile,
                 indexedDB::FileInfo* aFileInfo);

  static already_AddRefed<File>
  CreateFromFile(nsISupports* aParent, const nsAString& aName,
                 const nsAString& aContentType, uint64_t aLength,
                 nsIFile* aFile, indexedDB::FileInfo* aFileInfo);

  static already_AddRefed<File>
  CreateFromFile(nsISupports* aParent, nsIFile* aFile,
                 indexedDB::FileInfo* aFileInfo);

  static already_AddRefed<File>
  CreateFromFile(nsISupports* aParent, nsIFile* aFile, const nsAString& aName,
                 const nsAString& aContentType);

  // WebIDL methods

  virtual JSObject* WrapObject(JSContext *cx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // File constructor
  static already_AddRefed<File>
  Constructor(const GlobalObject& aGlobal,
              const Sequence<OwningArrayBufferOrArrayBufferViewOrBlobOrString>& aData,
              const nsAString& aName,
              const FilePropertyBag& aBag,
              ErrorResult& aRv);

  // File constructor - ChromeOnly
  static already_AddRefed<File>
  Constructor(const GlobalObject& aGlobal,
              Blob& aData,
              const ChromeFilePropertyBag& aBag,
              ErrorResult& aRv);

  // File constructor - ChromeOnly
  static already_AddRefed<File>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aData,
              const ChromeFilePropertyBag& aBag,
              ErrorResult& aRv);

  // File constructor - ChromeOnly
  static already_AddRefed<File>
  Constructor(const GlobalObject& aGlobal,
              nsIFile* aData,
              const ChromeFilePropertyBag& aBag,
              ErrorResult& aRv);

  // XPCOM GetName is OK

  int64_t GetLastModified(ErrorResult& aRv);

  Date GetLastModifiedDate(ErrorResult& aRv);


  void GetMozFullPath(nsAString& aFilename, ErrorResult& aRv);

protected:
  virtual bool HasFileInterface() const override { return true; }

private:
  // File constructor should never be used directly. Use Blob::Create or
  // File::Create.
  File(nsISupports* aParent, BlobImpl* aImpl);
  ~File() {};
};

// This is the abstract class for any File backend. It must be nsISupports
// because this class must be ref-counted and it has to work with IPC.
class BlobImpl : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(FILEIMPL_IID)
  NS_DECL_THREADSAFE_ISUPPORTS

  BlobImpl() {}

  virtual void GetName(nsAString& aName) = 0;

  virtual nsresult GetPath(nsAString& aName) = 0;

  virtual int64_t GetLastModified(ErrorResult& aRv) = 0;

  virtual void SetLastModified(int64_t aLastModified) = 0;

  virtual void GetMozFullPath(nsAString& aName, ErrorResult& aRv) = 0;

  virtual void GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv) = 0;

  virtual uint64_t GetSize(ErrorResult& aRv) = 0;

  virtual void GetType(nsAString& aType) = 0;

  already_AddRefed<BlobImpl>
  Slice(const Optional<int64_t>& aStart, const Optional<int64_t>& aEnd,
        const nsAString& aContentType, ErrorResult& aRv);

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType, ErrorResult& aRv) = 0;

  virtual const nsTArray<nsRefPtr<BlobImpl>>*
  GetSubBlobImpls() const = 0;

  virtual nsresult GetInternalStream(nsIInputStream** aStream) = 0;

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
                           int64_t aLastModifiedDate) = 0;

  virtual bool IsMemoryFile() const = 0;

  virtual bool IsSizeUnknown() const = 0;

  virtual bool IsDateUnknown() const = 0;

  virtual bool IsFile() const = 0;

  // True if this implementation can be sent to other threads.
  virtual bool MayBeClonedToOtherThreads() const
  {
    return true;
  }

protected:
  virtual ~BlobImpl() {}
};

NS_DEFINE_STATIC_IID_ACCESSOR(BlobImpl, FILEIMPL_IID)

class BlobImplBase : public BlobImpl
{
public:
  BlobImplBase(const nsAString& aName, const nsAString& aContentType,
               uint64_t aLength, int64_t aLastModifiedDate)
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

  BlobImplBase(const nsAString& aName, const nsAString& aContentType,
               uint64_t aLength)
    : mIsFile(true)
    , mImmutable(false)
    , mContentType(aContentType)
    , mName(aName)
    , mStart(0)
    , mLength(aLength)
    , mLastModificationDate(INT64_MAX)
  {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  BlobImplBase(const nsAString& aContentType, uint64_t aLength)
    : mIsFile(false)
    , mImmutable(false)
    , mContentType(aContentType)
    , mStart(0)
    , mLength(aLength)
    , mLastModificationDate(INT64_MAX)
  {
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  BlobImplBase(const nsAString& aContentType, uint64_t aStart,
               uint64_t aLength)
    : mIsFile(false)
    , mImmutable(false)
    , mContentType(aContentType)
    , mStart(aStart)
    , mLength(aLength)
    , mLastModificationDate(INT64_MAX)
  {
    NS_ASSERTION(aLength != UINT64_MAX,
                 "Must know length when creating slice");
    // Ensure non-null mContentType by default
    mContentType.SetIsVoid(false);
  }

  virtual void GetName(nsAString& aName) override;

  virtual nsresult GetPath(nsAString& aName) override;

  virtual int64_t GetLastModified(ErrorResult& aRv) override;

  virtual void SetLastModified(int64_t aLastModified) override;

  virtual void GetMozFullPath(nsAString& aName, ErrorResult& aRv) override;

  virtual void GetMozFullPathInternal(nsAString& aFileName,
                                      ErrorResult& aRv) override;

  virtual uint64_t GetSize(ErrorResult& aRv) override
  {
    return mLength;
  }

  virtual void GetType(nsAString& aType) override;

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType, ErrorResult& aRv) override
  {
    return nullptr;
  }

  virtual const nsTArray<nsRefPtr<BlobImpl>>*
  GetSubBlobImpls() const override
  {
    return nullptr;
  }

  virtual nsresult GetInternalStream(nsIInputStream** aStream) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  virtual int64_t GetFileId() override;

  virtual void AddFileInfo(indexedDB::FileInfo* aFileInfo) override;

  virtual indexedDB::FileInfo*
  GetFileInfo(indexedDB::FileManager* aFileManager) override;

  virtual nsresult GetSendInfo(nsIInputStream** aBody,
                               uint64_t* aContentLength,
                               nsACString& aContentType,
                               nsACString& aCharset) override;

  virtual nsresult GetMutable(bool* aMutable) const override;

  virtual nsresult SetMutable(bool aMutable) override;

  virtual void
  SetLazyData(const nsAString& aName, const nsAString& aContentType,
              uint64_t aLength, int64_t aLastModifiedDate) override
  {
    NS_ASSERTION(aLength, "must have length");

    mName = aName;
    mContentType = aContentType;
    mLength = aLength;
    mLastModificationDate = aLastModifiedDate;
    mIsFile = !aName.IsVoid();
  }

  virtual bool IsMemoryFile() const override
  {
    return false;
  }

  virtual bool IsDateUnknown() const override
  {
    return mIsFile && mLastModificationDate == INT64_MAX;
  }

  virtual bool IsFile() const override
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

  virtual bool IsSizeUnknown() const override
  {
    return mLength == UINT64_MAX;
  }

protected:
  virtual ~BlobImplBase() {}

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

  int64_t mLastModificationDate;

  // Protected by IndexedDatabaseManager::FileMutex()
  nsTArray<nsRefPtr<indexedDB::FileInfo>> mFileInfos;
};

/**
 * This class may be used off the main thread, and in particular, its
 * constructor and destructor may not run on the same thread.  Be careful!
 */
class BlobImplMemory final : public BlobImplBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  BlobImplMemory(void* aMemoryBuffer, uint64_t aLength, const nsAString& aName,
                 const nsAString& aContentType, int64_t aLastModifiedDate)
    : BlobImplBase(aName, aContentType, aLength, aLastModifiedDate)
    , mDataOwner(new DataOwner(aMemoryBuffer, aLength))
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
  }

  BlobImplMemory(void* aMemoryBuffer, uint64_t aLength,
                 const nsAString& aContentType)
    : BlobImplBase(aContentType, aLength)
    , mDataOwner(new DataOwner(aMemoryBuffer, aLength))
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
  }

  virtual nsresult GetInternalStream(nsIInputStream** aStream) override;

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType, ErrorResult& aRv) override;

  virtual bool IsMemoryFile() const override
  {
    return true;
  }

  class DataOwner final : public mozilla::LinkedListElement<DataOwner>
  {
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

      free(mData);
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
  BlobImplMemory(const BlobImplMemory* aOther, uint64_t aStart,
                 uint64_t aLength, const nsAString& aContentType)
    : BlobImplBase(aContentType, aOther->mStart + aStart, aLength)
    , mDataOwner(aOther->mDataOwner)
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");
    mImmutable = aOther->mImmutable;
  }

  ~BlobImplMemory() {}

  // Used when backed by a memory store
  nsRefPtr<DataOwner> mDataOwner;
};

class BlobImplTemporaryBlob final : public BlobImplBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  BlobImplTemporaryBlob(PRFileDesc* aFD, uint64_t aStartPos,
                        uint64_t aLength, const nsAString& aContentType)
    : BlobImplBase(aContentType, aLength)
    , mStartPos(aStartPos)
  {
    mFileDescOwner = new nsTemporaryFileInputStream::FileDescOwner(aFD);
  }

  virtual nsresult GetInternalStream(nsIInputStream** aStream) override;

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType, ErrorResult& aRv) override;

private:
  BlobImplTemporaryBlob(const BlobImplTemporaryBlob* aOther,
                        uint64_t aStart, uint64_t aLength,
                        const nsAString& aContentType)
    : BlobImplBase(aContentType, aLength)
    , mStartPos(aStart)
    , mFileDescOwner(aOther->mFileDescOwner)
  {}

  ~BlobImplTemporaryBlob() {}

  uint64_t mStartPos;
  nsRefPtr<nsTemporaryFileInputStream::FileDescOwner> mFileDescOwner;
};

class BlobImplFile : public BlobImplBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // Create as a file
  explicit BlobImplFile(nsIFile* aFile, bool aTemporary = false)
    : BlobImplBase(EmptyString(), EmptyString(), UINT64_MAX, INT64_MAX)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(false)
    , mIsTemporary(aTemporary)
  {
    NS_ASSERTION(mFile, "must have file");
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
    mFile->GetLeafName(mName);
  }

  BlobImplFile(nsIFile* aFile, indexedDB::FileInfo* aFileInfo)
    : BlobImplBase(EmptyString(), EmptyString(), UINT64_MAX, INT64_MAX)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(true)
    , mIsTemporary(false)
  {
    NS_ASSERTION(mFile, "must have file");
    NS_ASSERTION(aFileInfo, "must have file info");
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
    mFile->GetLeafName(mName);

    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a file
  BlobImplFile(const nsAString& aName, const nsAString& aContentType,
               uint64_t aLength, nsIFile* aFile)
    : BlobImplBase(aName, aContentType, aLength, UINT64_MAX)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(false)
    , mIsTemporary(false)
  {
    NS_ASSERTION(mFile, "must have file");
  }

  BlobImplFile(const nsAString& aName, const nsAString& aContentType,
               uint64_t aLength, nsIFile* aFile,
               int64_t aLastModificationDate)
    : BlobImplBase(aName, aContentType, aLength, aLastModificationDate)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(false)
    , mIsTemporary(false)
  {
    NS_ASSERTION(mFile, "must have file");
  }

  // Create as a file with custom name
  BlobImplFile(nsIFile* aFile, const nsAString& aName,
               const nsAString& aContentType)
    : BlobImplBase(aName, aContentType, UINT64_MAX, INT64_MAX)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(false)
    , mIsTemporary(false)
  {
    NS_ASSERTION(mFile, "must have file");
    if (aContentType.IsEmpty()) {
      // Lazily get the content type and size
      mContentType.SetIsVoid(true);
    }
  }

  // Create as a stored file
  BlobImplFile(const nsAString& aName, const nsAString& aContentType,
               uint64_t aLength, nsIFile* aFile,
               indexedDB::FileInfo* aFileInfo)
    : BlobImplBase(aName, aContentType, aLength, UINT64_MAX)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(true)
    , mIsTemporary(false)
  {
    NS_ASSERTION(mFile, "must have file");
    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a stored blob
  BlobImplFile(const nsAString& aContentType, uint64_t aLength,
               nsIFile* aFile, indexedDB::FileInfo* aFileInfo)
    : BlobImplBase(aContentType, aLength)
    , mFile(aFile)
    , mWholeFile(true)
    , mStoredFile(true)
    , mIsTemporary(false)
  {
    NS_ASSERTION(mFile, "must have file");
    mFileInfos.AppendElement(aFileInfo);
  }

  // Create as a file to be later initialized
  BlobImplFile()
    : BlobImplBase(EmptyString(), EmptyString(), UINT64_MAX, INT64_MAX)
    , mWholeFile(true)
    , mStoredFile(false)
    , mIsTemporary(false)
  {
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
    mName.SetIsVoid(true);
  }

  // Overrides
  virtual uint64_t GetSize(ErrorResult& aRv) override;
  virtual void GetType(nsAString& aType) override;
  virtual int64_t GetLastModified(ErrorResult& aRv) override;
  virtual void SetLastModified(int64_t aLastModified) override;
  virtual void GetMozFullPathInternal(nsAString& aFullPath,
                                      ErrorResult& aRv) override;
  virtual nsresult GetInternalStream(nsIInputStream**) override;

  void SetPath(const nsAString& aFullPath);

protected:
  virtual ~BlobImplFile() {
    if (mFile && mIsTemporary) {
      NS_WARNING("In temporary ~BlobImplFile");
      // Ignore errors if any, not much we can do. Clean-up will be done by
      // https://mxr.mozilla.org/mozilla-central/source/xpcom/io/nsAnonymousTemporaryFile.cpp?rev=6c1c7e45c902#127
#ifdef DEBUG
      nsresult rv =
#endif
      mFile->Remove(false);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to remove temporary DOMFile.");
    }
  }

private:
  // Create slice
  BlobImplFile(const BlobImplFile* aOther, uint64_t aStart,
               uint64_t aLength, const nsAString& aContentType)
    : BlobImplBase(aContentType, aOther->mStart + aStart, aLength)
    , mFile(aOther->mFile)
    , mWholeFile(false)
    , mStoredFile(aOther->mStoredFile)
    , mIsTemporary(false)
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

  virtual already_AddRefed<BlobImpl>
  CreateSlice(uint64_t aStart, uint64_t aLength,
              const nsAString& aContentType, ErrorResult& aRv) override;

  virtual bool IsStoredFile() const override
  {
    return mStoredFile;
  }

  virtual bool IsWholeFile() const override
  {
    return mWholeFile;
  }

  nsCOMPtr<nsIFile> mFile;
  bool mWholeFile;
  bool mStoredFile;
  bool mIsTemporary;
};

class FileList final : public nsIDOMFileList,
                       public nsWrapperCache
{
  ~FileList() {}

public:
  explicit FileList(nsISupports *aParent) : mParent(aParent)
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FileList)

  NS_DECL_NSIDOMFILELIST

  virtual JSObject* WrapObject(JSContext *cx,
                               JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject()
  {
    return mParent;
  }

  void Disconnect()
  {
    mParent = nullptr;
  }

  bool Append(File *aFile) { return mFiles.AppendElement(aFile); }

  bool Remove(uint32_t aIndex) {
    if (aIndex < mFiles.Length()) {
      mFiles.RemoveElementAt(aIndex);
      return true;
    }

    return false;
  }

  void Clear() { return mFiles.Clear(); }

  static FileList* FromSupports(nsISupports* aSupports)
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

    return static_cast<FileList*>(aSupports);
  }

  File* Item(uint32_t aIndex)
  {
    return mFiles.SafeElementAt(aIndex);
  }
  File* IndexedGetter(uint32_t aIndex, bool& aFound)
  {
    aFound = aIndex < mFiles.Length();
    return aFound ? mFiles.ElementAt(aIndex) : nullptr;
  }
  uint32_t Length()
  {
    return mFiles.Length();
  }

private:
  nsTArray<nsRefPtr<File>> mFiles;
  nsISupports *mParent;
};

} // dom namespace
} // file namespace

#endif // mozilla_dom_File_h
