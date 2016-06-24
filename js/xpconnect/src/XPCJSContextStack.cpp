/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement global service to track stack of JSContext. */

#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "nsDOMJSUtils.h"
#include "nsNullPrincipal.h"
#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;
using namespace JS;
using namespace xpc;

/***************************************************************************/

XPCJSContextStack::~XPCJSContextStack()
{
    if (mSafeJSContext) {
        mSafeJSContext = nullptr;
    }
}

JSContext*
XPCJSContextStack::GetSafeJSContext()
{
    MOZ_ASSERT(mSafeJSContext);
    return mSafeJSContext;
}

void
XPCJSContextStack::InitSafeJSContext()
{
    MOZ_ASSERT(!mSafeJSContext);

    mSafeJSContext = JS_GetContext(mRuntime->Runtime());
    if (!JS::InitSelfHostedCode(mSafeJSContext))
        MOZ_CRASH("InitSelfHostedCode failed");

    if (!mRuntime->JSContextInitialized(mSafeJSContext))
        MOZ_CRASH("JSContextCreated failed");
}
