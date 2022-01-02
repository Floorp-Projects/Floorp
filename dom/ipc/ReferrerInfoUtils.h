/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_referrer_info_utils_h__
#define mozilla_dom_referrer_info_utils_h__

#include "ipc/IPCMessageUtils.h"
#include "nsCOMPtr.h"
#include "nsIReferrerInfo.h"

namespace IPC {

template <>
struct ParamTraits<nsIReferrerInfo*> {
  static void Write(Message* aMsg, nsIReferrerInfo* aParam);
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   RefPtr<nsIReferrerInfo>* aResult);
};

}  // namespace IPC

#endif  // mozilla_dom_referrer_info_utils_h__
