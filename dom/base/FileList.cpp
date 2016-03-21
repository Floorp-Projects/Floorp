/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Directory.h"
#include "mozilla/dom/FileList.h"
#include "mozilla/dom/FileListBinding.h"
#include "mozilla/dom/File.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileList, mFilesOrDirectories, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFileList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFileList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileList)

JSObject*
FileList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::FileListBinding::Wrap(aCx, this, aGivenProto);
}

void
FileList::Append(File* aFile)
{
  OwningFileOrDirectory* element = mFilesOrDirectories.AppendElement();
  element->SetAsFile() = aFile;
}

void
FileList::Append(Directory* aDirectory)
{
  OwningFileOrDirectory* element = mFilesOrDirectories.AppendElement();
  element->SetAsDirectory() = aDirectory;
}

NS_IMETHODIMP
FileList::GetLength(uint32_t* aLength)
{
  *aLength = Length();

  return NS_OK;
}

NS_IMETHODIMP
FileList::Item(uint32_t aIndex, nsISupports** aValue)
{
  if (aIndex >= mFilesOrDirectories.Length()) {
    return NS_ERROR_FAILURE;
  }

  if (mFilesOrDirectories[aIndex].IsFile()) {
    nsCOMPtr<nsIDOMBlob> file = mFilesOrDirectories[aIndex].GetAsFile();
    file.forget(aValue);
    return NS_OK;
  }

  MOZ_ASSERT(mFilesOrDirectories[aIndex].IsDirectory());
  RefPtr<Directory> directory = mFilesOrDirectories[aIndex].GetAsDirectory();
  directory.forget(aValue);
  return NS_OK;
}

void
FileList::Item(uint32_t aIndex, Nullable<OwningFileOrDirectory>& aValue,
               ErrorResult& aRv) const
{
  if (aIndex >= mFilesOrDirectories.Length()) {
    aValue.SetNull();
    return;
  }

  aValue.SetValue(mFilesOrDirectories[aIndex]);
}

void
FileList::IndexedGetter(uint32_t aIndex, bool& aFound,
                        Nullable<OwningFileOrDirectory>& aFileOrDirectory,
                        ErrorResult& aRv) const
{
  aFound = aIndex < mFilesOrDirectories.Length();
  Item(aIndex, aFileOrDirectory, aRv);
}

void
FileList::ToSequence(Sequence<OwningFileOrDirectory>& aSequence,
                     ErrorResult& aRv) const
{
  MOZ_ASSERT(aSequence.IsEmpty());
  if (mFilesOrDirectories.IsEmpty()) {
    return;
  }

  if (!aSequence.SetLength(mFilesOrDirectories.Length(),
                           mozilla::fallible_t())) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  for (uint32_t i = 0; i < mFilesOrDirectories.Length(); ++i) {
    aSequence[i] = mFilesOrDirectories[i];
  }
}

bool
FileList::ClonableToDifferentThreadOrProcess() const
{
  for (uint32_t i = 0; i < mFilesOrDirectories.Length(); ++i) {
    if (mFilesOrDirectories[i].IsDirectory()) {
      return false;
    }
  }

  return true;
}

} // namespace dom
} // namespace mozilla
