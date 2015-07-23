/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileList.h"
#include "mozilla/dom/FileListBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileList, mFiles, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFileList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFileList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileList)

/* static */ already_AddRefed<FileList>
FileList::Create(nsISupports* aParent, FileListClonedData* aData)
{
  MOZ_ASSERT(aData);

  nsRefPtr<FileList> fileList = new FileList(aParent);

  const nsTArray<nsRefPtr<BlobImpl>>& blobImpls = aData->BlobImpls();
  for (uint32_t i = 0; i < blobImpls.Length(); ++i) {
    const nsRefPtr<BlobImpl>& blobImpl = blobImpls[i];
    MOZ_ASSERT(blobImpl);
    MOZ_ASSERT(blobImpl->IsFile());

    nsRefPtr<File> file = File::Create(aParent, blobImpl);
    MOZ_ASSERT(file);

    if (NS_WARN_IF(!fileList->Append(file))) {
      return nullptr;
    }
  }

  return fileList.forget();
}

JSObject*
FileList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::FileListBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
FileList::GetLength(uint32_t* aLength)
{
  *aLength = Length();

  return NS_OK;
}

NS_IMETHODIMP
FileList::Item(uint32_t aIndex, nsISupports** aFile)
{
  nsCOMPtr<nsIDOMBlob> file = Item(aIndex);
  file.forget(aFile);
  return NS_OK;
}

already_AddRefed<FileListClonedData>
FileList::CreateClonedData() const
{
  nsTArray<nsRefPtr<BlobImpl>> blobImpls;
  for (uint32_t i = 0; i < mFiles.Length(); ++i) {
    blobImpls.AppendElement(mFiles[i]->Impl());
  }

  nsRefPtr<FileListClonedData> data = new FileListClonedData(blobImpls);
  return data.forget();
}

NS_IMPL_ISUPPORTS0(FileListClonedData)

} // namespace dom
} // namespace mozilla
