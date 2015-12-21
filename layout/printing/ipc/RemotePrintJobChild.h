/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_RemotePrintJobChild_h
#define mozilla_layout_RemotePrintJobChild_h

#include "mozilla/layout/PRemotePrintJobChild.h"

#include "mozilla/RefPtr.h"

namespace mozilla {
namespace layout {

class RemotePrintJobChild final : public PRemotePrintJobChild
{
public:
  NS_INLINE_DECL_REFCOUNTING(RemotePrintJobChild)

  RemotePrintJobChild();

  void ActorDestroy(ActorDestroyReason aWhy) final;

  bool RecvPageProcessed() final;

  bool RecvAbortPrint(const nsresult& aRv) final;

  void ProcessPage(Shmem& aStoredPage);

private:
  ~RemotePrintJobChild() final;
};

} // namespace layout
} // namespace mozilla

#endif // mozilla_layout_RemotePrintJobChild_h
