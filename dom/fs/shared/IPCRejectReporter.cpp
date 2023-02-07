/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/ipc/MessageChannel.h"

namespace mozilla::dom::fs {

// TODO: Find a better way to deal with these errors
void IPCRejectReporter(mozilla::ipc::ResponseRejectReason aReason) {
  switch (aReason) {
    case mozilla::ipc::ResponseRejectReason::ActorDestroyed:
      // This is ok
      break;
    case mozilla::ipc::ResponseRejectReason::HandlerRejected:
      QM_TRY(OkIf(false), QM_VOID);
      break;
    case mozilla::ipc::ResponseRejectReason::ChannelClosed:
      QM_TRY(OkIf(false), QM_VOID);
      break;
    case mozilla::ipc::ResponseRejectReason::ResolverDestroyed:
      QM_TRY(OkIf(false), QM_VOID);
      break;
    case mozilla::ipc::ResponseRejectReason::SendError:
      QM_TRY(OkIf(false), QM_VOID);
      break;
    default:
      QM_TRY(OkIf(false), QM_VOID);
      break;
  }
}

}  // namespace mozilla::dom::fs
