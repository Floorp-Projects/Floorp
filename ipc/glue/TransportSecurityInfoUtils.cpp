/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransportSecurityInfoUtils.h"

#include "ipc/IPCMessageUtils.h"
#include "mozilla/psm/TransportSecurityInfo.h"

namespace IPC {

void ParamTraits<nsITransportSecurityInfo*>::Write(
    Message* aMsg, nsITransportSecurityInfo* aParam) {
  bool nonNull = !!aParam;
  WriteParam(aMsg, nonNull);
  if (!nonNull) {
    return;
  }

  aParam->SerializeToIPC(aMsg);
}

bool ParamTraits<nsITransportSecurityInfo*>::Read(
    const Message* aMsg, PickleIterator* aIter,
    RefPtr<nsITransportSecurityInfo>* aResult) {
  *aResult = nullptr;

  bool nonNull = false;
  if (!ReadParam(aMsg, aIter, &nonNull)) {
    return false;
  }

  if (!nonNull) {
    return true;
  }

  RefPtr<nsITransportSecurityInfo> info =
      new mozilla::psm::TransportSecurityInfo();
  if (!info->DeserializeFromIPC(aMsg, aIter)) {
    return false;
  }

  *aResult = info.forget();
  return true;
}

void ParamTraits<nsIX509Cert*>::Write(Message* aMsg, nsIX509Cert* aParam) {
  bool nonNull = !!aParam;
  WriteParam(aMsg, nonNull);
  if (!nonNull) {
    return;
  }

  aParam->SerializeToIPC(aMsg);
}

bool ParamTraits<nsIX509Cert*>::Read(const Message* aMsg, PickleIterator* aIter,
                                     RefPtr<nsIX509Cert>* aResult) {
  *aResult = nullptr;

  bool nonNull = false;
  if (!ReadParam(aMsg, aIter, &nonNull)) {
    return false;
  }

  if (!nonNull) {
    return true;
  }

  RefPtr<nsIX509Cert> cert = new nsNSSCertificate();
  if (!cert->DeserializeFromIPC(aMsg, aIter)) {
    return false;
  }

  *aResult = cert.forget();
  return true;
}

void ParamTraits<nsIX509CertList*>::Write(Message* aMsg,
                                          nsIX509CertList* aParam) {
  bool nonNull = !!aParam;
  WriteParam(aMsg, nonNull);
  if (!nonNull) {
    return;
  }

  aParam->SerializeToIPC(aMsg);
}

bool ParamTraits<nsIX509CertList*>::Read(const Message* aMsg,
                                         PickleIterator* aIter,
                                         RefPtr<nsIX509CertList>* aResult) {
  bool nonNull = false;
  if (!ReadParam(aMsg, aIter, &nonNull)) {
    return false;
  }

  if (!nonNull) {
    *aResult = nullptr;
    return true;
  }

  RefPtr<nsIX509CertList> certList = new nsNSSCertList();
  if (!certList->DeserializeFromIPC(aMsg, aIter)) {
    return false;
  }

  *aResult = certList.forget();
  return true;
}

}  // namespace IPC
