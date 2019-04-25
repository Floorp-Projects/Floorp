/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "File.h"
#include "FileBlobImpl.h"
#include "MemoryBlobImpl.h"
#include "MultipartBlobImpl.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/FileCreatorHelper.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/Promise.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace dom {

File::File(nsISupports* aParent, BlobImpl* aImpl) : Blob(aParent, aImpl) {
  MOZ_ASSERT(aImpl->IsFile());
}

File::~File() {}

/* static */
File* File::Create(nsISupports* aParent, BlobImpl* aImpl) {
  MOZ_ASSERT(aImpl);
  MOZ_ASSERT(aImpl->IsFile());

  return new File(aParent, aImpl);
}

/* static */
already_AddRefed<File> File::Create(nsISupports* aParent,
                                    const nsAString& aName,
                                    const nsAString& aContentType,
                                    uint64_t aLength,
                                    int64_t aLastModifiedDate) {
  RefPtr<File> file = new File(
      aParent, new BaseBlobImpl(NS_LITERAL_STRING("BaseBlobImpl"), aName,
                                aContentType, aLength, aLastModifiedDate));
  return file.forget();
}

/* static */
already_AddRefed<File> File::CreateMemoryFile(nsISupports* aParent,
                                              void* aMemoryBuffer,
                                              uint64_t aLength,
                                              const nsAString& aName,
                                              const nsAString& aContentType,
                                              int64_t aLastModifiedDate) {
  RefPtr<File> file =
      new File(aParent, new MemoryBlobImpl(aMemoryBuffer, aLength, aName,
                                           aContentType, aLastModifiedDate));
  return file.forget();
}

/* static */
already_AddRefed<File> File::CreateFromFile(nsISupports* aParent,
                                            nsIFile* aFile) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<File> file = new File(aParent, new FileBlobImpl(aFile));
  return file.forget();
}

/* static */
already_AddRefed<File> File::CreateFromFile(nsISupports* aParent,
                                            nsIFile* aFile,
                                            const nsAString& aName,
                                            const nsAString& aContentType) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<File> file =
      new File(aParent, new FileBlobImpl(aFile, aName, aContentType));
  return file.forget();
}

JSObject* File::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return File_Binding::Wrap(aCx, this, aGivenProto);
}

void File::GetName(nsAString& aFileName) const { mImpl->GetName(aFileName); }

void File::GetRelativePath(nsAString& aPath) const {
  aPath.Truncate();

  nsAutoString path;
  mImpl->GetDOMPath(path);

  // WebkitRelativePath doesn't start with '/'
  if (!path.IsEmpty()) {
    MOZ_ASSERT(path[0] == FILESYSTEM_DOM_PATH_SEPARATOR_CHAR);
    aPath.Assign(Substring(path, 1));
  }
}

int64_t File::GetLastModified(ErrorResult& aRv) {
  return mImpl->GetLastModified(aRv);
}

void File::GetMozFullPath(nsAString& aFilename,
                          SystemCallerGuarantee aGuarantee, ErrorResult& aRv) {
  mImpl->GetMozFullPath(aFilename, aGuarantee, aRv);
}

void File::GetMozFullPathInternal(nsAString& aFileName, ErrorResult& aRv) {
  mImpl->GetMozFullPathInternal(aFileName, aRv);
}

/* static */
already_AddRefed<File> File::Constructor(const GlobalObject& aGlobal,
                                         const Sequence<BlobPart>& aData,
                                         const nsAString& aName,
                                         const FilePropertyBag& aBag,
                                         ErrorResult& aRv) {
  // Normalizing the filename
  nsString name(aName);
  name.ReplaceChar('/', ':');

  RefPtr<MultipartBlobImpl> impl = new MultipartBlobImpl(name);

  nsAutoString type(aBag.mType);
  MakeValidBlobType(type);
  impl->InitializeBlob(aData, type, aBag.mEndings == EndingTypes::Native, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(impl->IsFile());

  if (aBag.mLastModified.WasPassed()) {
    impl->SetLastModified(aBag.mLastModified.Value());
  }

  RefPtr<File> file = new File(aGlobal.GetAsSupports(), impl);
  return file.forget();
}

/* static */
already_AddRefed<Promise> File::CreateFromNsIFile(
    const GlobalObject& aGlobal, nsIFile* aData,
    const ChromeFilePropertyBag& aBag, SystemCallerGuarantee aGuarantee,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  RefPtr<Promise> promise =
      FileCreatorHelper::CreateFile(global, aData, aBag, true, aRv);
  return promise.forget();
}

/* static */
already_AddRefed<Promise> File::CreateFromFileName(
    const GlobalObject& aGlobal, const nsAString& aPath,
    const ChromeFilePropertyBag& aBag, SystemCallerGuarantee aGuarantee,
    ErrorResult& aRv) {
  nsCOMPtr<nsIFile> file;
  aRv = NS_NewLocalFile(aPath, false, getter_AddRefs(file));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  RefPtr<Promise> promise =
      FileCreatorHelper::CreateFile(global, file, aBag, false, aRv);
  return promise.forget();
}

}  // namespace dom
}  // namespace mozilla
