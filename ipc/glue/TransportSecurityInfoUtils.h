/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_TransportSecurityInfoUtils_h
#define mozilla_ipc_TransportSecurityInfoUtils_h

#include "CertVerifier.h"
#include "ipc/EnumSerializer.h"
#include "mozilla/RefPtr.h"
#include "nsITransportSecurityInfo.h"
#include "nsIX509Cert.h"

class MessageReader;
class MessageWriter;

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

template <>
struct ParamTraits<nsITransportSecurityInfo::OverridableErrorCategory>
    : public ContiguousEnumSerializerInclusive<
          nsITransportSecurityInfo::OverridableErrorCategory,
          nsITransportSecurityInfo::OverridableErrorCategory::ERROR_UNSET,
          nsITransportSecurityInfo::OverridableErrorCategory::ERROR_TIME> {};

template <>
struct ParamTraits<mozilla::psm::EVStatus>
    : public ContiguousEnumSerializerInclusive<mozilla::psm::EVStatus,
                                               mozilla::psm::EVStatus::NotEV,
                                               mozilla::psm::EVStatus::EV> {};

}  // namespace IPC

#endif  // mozilla_ipc_TransportSecurityInfoUtils_h
