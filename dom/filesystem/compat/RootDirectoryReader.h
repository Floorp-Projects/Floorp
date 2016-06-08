/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RootDirectoryReader_h
#define mozilla_dom_RootDirectoryReader_h

#include "DirectoryReader.h"

namespace mozilla {
namespace dom {

class RootDirectoryReader final : public DirectoryReader
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RootDirectoryReader, DirectoryReader)

  explicit RootDirectoryReader(nsIGlobalObject* aGlobalObject,
                               DOMFileSystem* aFileSystem,
                               const Sequence<RefPtr<Entry>>& aEntries);

  virtual void
  ReadEntries(EntriesCallback& aSuccessCallback,
              const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
              ErrorResult& aRv) override;

private:
  ~RootDirectoryReader();

  Sequence<RefPtr<Entry>> mEntries;
  bool mAlreadyRead;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_RootDirectoryReader_h
