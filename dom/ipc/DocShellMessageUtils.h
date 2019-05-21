/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_docshell_message_utils_h__
#define mozilla_dom_docshell_message_utils_h__

#include "ipc/IPCMessageUtils.h"
#include "nsCOMPtr.h"
#include "nsDocShellLoadState.h"

namespace mozilla {
namespace ipc {

template <>
struct IPDLParamTraits<nsDocShellLoadState*> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    nsDocShellLoadState* aParam);
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, RefPtr<nsDocShellLoadState>* aResult);
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_dom_docshell_message_utils_h__
