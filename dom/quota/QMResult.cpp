/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QMResult.h"

#include "mozilla/Atomics.h"

namespace mozilla {

namespace {

static Atomic<uint64_t> gLastStackId{0};

}

QMResult::QMResult(nsresult aNSResult)
    : mStackId(++gLastStackId), mFrameId(1), mNSResult(aNSResult) {}

}  // namespace mozilla
