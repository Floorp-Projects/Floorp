/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimeoutHandler.h"
#include "nsJSUtils.h"

namespace mozilla {
namespace dom {

TimeoutHandler::TimeoutHandler(JSContext* aCx) : TimeoutHandler() {
  nsJSUtils::GetCallingLocation(aCx, mFileName, &mLineNo, &mColumn);
}

void TimeoutHandler::Call() {}

void TimeoutHandler::GetLocation(const char** aFileName, uint32_t* aLineNo,
                                 uint32_t* aColumn) {
  *aFileName = mFileName.get();
  *aLineNo = mLineNo;
  *aColumn = mColumn;
}

NS_IMPL_CYCLE_COLLECTION_0(TimeoutHandler)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TimeoutHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TimeoutHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsITimeoutHandler)
NS_INTERFACE_MAP_END

}  // namespace dom
}  // namespace mozilla
