/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileEntry_h
#define mozilla_dom_FileEntry_h

#include "mozilla/dom/Entry.h"

namespace mozilla {
namespace dom {

class File;

class FileEntry final : public Entry
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileEntry, Entry)

  FileEntry(nsIGlobalObject* aGlobalObject, File* aFile);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual bool
  IsFile() const override
  {
    return true;
  }

  virtual void
  GetName(nsAString& aName, ErrorResult& aRv) const override;

  virtual void
  GetFullPath(nsAString& aFullPath, ErrorResult& aRv) const override;

  void
  CreateWriter(VoidCallback& aSuccessCallback,
               const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback) const;

  void
  GetFile(BlobCallback& aSuccessCallback,
          const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback) const;

private:
  ~FileEntry();

  RefPtr<File> mFile;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileEntry_h
