/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_FILESYSTEMMANAGERCHILDBLOCKER_H_
#define DOM_FS_CHILD_FILESYSTEMMANAGERCHILDBLOCKER_H_

#include "mozilla/AlreadyAddRefed.h"
#include "nsIAsyncShutdown.h"
#include "nsISupports.h"

namespace mozilla::dom::fs {

class FileSystemShutdownBlocker : public nsIAsyncShutdownBlocker {
 public:
  static already_AddRefed<FileSystemShutdownBlocker> CreateForWritable();

  virtual void SetCallback(std::function<void()>&& aCallback) = 0;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

  virtual NS_IMETHODIMP Block() = 0;

  virtual NS_IMETHODIMP Unblock() = 0;

 protected:
  virtual ~FileSystemShutdownBlocker() = default;
};

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_CHILD_FILESYSTEMMANAGERCHILDBLOCKER_H_
