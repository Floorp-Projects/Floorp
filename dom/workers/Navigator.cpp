/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Navigator.h"

#include "DOMBindingInlines.h"
#include "RuntimeService.h"

BEGIN_WORKERS_NAMESPACE

/* static */ already_AddRefed<WorkerNavigator>
WorkerNavigator::Create(JSContext* aCx, JS::Handle<JSObject*> aGlobal)
{
  RuntimeService* rts = RuntimeService::GetService();
  MOZ_ASSERT(rts);

  const RuntimeService::NavigatorStrings& strings =
    rts->GetNavigatorStrings();

  nsRefPtr<WorkerNavigator> navigator =
    new WorkerNavigator(aCx, strings.mAppName, strings.mAppVersion,
                        strings.mPlatform, strings.mUserAgent);

  if (!Wrap(aCx, aGlobal, navigator)) {
    return nullptr;
  }

  return navigator.forget();
}

void
WorkerNavigator::_trace(JSTracer* aTrc)
{
  DOMBindingBase::_trace(aTrc);
}

void
WorkerNavigator::_finalize(JSFreeOp* aFop)
{
  DOMBindingBase::_finalize(aFop);
}

END_WORKERS_NAMESPACE
