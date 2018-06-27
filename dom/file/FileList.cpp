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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileList, mFiles, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileList)

JSObject*
FileList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::FileList_Binding::Wrap(aCx, this, aGivenProto);
}

File*
FileList::Item(uint32_t aIndex) const
{
  if (aIndex >= mFiles.Length()) {
    return nullptr;
  }

  return mFiles[aIndex];
}

File*
FileList::IndexedGetter(uint32_t aIndex, bool& aFound) const
{
  aFound = aIndex < mFiles.Length();
  return Item(aIndex);
}

void
FileList::ToSequence(Sequence<RefPtr<File>>& aSequence,
                     ErrorResult& aRv) const
{
  MOZ_ASSERT(aSequence.IsEmpty());
  if (mFiles.IsEmpty()) {
    return;
  }

  if (!aSequence.SetLength(mFiles.Length(),
                           mozilla::fallible_t())) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  for (uint32_t i = 0; i < mFiles.Length(); ++i) {
    aSequence[i] = mFiles[i];
  }
}

} // namespace dom
} // namespace mozilla
