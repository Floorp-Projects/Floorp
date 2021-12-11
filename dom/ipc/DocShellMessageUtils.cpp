/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DocShellMessageUtils.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "nsSerializationHelper.h"

namespace mozilla::ipc {

void IPDLParamTraits<nsDocShellLoadState*>::Write(IPC::Message* aMsg,
                                                  IProtocol* aActor,
                                                  nsDocShellLoadState* aParam) {
  MOZ_RELEASE_ASSERT(aParam);
  WriteIPDLParam(aMsg, aActor, aParam->Serialize());
}

bool IPDLParamTraits<nsDocShellLoadState*>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    RefPtr<nsDocShellLoadState>* aResult) {
  dom::DocShellLoadStateInit loadState;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &loadState)) {
    return false;
  }

  // Assert if we somehow don't have a URI in our IPDL type, because we can't
  // construct anything out of it. This mimics the assertion in the constructor
  // for nsDocShellLoadState, but makes it clearer that the
  // DocShellLoadStateInit IPC object can't be clearly converted into a
  // nsDocShellLoadState.
  MOZ_ASSERT(loadState.URI());

  *aResult = new nsDocShellLoadState(loadState);
  return true;
}

}  // namespace mozilla::ipc
