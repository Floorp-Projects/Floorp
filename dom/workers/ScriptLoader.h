/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_scriptloader_h__
#define mozilla_dom_workers_scriptloader_h__

#include "Workers.h"

#include "jsapi.h"

BEGIN_WORKERS_NAMESPACE

namespace scriptloader {

bool LoadWorkerScript(JSContext* aCx);

bool Load(JSContext* aCx, unsigned aURLCount, jsval* aURLs);

} // namespace scriptloader

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_scriptloader_h__ */
