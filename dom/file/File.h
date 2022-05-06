/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_File_h
#define mozilla_dom_File_h

#include "mozilla/dom/Blob.h"

class nsIFile;

namespace mozilla::dom {

struct ChromeFilePropertyBag;
struct FilePropertyBag;
class Promise;

class File final : public Blob {
  friend class Blob;

 public:
  // Note: BlobImpl must be a File in order to use this method.
  // Check impl->IsFile().
  static File* Create(nsIGlobalObject* aGlobal, BlobImpl* aImpl);

  // The returned File takes ownership of aMemoryBuffer. aMemoryBuffer will be
  // freed by free so it must be allocated by malloc or something
  // compatible with it.
  static already_AddRefed<File> CreateMemoryFileWithLastModifiedNow(
      nsIGlobalObject* aGlobal, void* aMemoryBuffer, uint64_t aLength,
      const nsAString& aName, const nsAString& aContentType);

  // You should not use this method! Please consider to use the
  // CreateMemoryFileWithLastModifiedNow.
  static already_AddRefed<File> CreateMemoryFileWithCustomLastModified(
      nsIGlobalObject* aGlobal, void* aMemoryBuffer, uint64_t aLength,
      const nsAString& aName, const nsAString& aContentType,
      int64_t aLastModifiedDate);

  // This method creates a BlobFileImpl for the new File object. This is
  // thread-safe, cross-process, cross-thread as any other BlobImpl, but, when
  // GetType() is called, it must dispatch a runnable to the main-thread in
  // order to use nsIMIMEService.
  // Would be nice if we try to avoid to use this method outside the
  // main-thread to avoid extra runnables.
  static already_AddRefed<File> CreateFromFile(nsIGlobalObject* aGlobal,
                                               nsIFile* aFile);

  static already_AddRefed<File> CreateFromFile(nsIGlobalObject* aGlobal,
                                               nsIFile* aFile,
                                               const nsAString& aName,
                                               const nsAString& aContentType);

  // WebIDL methods

  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // File constructor
  static already_AddRefed<File> Constructor(const GlobalObject& aGlobal,
                                            const Sequence<BlobPart>& aData,
                                            const nsAString& aName,
                                            const FilePropertyBag& aBag,
                                            ErrorResult& aRv);

  // ChromeOnly
  static already_AddRefed<Promise> CreateFromFileName(
      const GlobalObject& aGlobal, const nsAString& aPath,
      const ChromeFilePropertyBag& aBag, SystemCallerGuarantee aGuarantee,
      ErrorResult& aRv);

  // ChromeOnly
  static already_AddRefed<Promise> CreateFromNsIFile(
      const GlobalObject& aGlobal, nsIFile* aData,
      const ChromeFilePropertyBag& aBag, SystemCallerGuarantee aGuarantee,
      ErrorResult& aRv);

  void GetName(nsAString& aFileName) const;

  int64_t GetLastModified(ErrorResult& aRv);

  void GetRelativePath(nsAString& aPath) const;

  void GetMozFullPath(nsAString& aFilename, SystemCallerGuarantee aGuarantee,
                      ErrorResult& aRv);

  void GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv);

 protected:
  bool HasFileInterface() const override { return true; }

 private:
  // File constructor should never be used directly. Use Blob::Create or
  // File::Create.
  File(nsIGlobalObject* aGlobal, BlobImpl* aImpl);
  ~File() override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_File_h
