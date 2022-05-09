/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemSecurity_h
#define mozilla_dom_FileSystemSecurity_h

#include "mozilla/dom/ipc/IdType.h"
#include "nsClassHashtable.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

class FileSystemSecurity final {
 public:
  NS_INLINE_DECL_REFCOUNTING(FileSystemSecurity)

  static already_AddRefed<FileSystemSecurity> Get();

  static already_AddRefed<FileSystemSecurity> GetOrCreate();

  void GrantAccessToContentProcess(ContentParentId aId,
                                   const nsAString& aDirectoryPath);

  void Forget(ContentParentId aId);

  bool ContentProcessHasAccessTo(ContentParentId aId, const nsAString& aPath);

 private:
  FileSystemSecurity();
  ~FileSystemSecurity();

  nsClassHashtable<nsUint64HashKey, nsTArray<nsString>> mPaths;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_FileSystemSecurity_h
