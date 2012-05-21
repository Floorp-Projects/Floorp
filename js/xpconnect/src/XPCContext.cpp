/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
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
        mSecurityManager(nsnull),
        mException(nsnull),
        mCallingLangType(LANG_UNKNOWN),
        mSecurityManagerFlags(0)
{
    MOZ_COUNT_CTOR(XPCContext);

    PR_INIT_CLIST(&mScopes);

    NS_ASSERTION(!JS_GetSecondContextPrivate(mJSContext), "Must be null");
    JS_SetSecondContextPrivate(mJSContext, this);
}

XPCContext::~XPCContext()
{
    MOZ_COUNT_DTOR(XPCContext);
    NS_ASSERTION(JS_GetSecondContextPrivate(mJSContext) == this, "Must match this");
    JS_SetSecondContextPrivate(mJSContext, nsnull);
    NS_IF_RELEASE(mException);
    NS_IF_RELEASE(mSecurityManager);

    // Iterate over our scopes and tell them that we have been destroyed
    for (PRCList *scopeptr = PR_NEXT_LINK(&mScopes);
         scopeptr != &mScopes;
         scopeptr = PR_NEXT_LINK(scopeptr)) {
        XPCWrappedNativeScope *scope =
            static_cast<XPCWrappedNativeScope*>(scopeptr);
        scope->ClearContext();
    }

    // we do not call JS_RemoveArgumentFormatter because we now only
    // delete XPCContext *after* the underlying JSContext is dead
}

void
XPCContext::DebugDump(PRInt16 depth)
{
#ifdef DEBUG
    depth--;
    XPC_LOG_ALWAYS(("XPCContext @ %x", this));
        XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("mRuntime @ %x", mRuntime));
        XPC_LOG_ALWAYS(("mJSContext @ %x", mJSContext));
        XPC_LOG_ALWAYS(("mLastResult of %x", mLastResult));
        XPC_LOG_ALWAYS(("mPendingResult of %x", mPendingResult));
        XPC_LOG_ALWAYS(("mSecurityManager @ %x", mSecurityManager));
        XPC_LOG_ALWAYS(("mSecurityManagerFlags of %x", mSecurityManagerFlags));

        XPC_LOG_ALWAYS(("mException @ %x", mException));
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
