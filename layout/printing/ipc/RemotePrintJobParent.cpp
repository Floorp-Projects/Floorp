/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemotePrintJobParent.h"

#include "nsIPrintSettings.h"

namespace mozilla {
namespace layout {

RemotePrintJobParent::RemotePrintJobParent(nsIPrintSettings* aPrintSettings)
  : mPrintSettings(aPrintSettings)
{
  MOZ_COUNT_CTOR(RemotePrintJobParent);
}

bool
RemotePrintJobParent::RecvInitializePrint(const nsString& aDocumentTitle,
                                          const nsString& aPrintToFile,
                                          const int32_t& aStartPage,
                                          const int32_t& aEndPage)
{
  NS_NOTREACHED("RemotePrintJobParent::RecvInitializePrint not implemented!");
  return false;
}

bool
RemotePrintJobParent::RecvProcessPage(Shmem&& aStoredPage)
{
  NS_NOTREACHED("PrintingParent::RecvProcessPage not implemented!");
  return false;
}

bool
RemotePrintJobParent::RecvFinalizePrint()
{
  NS_NOTREACHED("PrintingParent::RecvFinalizePrint not implemented!");
  return false;
}

bool
RemotePrintJobParent::RecvAbortPrint(const nsresult& aRv)
{
  NS_NOTREACHED("PrintingParent::RecvAbortPrint not implemented!");
  return false;
}

RemotePrintJobParent::~RemotePrintJobParent()
{
  MOZ_COUNT_DTOR(RemotePrintJobParent);
}

void
RemotePrintJobParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace layout
} // namespace mozilla


