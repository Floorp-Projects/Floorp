/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMTHREADSAFESTREAMOWNER_H_
#define DOM_FS_FILESYSTEMTHREADSAFESTREAMOWNER_H_

#include "nsCOMPtr.h"

class nsIOutputStream;
class nsIRandomAccessStream;

namespace mozilla::dom {

class FileSystemWritableFileStream;

namespace fs {

class FileSystemThreadSafeStreamOwner {
 public:
  FileSystemThreadSafeStreamOwner(
      FileSystemWritableFileStream* aWritableFileStream,
      nsCOMPtr<nsIRandomAccessStream>&& aStream);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileSystemThreadSafeStreamOwner)

  nsresult Truncate(uint64_t aSize);

  nsresult Seek(uint64_t aPosition);

  void Close();

  nsCOMPtr<nsIOutputStream> OutputStream();

 protected:
  virtual ~FileSystemThreadSafeStreamOwner() = default;

 private:
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  FileSystemWritableFileStream* MOZ_NON_OWNING_REF mWritableFileStream;
#endif
  nsCOMPtr<nsIRandomAccessStream> mStream;

  bool mClosed;
};

}  // namespace fs
}  // namespace mozilla::dom

#endif  // DOM_FS_FILESYSTEMTHREADSAFESTREAMOWNER_H_
