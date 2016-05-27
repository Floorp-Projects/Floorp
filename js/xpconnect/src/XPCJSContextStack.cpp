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
using mozilla::dom::DestroyProtoAndIfaceCache;

/***************************************************************************/

XPCJSContextStack::~XPCJSContextStack()
{
    if (mSafeJSContext) {
        JS_DestroyContextNoGC(mSafeJSContext);
        mSafeJSContext = nullptr;
    }
}

void
XPCJSContextStack::Pop()
{
    MOZ_ASSERT(!mStack.IsEmpty());

    uint32_t idx = mStack.Length() - 1; // The thing we're popping

    mStack.RemoveElementAt(idx);
    JSContext* newTop;
    // We _could_ probably use mStack.SafeElementAt(idx-1, nullptr) here....
    if (idx == 0) {
        newTop = nullptr;
    } else {
        newTop = mStack[idx-1];
    }
    js::Debug_SetActiveJSContext(mRuntime->Runtime(), newTop);
}

void
XPCJSContextStack::Push(JSContext* cx)
{
    js::Debug_SetActiveJSContext(mRuntime->Runtime(), cx);
    mStack.AppendElement(cx);
}

JSContext*
XPCJSContextStack::GetSafeJSContext()
{
    MOZ_ASSERT(mSafeJSContext);
    return mSafeJSContext;
}

JSContext*
XPCJSContextStack::InitSafeJSContext()
{
    MOZ_ASSERT(!mSafeJSContext);
    mSafeJSContext = JS_NewContext(XPCJSRuntime::Get()->Runtime(), 8192);
    if (!mSafeJSContext)
        MOZ_CRASH();
    return mSafeJSContext;
}
