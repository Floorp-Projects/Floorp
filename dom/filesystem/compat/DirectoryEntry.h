/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DirectoryEntry_h
#define mozilla_dom_DirectoryEntry_h

#include "mozilla/dom/Entry.h"
#include "mozilla/dom/DOMFileSystemBinding.h"

namespace mozilla {
namespace dom {

class DirectoryEntry final : public Entry
{
public:
  explicit DirectoryEntry(nsIGlobalObject* aGlobalObject);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<DirectoryReader>
  CreateReader(ErrorResult& aRv) const
  {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  void
  GetFile(const nsAString& aPath, const FileSystemFlags& aFlag,
          const Optional<OwningNonNull<EntryCallback>>& aSuccessCallback,
          const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
          ErrorResult& aRv) const
  {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

  void
  GetDirectory(const nsAString& aPath, const FileSystemFlags& aFlag,
               const Optional<OwningNonNull<EntryCallback>>& aSuccessCallback,
               const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
               ErrorResult& aRv) const
  {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

  void
  RemoveRecursively(VoidCallback& aSuccessCallback,
                    const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
                    ErrorResult& aRv) const
  {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

private:
  ~DirectoryEntry();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DirectoryEntry_h
