/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Per JSContext object. */

#include "xpcprivate.h"

/***************************************************************************/

// static
XPCContext*
XPCContext::newXPCContext(XPCJSRuntime* aRuntime,
                          JSContext* aJSContext)
{
    NS_PRECONDITION(aRuntime,"bad param");
    NS_PRECONDITION(aJSContext,"bad param");
    NS_ASSERTION(JS_GetRuntime(aJSContext) == aRuntime->GetJSRuntime(),
                 "XPConnect can not be used on multiple JSRuntimes!");

    return new XPCContext(aRuntime, aJSContext);
}

MOZ_DECL_CTOR_COUNTER(XPCContext)

XPCContext::XPCContext(XPCJSRuntime* aRuntime,
                       JSContext* aJSContext)
    :   mRuntime(aRuntime),
        mJSContext(aJSContext),
        mLastResult(NS_OK),
        mPendingResult(NS_OK),
        mSecurityManager(nsnull),
        mSecurityManagerFlags(0),
        mException(nsnull),
        mCallingLangType(LANG_UNKNOWN)
{
    MOZ_COUNT_CTOR(XPCContext);

    for(const char** p =  XPC_ARG_FORMATTER_FORMAT_STRINGS; *p; p++)
        JS_AddArgumentFormatter(mJSContext, *p, XPC_JSArgumentFormatter);
}

XPCContext::~XPCContext()
{
    MOZ_COUNT_DTOR(XPCContext);
    NS_IF_RELEASE(mException);
    NS_IF_RELEASE(mSecurityManager);
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
        if(depth && mException)
        {
            // XXX show the exception here...
        }

        XPC_LOG_ALWAYS(("mCallingLangType of %s",
                         mCallingLangType == LANG_UNKNOWN ? "LANG_UNKNOWN" :
                         mCallingLangType == LANG_JS      ? "LANG_JS" :
                                                            "LANG_NATIVE"));
        XPC_LOG_OUTDENT();
#endif
}
