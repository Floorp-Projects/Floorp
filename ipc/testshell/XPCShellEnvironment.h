/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla IPCShell.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef _IPC_TESTSHELL_XPCSHELLENVIRONMENT_H_
#define _IPC_TESTSHELL_XPCSHELLENVIRONMENT_H_

#include "base/basictypes.h"

#include <string>
#include <stdio.h>

#include "nsAutoJSValHolder.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsStringGlue.h"

struct JSContext;
struct JSObject;
struct JSPrincipals;

class nsIJSContextStack;

namespace mozilla {
namespace ipc {

class XPCShellEnvironment
{
public:
    static XPCShellEnvironment* CreateEnvironment();
    ~XPCShellEnvironment();

    bool EvaluateString(const nsString& aString,
                        nsString* aResult = nsnull);

    JSPrincipals* GetPrincipal() {
        return mJSPrincipals;
    }

    JSObject* GetGlobalObject() {
        return mGlobalHolder.ToJSObject();
    }

    void SetExitCode(int aExitCode) {
        mExitCode = aExitCode;
    }
    int ExitCode() {
        return mExitCode;
    }

    void SetIsQuitting() {
        mQuitting = JS_TRUE;
    }
    JSBool IsQuitting() {
        return mQuitting;
    }

    void SetShouldReportWarnings(JSBool aReportWarnings) {
        mReportWarnings = aReportWarnings;
    }
    JSBool ShouldReportWarnings() {
        return mReportWarnings;
    }

    void SetShouldCompoleOnly(JSBool aCompileOnly) {
        mCompileOnly = aCompileOnly;
    }
    JSBool ShouldCompileOnly() {
        return mCompileOnly;
    }

    class AutoContextPusher
    {
    public:
        AutoContextPusher(XPCShellEnvironment* aEnv);
        ~AutoContextPusher();
    private:
        XPCShellEnvironment* mEnv;
    };

protected:
    XPCShellEnvironment();
    bool Init();

private:
    JSContext* mCx;
    nsAutoJSValHolder mGlobalHolder;
    nsCOMPtr<nsIJSContextStack> mCxStack;
    JSPrincipals* mJSPrincipals;

    int mExitCode;
    JSBool mQuitting;
    JSBool mReportWarnings;
    JSBool mCompileOnly;
};

} /* namespace ipc */
} /* namespace mozilla */

#endif /* _IPC_TESTSHELL_XPCSHELLENVIRONMENT_H_ */