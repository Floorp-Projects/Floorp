/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DirectoryEntry.h"
#include "DirectoryReader.h"
#include "ErrorCallbackRunnable.h"
#include "mozilla/dom/Directory.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(DirectoryEntry, Entry, mDirectory)

NS_IMPL_ADDREF_INHERITED(DirectoryEntry, Entry)
NS_IMPL_RELEASE_INHERITED(DirectoryEntry, Entry)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DirectoryEntry)
NS_INTERFACE_MAP_END_INHERITING(Entry)

DirectoryEntry::DirectoryEntry(nsIGlobalObject* aGlobal,
                               Directory* aDirectory)
  : Entry(aGlobal)
  , mDirectory(aDirectory)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(mDirectory);
}

DirectoryEntry::~DirectoryEntry()
{}

JSObject*
DirectoryEntry::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DirectoryEntryBinding::Wrap(aCx, this, aGivenProto);
}

void
DirectoryEntry::GetName(nsAString& aName, ErrorResult& aRv) const
{
  mDirectory->GetName(aName, aRv);
}

void
DirectoryEntry::GetFullPath(nsAString& aPath, ErrorResult& aRv) const
{
  mDirectory->GetPath(aPath, aRv);
}

already_AddRefed<DirectoryReader>
DirectoryEntry::CreateReader() const
{
  RefPtr<DirectoryReader> reader =
    new DirectoryReader(GetParentObject(), mDirectory);
  return reader.forget();
}

void
DirectoryEntry::RemoveRecursively(VoidCallback& aSuccessCallback,
                                  const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback) const
{
  if (aErrorCallback.WasPassed()) {
    RefPtr<ErrorCallbackRunnable> runnable =
      new ErrorCallbackRunnable(GetParentObject(),
                                &aErrorCallback.Value());
    nsresult rv = NS_DispatchToMainThread(runnable);
    NS_WARN_IF(NS_FAILED(rv));
  }
}

} // dom namespace
} // mozilla namespace
