/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_ipc_VsyncIOThreadHolder_h
#define mozilla_gfx_ipc_VsyncIOThreadHolder_h

#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace gfx {

class VsyncIOThreadHolder final {
 public:
  VsyncIOThreadHolder();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncIOThreadHolder)

  bool Start();

  RefPtr<nsIThread> GetThread() const;

  bool IsOnCurrentThread() const { return mThread->IsOnCurrentThread(); }

  void Dispatch(already_AddRefed<nsIRunnable> task) {
    mThread->Dispatch(std::move(task), NS_DISPATCH_NORMAL);
  }

 private:
  ~VsyncIOThreadHolder();

 private:
  RefPtr<nsIThread> mThread;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_ipc_VsyncIOThreadHolder_h
