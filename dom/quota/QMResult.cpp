/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QMResult.h"

#ifdef QM_ERROR_STACKS_ENABLED
#  include "mozilla/Atomics.h"
#endif

namespace mozilla {

#ifdef QM_ERROR_STACKS_ENABLED
namespace {

static Atomic<uint64_t> gLastStackId{0};

}

QMResult::QMResult(nsresult aNSResult)
    : mStackId(++gLastStackId), mFrameId(1), mNSResult(aNSResult) {}
#endif

}  // namespace mozilla
