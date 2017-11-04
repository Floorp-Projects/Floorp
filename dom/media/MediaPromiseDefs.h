/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MediaPromiseDefs_h_
#define MediaPromiseDefs_h_

#include "MediaResult.h"
#include "mozilla/MozPromise.h"

namespace mozilla {

using SetCDMPromise =
  MozPromise<bool /* aIgnored */, MediaResult, /* IsExclusive */ true>;

} // namespace mozilla

#endif
