/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SEEK_JOB_H
#define SEEK_JOB_H

#include "mozilla/MozPromise.h"
#include "MediaDecoder.h"
#include "MediaDecoderReader.h"
#include "SeekTarget.h"

namespace mozilla {

struct SeekJob {
  SeekJob();

  SeekJob(SeekJob&& aOther);

  SeekJob& operator=(SeekJob&& aOther);

  bool Exists() const;

  void Resolve(bool aAtEnd, const char* aCallSite);

  void RejectIfExists(const char* aCallSite);

  ~SeekJob();

  SeekTarget mTarget;
  MozPromiseHolder<MediaDecoder::SeekPromise> mPromise;
};

} // namespace mozilla

#endif /* SEEK_JOB_H */
