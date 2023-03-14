/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_dom_media_RddProcessTest_h_
#define _include_dom_media_RddProcessTest_h_

#if defined(ENABLE_TESTS)
#  include "nsIRddProcessTest.h"

namespace mozilla {

class RddProcessTest final : public nsIRddProcessTest {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRDDPROCESSTEST

  RddProcessTest() = default;

 private:
  ~RddProcessTest() = default;
};

}  // namespace mozilla
#endif  // defined(ENABLE_TESTS)

#endif  // _include_dom_media_RddProcessTest_h_
