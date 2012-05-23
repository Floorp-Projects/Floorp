/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequestEventTarget.h"

USING_WORKERS_NAMESPACE

void
XMLHttpRequestEventTarget::_trace(JSTracer* aTrc)
{
  EventTarget::_trace(aTrc);
}

void
XMLHttpRequestEventTarget::_finalize(JSFreeOp* aFop)
{
  EventTarget::_finalize(aFop);
}
