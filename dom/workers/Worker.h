/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_worker_h__
#define mozilla_dom_workers_worker_h__

#include "Workers.h"

#include "mozilla/dom/DOMJSClass.h"

BEGIN_WORKERS_NAMESPACE

namespace worker {

JSObject*
InitClass(JSContext* aCx, JSObject* aGlobal, JSObject* aProto,
          bool aMainRuntime);

} // namespace worker

namespace chromeworker {

bool
InitClass(JSContext* aCx, JSObject* aGlobal, JSObject* aProto,
          bool aMainRuntime);

} // namespace chromeworker

bool
ClassIsWorker(JSClass* aClass);

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_worker_h__ */
