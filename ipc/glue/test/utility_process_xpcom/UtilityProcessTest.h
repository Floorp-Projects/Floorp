/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_ipc_glue_UtilityProcessTest_h_
#define _include_ipc_glue_UtilityProcessTest_h_

#if defined(ENABLE_TESTS)
#  include "nsServiceManagerUtils.h"
#  include "nsIUtilityProcessTest.h"

namespace mozilla::ipc {

class UtilityProcessTest final : public nsIUtilityProcessTest {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUTILITYPROCESSTEST

  UtilityProcessTest() = default;

 private:
  ~UtilityProcessTest() = default;
};

}  // namespace mozilla::ipc
#endif  // defined(ENABLE_TESTS)

#endif  // _include_ipc_glue_UtilityProcessTest_h_
