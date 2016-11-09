/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SeekJob.h"

namespace mozilla {

SeekJob::SeekJob()
{
}

SeekJob::SeekJob(SeekJob&& aOther) : mTarget(aOther.mTarget)
{
  aOther.mTarget.Reset();
  mPromise = Move(aOther.mPromise);
}

SeekJob::~SeekJob()
{
  MOZ_DIAGNOSTIC_ASSERT(!mTarget.IsValid());
  MOZ_DIAGNOSTIC_ASSERT(mPromise.IsEmpty());
}

SeekJob& SeekJob::operator=(SeekJob&& aOther)
{
  MOZ_DIAGNOSTIC_ASSERT(!Exists());
  mTarget = aOther.mTarget;
  aOther.mTarget.Reset();
  mPromise = Move(aOther.mPromise);
  return *this;
}

bool SeekJob::Exists() const
{
  MOZ_ASSERT(mTarget.IsValid() == !mPromise.IsEmpty());
  return mTarget.IsValid();
}

void SeekJob::Resolve(bool aAtEnd, const char* aCallSite)
{
  MediaDecoder::SeekResolveValue val;
  val.mAtEnd = aAtEnd;
  mPromise.Resolve(val, aCallSite);
  mTarget.Reset();
}

void SeekJob::RejectIfExists(const char* aCallSite)
{
  mTarget.Reset();
  mPromise.RejectIfExists(true, aCallSite);
}

} // namespace mozilla
