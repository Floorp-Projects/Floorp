/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PromiseNativeAbortCallback_h
#define mozilla_dom_PromiseNativeAbortCallback_h

#include "nsISupports.h"

namespace mozilla {
namespace dom {

/*
 * PromiseNativeAbortCallback allows C++ to react to an AbortablePromise being
 * aborted.
 */
class PromiseNativeAbortCallback : public nsISupports
{
protected:
  virtual ~PromiseNativeAbortCallback()
  { }

public:
  // Implemented in AbortablePromise.cpp.
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PromiseNativeAbortCallback)

  virtual void Call() = 0;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PromiseNativeAbortCallback_h
