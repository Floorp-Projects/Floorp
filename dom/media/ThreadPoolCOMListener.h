/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MSCOMInitThreadPoolListener_h_
#define MSCOMInitThreadPoolListener_h_

#include "nsIThreadPool.h"
#include <objbase.h>

namespace mozilla {

// Thread pool listener which ensures that MSCOM is initialized and
// deinitialized on the thread pool thread. We may call into WMF on this thread,
// so we need MSCOM working.
class MSCOMInitThreadPoolListener final : public nsIThreadPoolListener {
  ~MSCOMInitThreadPoolListener() {}
  public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITHREADPOOLLISTENER
};

} // namespace mozilla


#endif // MSCOMInitThreadPoolListener_h_
