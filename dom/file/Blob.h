/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Blob_h
#define mozilla_dom_Blob_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BlobImpl.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsIMutable.h"
#include "nsWrapperCache.h"
#include "nsWeakReference.h"

class nsIInputStream;

namespace mozilla {
namespace dom {

struct BlobPropertyBag;
class File;
class OwningArrayBufferViewOrArrayBufferOrBlobOrUSVString;

#define NS_DOM_BLOB_IID \
{ 0x648c2a83, 0xbdb1, 0x4a7d, \
  { 0xb5, 0x0a, 0xca, 0xcd, 0x92, 0x87, 0x45, 0xc2 } }


class Blob : public nsIMutable
           , public nsSupportsWeakReference
           , public nsWrapperCache
{
public:
  NS_DECL_NSIMUTABLE

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Blob, nsIMutable)
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOM_BLOB_IID)

  typedef OwningArrayBufferViewOrArrayBufferOrBlobOrUSVString BlobPart;

  // This creates a Blob or a File based on the type of BlobImpl.
  static Blob*
  Create(nsISupports* aParent, BlobImpl* aImpl);

  static already_AddRefed<Blob>
  CreateStringBlob(nsISupports* aParent, const nsACString& aData,
                   const nsAString& aContentType);

  // The returned Blob takes ownership of aMemoryBuffer. aMemoryBuffer will be
  // freed by free so it must be allocated by malloc or something
  // compatible with it.
  static already_AddRefed<Blob>
  CreateMemoryBlob(nsISupports* aParent, void* aMemoryBuffer, uint64_t aLength,
                   const nsAString& aContentType);

  BlobImpl* Impl() const
  {
    return mImpl;
  }

  bool IsFile() const;

  const nsTArray<RefPtr<BlobImpl>>* GetSubBlobImpls() const;

  // This method returns null if this Blob is not a File; it returns
  // the same object in case this Blob already implements the File interface;
  // otherwise it returns a new File object with the same BlobImpl.
  already_AddRefed<File> ToFile();

  // This method creates a new File object with the given name and the same
  // BlobImpl.
  already_AddRefed<File> ToFile(const nsAString& aName,
                                ErrorResult& aRv) const;

  already_AddRefed<Blob>
  CreateSlice(uint64_t aStart, uint64_t aLength, const nsAString& aContentType,
              ErrorResult& aRv);

  void
  CreateInputStream(nsIInputStream** aStream, ErrorResult& aRv);

  int64_t
  GetFileId();

  // A utility function that enforces the spec constraints on the type of a
  // blob: no codepoints outside the ASCII range (otherwise type becomes empty)
  // and lowercase ASCII only.  We can't just use our existing nsContentUtils
  // ASCII-related helpers because we need the "outside ASCII range" check, and
  // we can't use NS_IsAscii because its definition of "ASCII" (chars all <=
  // 0x7E) differs from the file API definition (which excludes control chars).
  static void
  MakeValidBlobType(nsAString& aType);

  // WebIDL methods
  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  bool
  IsMemoryFile() const;

  // Blob constructor
  static already_AddRefed<Blob>
  Constructor(const GlobalObject& aGlobal,
              const Optional<Sequence<BlobPart>>& aData,
              const BlobPropertyBag& aBag,
              ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  uint64_t GetSize(ErrorResult& aRv);

  void GetType(nsAString& aType);

  already_AddRefed<Blob> Slice(const Optional<int64_t>& aStart,
                               const Optional<int64_t>& aEnd,
                               const Optional<nsAString>& aContentType,
                               ErrorResult& aRv);

  size_t GetAllocationSize() const;

  nsresult
  GetSendInfo(nsIInputStream** aBody,
              uint64_t* aContentLength,
              nsACString& aContentType,
              nsACString& aCharset) const;

protected:
  // File constructor should never be used directly. Use Blob::Create instead.
  Blob(nsISupports* aParent, BlobImpl* aImpl);
  virtual ~Blob();

  virtual bool HasFileInterface() const { return false; }

  // The member is the real backend implementation of this File/Blob.
  // It's thread-safe and not CC-able and it's the only element that is moved
  // between threads.
  // Note: we should not store any other state in this class!
  RefPtr<BlobImpl> mImpl;

private:
  nsCOMPtr<nsISupports> mParent;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Blob, NS_DOM_BLOB_IID)

// Override BindingJSObjectMallocBytes for blobs to tell the JS GC how much
// memory is held live by the binding object.
size_t BindingJSObjectMallocBytes(Blob* aBlob);

} // namespace dom
} // namespace mozilla

inline nsISupports*
ToSupports(mozilla::dom::Blob* aBlob)
{
  return static_cast<nsIMutable*>(aBlob);
}

#endif // mozilla_dom_Blob_h
