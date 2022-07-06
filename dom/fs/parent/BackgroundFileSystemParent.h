/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_BACKGROUNDFILESYSTEMPARENT_H_
#define DOM_FS_PARENT_BACKGROUNDFILESYSTEMPARENT_H_

#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/dom/PBackgroundFileSystemParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/dom/POriginPrivateFileSystemParent.h"
#include "mozilla/TaskQueue.h"
#include "nsISupports.h"

namespace mozilla::dom {

class BackgroundFileSystemParent : public PBackgroundFileSystemParent {
 public:
  explicit BackgroundFileSystemParent(
      const mozilla::ipc::PrincipalInfo& aPrincipalInfo)
      : mPrincipalInfo(aPrincipalInfo) {}

  mozilla::ipc::IPCResult RecvGetRoot(
      Endpoint<POriginPrivateFileSystemParent>&& aParentEp,
      GetRootResolver&& aResolver);

  NS_INLINE_DECL_REFCOUNTING(BackgroundFileSystemParent)

 protected:
  virtual ~BackgroundFileSystemParent() = default;

 private:
  mozilla::ipc::PrincipalInfo mPrincipalInfo;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_BACKGROUNDFILESYSTEMPARENT_H_
