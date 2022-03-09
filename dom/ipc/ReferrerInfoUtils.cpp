/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReferrerInfoUtils.h"

#include "ipc/IPCMessageUtilsSpecializations.h"
#include "nsSerializationHelper.h"
#include "nsString.h"

namespace IPC {

void ParamTraits<nsIReferrerInfo*>::Write(MessageWriter* aWriter,
                                          nsIReferrerInfo* aParam) {
  bool isNull = !aParam;
  WriteParam(aWriter, isNull);
  if (isNull) {
    return;
  }
  nsAutoCString infoString;
  nsresult rv = NS_SerializeToString(aParam, infoString);
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Unable to serialize referrer info.");
    return;
  }
  WriteParam(aWriter, infoString);
}

bool ParamTraits<nsIReferrerInfo*>::Read(MessageReader* aReader,
                                         RefPtr<nsIReferrerInfo>* aResult) {
  bool isNull;
  if (!ReadParam(aReader, &isNull)) {
    return false;
  }
  if (isNull) {
    *aResult = nullptr;
    return true;
  }
  nsAutoCString infoString;
  if (!ReadParam(aReader, &infoString)) {
    return false;
  }
  nsCOMPtr<nsISupports> iSupports;
  nsresult rv = NS_DeserializeObject(infoString, getter_AddRefs(iSupports));
  NS_ENSURE_SUCCESS(rv, false);
  nsCOMPtr<nsIReferrerInfo> referrerInfo = do_QueryInterface(iSupports);
  NS_ENSURE_TRUE(referrerInfo, false);
  *aResult = ToRefPtr(std::move(referrerInfo));
  return true;
}

}  // namespace IPC
