/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSPMessageUtils.h"
#include "nsSerializationHelper.h"
#include "BackgroundUtils.h"

namespace IPC {

using namespace mozilla::ipc;

void ParamTraits<nsIContentSecurityPolicy*>::Write(
    Message* aMsg, nsIContentSecurityPolicy* aParam) {
  bool isNull = !aParam;
  WriteParam(aMsg, isNull);
  if (isNull) {
    return;
  }

  CSPInfo csp;
  Unused << NS_WARN_IF(NS_FAILED(CSPToCSPInfo(aParam, &csp)));
  IPDLParamTraits<CSPInfo>::Write(aMsg, nullptr, csp);
}

bool ParamTraits<nsIContentSecurityPolicy*>::Read(
    const Message* aMsg, PickleIterator* aIter,
    RefPtr<nsIContentSecurityPolicy>* aResult) {
  bool isNull;
  if (!ReadParam(aMsg, aIter, &isNull)) {
    return false;
  }

  if (isNull) {
    *aResult = nullptr;
    return true;
  }

  CSPInfo csp;
  if (!IPDLParamTraits<CSPInfo>::Read(aMsg, aIter, nullptr, &csp)) {
    return false;
  }

  *aResult = CSPInfoToCSP(csp, nullptr, nullptr);
  return *aResult;
}

}  // namespace IPC
