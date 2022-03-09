/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSPMessageUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsSerializationHelper.h"
#include "mozilla/ipc/BackgroundUtils.h"

namespace IPC {

using namespace mozilla::ipc;

void ParamTraits<nsIContentSecurityPolicy*>::Write(
    MessageWriter* aWriter, nsIContentSecurityPolicy* aParam) {
  bool isNull = !aParam;
  WriteParam(aWriter, isNull);
  if (isNull) {
    return;
  }

  CSPInfo csp;
  mozilla::Unused << NS_WARN_IF(NS_FAILED(CSPToCSPInfo(aParam, &csp)));
  WriteParam(aWriter, csp);
}

bool ParamTraits<nsIContentSecurityPolicy*>::Read(
    MessageReader* aReader, RefPtr<nsIContentSecurityPolicy>* aResult) {
  bool isNull;
  if (!ReadParam(aReader, &isNull)) {
    return false;
  }

  if (isNull) {
    *aResult = nullptr;
    return true;
  }

  CSPInfo csp;
  if (!ReadParam(aReader, &csp)) {
    return false;
  }

  *aResult = CSPInfoToCSP(csp, nullptr, nullptr);
  return *aResult;
}

}  // namespace IPC
