/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLClassifierChild.h"
#include "nsComponentManagerUtils.h"
#include "nsIURI.h"

using namespace mozilla::dom;

mozilla::ipc::IPCResult
URLClassifierChild::Recv__delete__(const MaybeInfo& aInfo,
                                   const nsresult& aResult)
{
  MOZ_ASSERT(mCallback);
  if (aInfo.type() == MaybeInfo::TClassifierInfo) {
    mCallback->OnClassifyComplete(aResult, aInfo.get_ClassifierInfo().list(),
                                  aInfo.get_ClassifierInfo().provider(),
                                  aInfo.get_ClassifierInfo().prefix());
  }
  return IPC_OK();
}
