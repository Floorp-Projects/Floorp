/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMSTREAMCALLBACKS_H_
#define DOM_FS_PARENT_FILESYSTEMSTREAMCALLBACKS_H_

#include "mozilla/dom/quota/RemoteQuotaObjectParentTracker.h"
#include "nsIInterfaceRequestor.h"
#include "nsTObserverArray.h"

namespace mozilla::dom {

class FileSystemStreamCallbacks : public nsIInterfaceRequestor,
                                  public quota::RemoteQuotaObjectParentTracker {
 public:
  FileSystemStreamCallbacks() = default;

  void CloseAllRemoteQuotaObjectParents();

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_DECL_NSIINTERFACEREQUESTOR

  void RegisterRemoteQuotaObjectParent(
      NotNull<quota::RemoteQuotaObjectParent*> aActor) override;

  void UnregisterRemoteQuotaObjectParent(
      NotNull<quota::RemoteQuotaObjectParent*> aActor) override;

 private:
  virtual ~FileSystemStreamCallbacks() = default;

  nsTObserverArray<quota::RemoteQuotaObjectParent*> mRemoteQuotaObjectParents;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_FILESYSTEMSTREAMCALLBACKS_H_
