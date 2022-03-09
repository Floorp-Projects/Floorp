/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_URIUtils_h
#define mozilla_ipc_URIUtils_h

#include "mozilla/ipc/URIParams.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"

namespace mozilla {
namespace ipc {

void SerializeURI(nsIURI* aURI, URIParams& aParams);

void SerializeURI(nsIURI* aURI, Maybe<URIParams>& aParams);

already_AddRefed<nsIURI> DeserializeURI(const URIParams& aParams);

already_AddRefed<nsIURI> DeserializeURI(const Maybe<URIParams>& aParams);

template <>
struct IPDLParamTraits<nsIURI*> {
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    nsIURI* aParam) {
    Maybe<URIParams> params;
    SerializeURI(aParam, params);
    WriteIPDLParam(aWriter, aActor, params);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   RefPtr<nsIURI>* aResult) {
    Maybe<URIParams> params;
    if (!ReadIPDLParam(aReader, aActor, &params)) {
      return false;
    }
    *aResult = DeserializeURI(params);
    return true;
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_URIUtils_h
