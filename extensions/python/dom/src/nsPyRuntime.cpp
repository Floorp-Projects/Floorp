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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Hammond (original author)
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

#include "nsPyRuntime.h"
#include "nsPyContext.h"
#include "nsICategoryManager.h"
#include "nsIScriptContext.h"
#include "nsIDOMEventReceiver.h"

extern void init_nsdom();

static PRBool initialized = PR_FALSE;

// QueryInterface implementation for nsPythonRuntime
NS_INTERFACE_MAP_BEGIN(nsPythonRuntime)
  NS_INTERFACE_MAP_ENTRY(nsIScriptRuntime)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsPythonRuntime)
NS_IMPL_RELEASE(nsPythonRuntime)

nsresult
nsPythonRuntime::CreateContext(nsIScriptContext **ret)
{
    PyXPCOM_EnsurePythonEnvironment();
    if (!initialized) {
        PyInit_DOMnsISupports();
        init_nsdom();
        initialized = PR_TRUE;
    }
    *ret = new nsPythonContext();
    if (!ret)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_IF_ADDREF(*ret);
    return NS_OK;
}

nsresult
nsPythonRuntime::ParseVersion(const nsString &aVersionStr, PRUint32 *aFlags) {
    // We ignore the version, but it is safer to fail whenever a version is
    // specified, thereby ensuring noone will ever specify a version with
    // Python, allowing future semantics to be defined without concern for
    // existing behaviour.
    if (aVersionStr.IsEmpty()) {
        *aFlags = 0;
        return NS_OK;
    }
    NS_ERROR("Don't specify a version for Python");
    return NS_ERROR_UNEXPECTED;
  }

nsresult
nsPythonRuntime::DropScriptObject(void *object)
{
    if (object) {
        PYLEAK_STAT_DECREMENT(ScriptObject);
        CEnterLeavePython _celp;
        Py_DECREF((PyObject *)object);
    }
    return NS_OK;
}

nsresult
nsPythonRuntime::HoldScriptObject(void *object)
{
    if (object) {
        PYLEAK_STAT_INCREMENT(ScriptObject);
        CEnterLeavePython _celp;
        Py_INCREF((PyObject *)object);
    }
    return NS_OK;
}
