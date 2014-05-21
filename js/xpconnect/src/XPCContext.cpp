/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Per JSContext object. */

#include "xpcprivate.h"

#include "jsapi.h"

/***************************************************************************/

XPCContext::XPCContext(XPCJSRuntime* aRuntime,
                       JSContext* aJSContext)
    :   mRuntime(aRuntime),
        mJSContext(aJSContext),
        mLastResult(NS_OK),
        mPendingResult(NS_OK),
        mCallingLangType(LANG_UNKNOWN)
{
    MOZ_COUNT_CTOR(XPCContext);

    MOZ_ASSERT(!JS_GetSecondContextPrivate(mJSContext), "Must be null");
    JS_SetSecondContextPrivate(mJSContext, this);
}

XPCContext::~XPCContext()
{
    MOZ_COUNT_DTOR(XPCContext);
    MOZ_ASSERT(JS_GetSecondContextPrivate(mJSContext) == this, "Must match this");
    JS_SetSecondContextPrivate(mJSContext, nullptr);
}

void
XPCContext::DebugDump(int16_t depth)
{
#ifdef DEBUG
    depth--;
    XPC_LOG_ALWAYS(("XPCContext @ %x", this));
        XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("mRuntime @ %x", mRuntime));
        XPC_LOG_ALWAYS(("mJSContext @ %x", mJSContext));
        XPC_LOG_ALWAYS(("mLastResult of %x", mLastResult));
        XPC_LOG_ALWAYS(("mPendingResult of %x", mPendingResult));
        XPC_LOG_ALWAYS(("mException @ %x", mException.get()));
        if (depth && mException) {
            // XXX show the exception here...
        }

        XPC_LOG_ALWAYS(("mCallingLangType of %s",
                        mCallingLangType == LANG_UNKNOWN ? "LANG_UNKNOWN" :
                        mCallingLangType == LANG_JS      ? "LANG_JS" :
                        "LANG_NATIVE"));
        XPC_LOG_OUTDENT();
#endif
}
