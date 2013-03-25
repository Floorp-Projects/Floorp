/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioParamTimeline_h_
#define AudioParamTimeline_h_

// This header is intended to make it possible to use AudioParamTimeline
// from multiple places without dealing with #include hell!

#include "AudioEventTimeline.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {

namespace dom {

typedef AudioEventTimeline<ErrorResult> AudioParamTimeline;

}
}

#endif

