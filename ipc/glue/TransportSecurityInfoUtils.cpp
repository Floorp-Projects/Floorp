/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransportSecurityInfoUtils.h"

#include "ipc/IPCMessageUtils.h"
#include "mozilla/psm/TransportSecurityInfo.h"
#include "nsNSSCertificate.h"

namespace IPC {

void ParamTraits<nsITransportSecurityInfo*>::Write(
    MessageWriter* aWriter, nsITransportSecurityInfo* aParam) {
  bool nonNull = !!aParam;
  WriteParam(aWriter, nonNull);
  if (!nonNull) {
    return;
  }

  aParam->SerializeToIPC(aWriter);
}

bool ParamTraits<nsITransportSecurityInfo*>::Read(
    MessageReader* aReader, RefPtr<nsITransportSecurityInfo>* aResult) {
  *aResult = nullptr;

  bool nonNull = false;
  if (!ReadParam(aReader, &nonNull)) {
    return false;
  }

  if (!nonNull) {
    return true;
  }

  if (!mozilla::psm::TransportSecurityInfo::DeserializeFromIPC(aReader,
                                                               aResult)) {
    return false;
  }

  return true;
}

void ParamTraits<nsIX509Cert*>::Write(MessageWriter* aWriter,
                                      nsIX509Cert* aParam) {
  bool nonNull = !!aParam;
  WriteParam(aWriter, nonNull);
  if (!nonNull) {
    return;
  }

  aParam->SerializeToIPC(aWriter);
}

bool ParamTraits<nsIX509Cert*>::Read(MessageReader* aReader,
                                     RefPtr<nsIX509Cert>* aResult) {
  *aResult = nullptr;

  bool nonNull = false;
  if (!ReadParam(aReader, &nonNull)) {
    return false;
  }

  if (!nonNull) {
    return true;
  }

  RefPtr<nsIX509Cert> cert = new nsNSSCertificate();
  if (!cert->DeserializeFromIPC(aReader)) {
    return false;
  }

  *aResult = std::move(cert);
  return true;
}

}  // namespace IPC
