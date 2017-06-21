/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MediaTaskUtils_h
#define mozilla_MediaTaskUtils_h

#include "nsThreadUtils.h"

// The main reason this file is separate from MediaUtils.h
#include "base/task.h"

namespace mozilla {
namespace media {

/* media::NewTaskFrom() - Create a Task from a lambda.
 *
 * Similar to media::NewRunnableFrom() - Create an nsRunnable from a lambda.
 */

template<typename OnRunType>
class LambdaTask : public Runnable
{
public:
  explicit LambdaTask(OnRunType&& aOnRun) : mOnRun(Move(aOnRun)) {}
private:
  NS_IMETHOD
  Run() override
  {
    mOnRun();
    return NS_OK;
  }
  OnRunType mOnRun;
};

template<typename OnRunType>
already_AddRefed<LambdaTask<OnRunType>>
NewTaskFrom(OnRunType&& aOnRun)
{
  typedef LambdaTask<OnRunType> LambdaType;
  RefPtr<LambdaType> lambda = new LambdaType(Forward<OnRunType>(aOnRun));
  return lambda.forget();
}

} // namespace media
} // namespace mozilla

#endif // mozilla_MediaTaskUtils_h
