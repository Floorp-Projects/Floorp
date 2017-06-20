/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_ASYNCEVENTRUNNER_H_
#define MOZILLA_ASYNCEVENTRUNNER_H_

#include "nsThreadUtils.h"

namespace mozilla {

template <typename T>
class AsyncEventRunner : public Runnable
{
public:
  AsyncEventRunner(T* aTarget, const char* aName)
    : mTarget(aTarget)
    , mName(aName)
  {}

  NS_IMETHOD Run() override
  {
    mTarget->DispatchSimpleEvent(mName);
    return NS_OK;
  }

private:
  RefPtr<T> mTarget;
  const char* mName;
};

} // namespace mozilla

#endif /* MOZILLA_ASYNCEVENTRUNNER_H_ */
