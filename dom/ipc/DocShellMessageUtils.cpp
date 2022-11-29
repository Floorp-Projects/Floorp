/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DocShellMessageUtils.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "nsSerializationHelper.h"

namespace IPC {

void ParamTraits<nsDocShellLoadState*>::Write(IPC::MessageWriter* aWriter,
                                              nsDocShellLoadState* aParam) {
  MOZ_RELEASE_ASSERT(aParam);
  WriteParam(aWriter, aParam->Serialize(aWriter->GetActor()));
}

bool ParamTraits<nsDocShellLoadState*>::Read(
    IPC::MessageReader* aReader, RefPtr<nsDocShellLoadState>* aResult) {
  mozilla::dom::DocShellLoadStateInit loadState;
  if (!ReadParam(aReader, &loadState)) {
    return false;
  }

  // Assert if we somehow don't have a URI in our IPDL type, because we can't
  // construct anything out of it. This mimics the assertion in the constructor
  // for nsDocShellLoadState, but makes it clearer that the
  // DocShellLoadStateInit IPC object can't be clearly converted into a
  // nsDocShellLoadState.
  if (!loadState.URI()) {
    MOZ_ASSERT_UNREACHABLE("no URI in load state from IPC");
    return false;
  }

  bool readSuccess = false;
  RefPtr result =
      new nsDocShellLoadState(loadState, aReader->GetActor(), &readSuccess);
  if (readSuccess) {
    *aResult = result.forget();
  }
  return readSuccess;
}

}  // namespace IPC
