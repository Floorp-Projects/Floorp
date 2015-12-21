/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemotePrintJobChild.h"

namespace mozilla {
namespace layout {

RemotePrintJobChild::RemotePrintJobChild()
{
  MOZ_COUNT_CTOR(RemotePrintJobChild);
}

bool
RemotePrintJobChild::RecvPageProcessed()
{
  NS_NOTREACHED("RemotePrintJobChild::RecvPageProcessed not implemented!");
  return false;
}

bool
RemotePrintJobChild::RecvAbortPrint(const nsresult& aRv)
{
  NS_NOTREACHED("RemotePrintJobChild::RecvAbortPrint not implemented!");
  return false;
}
void
RemotePrintJobChild::ProcessPage(Shmem& aStoredPage)
{
  SendProcessPage(aStoredPage);
}

RemotePrintJobChild::~RemotePrintJobChild()
{
  MOZ_COUNT_DTOR(RemotePrintJobChild);
}

void
RemotePrintJobChild::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace layout
} // namespace mozilla


