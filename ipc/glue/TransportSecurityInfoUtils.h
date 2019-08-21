/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_TransportSecurityInfoUtils_h
#define mozilla_ipc_TransportSecurityInfoUtils_h

#include "nsCOMPtr.h"
#include "nsITransportSecurityInfo.h"

namespace IPC {

template <>
struct ParamTraits<nsITransportSecurityInfo*> {
  static void Write(Message* aMsg, nsITransportSecurityInfo* aParam);
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   RefPtr<nsITransportSecurityInfo>* aResult);
};

template <>
struct ParamTraits<nsIX509Cert*> {
  static void Write(Message* aMsg, nsIX509Cert* aCert);
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   RefPtr<nsIX509Cert>* aResult);
};

template <>
struct ParamTraits<nsIX509CertList*> {
  static void Write(Message* aMsg, nsIX509CertList* aCertList);
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   RefPtr<nsIX509CertList>* aResult);
};

}  // namespace IPC

#endif  // mozilla_ipc_TransportSecurityInfoUtils_h
