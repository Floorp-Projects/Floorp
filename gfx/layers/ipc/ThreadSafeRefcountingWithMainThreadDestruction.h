/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef THREADSAFEREFCOUNTINGWITHMAINTHREADDESTRUCTION_H_
#define THREADSAFEREFCOUNTINGWITHMAINTHREADDESTRUCTION_H_

#include "nsISupportsImpl.h"
#include "MainThreadUtils.h"
#include "nsThreadUtils.h"

#define NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(  \
    _class)                                                                  \
 private:                                                                    \
  void DeleteOnMainThread() {                                                \
    if (NS_IsMainThread()) {                                                 \
      delete this;                                                           \
      return;                                                                \
    }                                                                        \
    NS_DispatchToMainThread(NewNonOwningRunnableMethod(                      \
        #_class "::DeleteOnMainThread", this, &_class::DeleteOnMainThread)); \
  }                                                                          \
                                                                             \
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DESTROY(_class,                 \
                                                     DeleteOnMainThread())

#endif
