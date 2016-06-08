/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RootDirectoryEntry_h
#define mozilla_dom_RootDirectoryEntry_h

#include "mozilla/dom/DirectoryEntry.h"

namespace mozilla {
namespace dom {

class RootDirectoryEntry final : public DirectoryEntry
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RootDirectoryEntry, DirectoryEntry)

  RootDirectoryEntry(nsIGlobalObject* aGlobalObject,
                     const Sequence<RefPtr<Entry>>& aEntries,
                     DOMFileSystem* aFileSystem);

  virtual void
  GetName(nsAString& aName, ErrorResult& aRv) const override;

  virtual void
  GetFullPath(nsAString& aFullPath, ErrorResult& aRv) const override;

  virtual already_AddRefed<DirectoryReader>
  CreateReader() const override;

private:
  ~RootDirectoryEntry();

  virtual void
  GetInternal(const nsAString& aPath, const FileSystemFlags& aFlag,
              const Optional<OwningNonNull<EntryCallback>>& aSuccessCallback,
              const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
              GetInternalType aType) const override;

  void
  Error(const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
        nsresult aError) const;

  Sequence<RefPtr<Entry>> mEntries;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_RootDirectoryEntry_h
