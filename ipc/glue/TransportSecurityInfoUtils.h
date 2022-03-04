/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_TransportSecurityInfoUtils_h
#define mozilla_ipc_TransportSecurityInfoUtils_h

#include "nsCOMPtr.h"
#include "nsITransportSecurityInfo.h"

namespace IPC {

template <typename>
struct ParamTraits;

template <>
struct ParamTraits<nsITransportSecurityInfo*> {
  static void Write(MessageWriter* aWriter, nsITransportSecurityInfo* aParam);
  static bool Read(MessageReader* aReader,
                   RefPtr<nsITransportSecurityInfo>* aResult);
};

template <>
struct ParamTraits<nsIX509Cert*> {
  static void Write(MessageWriter* aWriter, nsIX509Cert* aCert);
  static bool Read(MessageReader* aReader, RefPtr<nsIX509Cert>* aResult);
};

}  // namespace IPC

#endif  // mozilla_ipc_TransportSecurityInfoUtils_h
