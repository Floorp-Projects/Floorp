/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMQUOTACLIENTFACTORY_H_
#define DOM_FS_PARENT_FILESYSTEMQUOTACLIENTFACTORY_H_

#include "mozilla/AlreadyAddRefed.h"
#include "nsISupportsUtils.h"

template <class>
class RefPtr;

namespace mozilla::dom {

namespace quota {

class Client;

}  // namespace quota

namespace fs {

class FileSystemQuotaClientFactory {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(
      mozilla::dom::fs::FileSystemQuotaClientFactory);

  static void SetCustomFactory(
      RefPtr<FileSystemQuotaClientFactory> aCustomFactory);

  static already_AddRefed<quota::Client> CreateQuotaClient();

 protected:
  virtual ~FileSystemQuotaClientFactory() = default;

  virtual already_AddRefed<quota::Client> AllocQuotaClient();
};

inline already_AddRefed<quota::Client> CreateQuotaClient() {
  return FileSystemQuotaClientFactory::CreateQuotaClient();
}

}  // namespace fs
}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_FILESYSTEMQUOTACLIENTFACTORY_H_
