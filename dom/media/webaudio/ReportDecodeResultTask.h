/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ReportDecodeResultTask_h_
#define ReportDecodeResultTask_h_

#include "mozilla/Attributes.h"
#include "MediaBufferDecoder.h"

namespace mozilla {

class ReportDecodeResultTask final : public nsRunnable
{
public:
  ReportDecodeResultTask(DecodeJob& aDecodeJob,
                         DecodeJob::ResultFn aFunction)
    : mDecodeJob(aDecodeJob)
    , mFunction(aFunction)
  {
    MOZ_ASSERT(aFunction);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    (mDecodeJob.*mFunction)();

    return NS_OK;
  }

private:
  DecodeJob& mDecodeJob;
  DecodeJob::ResultFn mFunction;
};

}

#endif

