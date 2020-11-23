/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileList.h"

#include <new>
#include <utility>
#include "ErrorList.h"
#include "js/RootingAPI.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileListBinding.h"
#include "mozilla/fallible.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollectionTraversalCallback.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileList, mFiles, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileList)

FileList::FileList(nsISupports* aParent) : mParent(aParent) {}

FileList::~FileList() = default;

JSObject* FileList::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::FileList_Binding::Wrap(aCx, this, aGivenProto);
}

bool FileList::Append(File* aFile) {
  MOZ_ASSERT(aFile);
  return mFiles.AppendElement(aFile, fallible);
}

bool FileList::Remove(uint32_t aIndex) {
  if (aIndex < mFiles.Length()) {
    mFiles.RemoveElementAt(aIndex);
    return true;
  }

  return false;
}

void FileList::Clear() { return mFiles.Clear(); }

File* FileList::Item(uint32_t aIndex) const {
  if (aIndex >= mFiles.Length()) {
    return nullptr;
  }

  return mFiles[aIndex];
}

File* FileList::IndexedGetter(uint32_t aIndex, bool& aFound) const {
  aFound = aIndex < mFiles.Length();
  return Item(aIndex);
}

void FileList::ToSequence(Sequence<RefPtr<File>>& aSequence,
                          ErrorResult& aRv) const {
  MOZ_ASSERT(aSequence.IsEmpty());
  if (mFiles.IsEmpty()) {
    return;
  }

  if (!aSequence.SetLength(mFiles.Length(), mozilla::fallible_t())) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  for (uint32_t i = 0; i < mFiles.Length(); ++i) {
    aSequence[i] = mFiles[i];
  }
}

}  // namespace mozilla::dom
