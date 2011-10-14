/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Per JSContext object. */

#include "xpcprivate.h"

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

    NS_ASSERTION(!mJSContext->data2, "Must be null");
    mJSContext->data2 = this;
}

XPCContext::~XPCContext()
{
    MOZ_COUNT_DTOR(XPCContext);
    NS_ASSERTION(mJSContext->data2 == this, "Must match this");
    mJSContext->data2 = nsnull;
    NS_IF_RELEASE(mException);
    NS_IF_RELEASE(mSecurityManager);

    // Iterate over our scopes and tell them that we have been destroyed
    for(PRCList *scopeptr = PR_NEXT_LINK(&mScopes);
        scopeptr != &mScopes;
        scopeptr = PR_NEXT_LINK(scopeptr))
    {
        XPCWrappedNativeScope *scope = (XPCWrappedNativeScope *)scopeptr;
        scope->SetContext(nsnull);
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
