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
 * The Original Code is the Python XPCOM language bindings.
 *
 * The Initial Developer of the Original Code is
 * ActiveState Tool Corp.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Hammond <mhammond@skippinet.com.au> (original author)
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

// pyloader
//
// Not part of the main Python _xpcom package, but a separate, thin DLL.
//
// The main loader and registrar for Python.  A thin DLL that is designed to live in
// the xpcom "components" directory.  Simply locates and loads the standard
// pyxpcom core library and transfers control to that.

#include <PyXPCOM.h>
#include "nsITimelineService.h"
#include "nsILocalFile.h"

typedef nsresult (*pfnPyXPCOM_NSGetModule)(nsIComponentManager *servMgr,
                                          nsIFile* location,
                                          nsIModule** result);


////////////////////////////////////////////////////////////
// This is the main entry point that delegates into Python
nsresult PyXPCOM_NSGetModule(nsIComponentManager *servMgr,
                             nsIFile* location,
                             nsIModule** result)
{
	NS_PRECONDITION(result!=NULL, "null result pointer in PyXPCOM_NSGetModule!");
	NS_PRECONDITION(location!=NULL, "null nsIFile pointer in PyXPCOM_NSGetModule!");
	NS_PRECONDITION(servMgr!=NULL, "null servMgr pointer in PyXPCOM_NSGetModule!");
	CEnterLeavePython _celp;
	PyObject *func = NULL;
	PyObject *obServMgr = NULL;
	PyObject *obLocation = NULL;
	PyObject *wrap_ret = NULL;
	PyObject *args = NULL;
	PyObject *mod = PyImport_ImportModule("xpcom.server");
	if (!mod) goto done;
	func = PyObject_GetAttrString(mod, "NS_GetModule");
	if (func==NULL) goto done;
	obServMgr = Py_nsISupports::PyObjectFromInterface(servMgr, NS_GET_IID(nsIComponentManager));
	if (obServMgr==NULL) goto done;
	obLocation = Py_nsISupports::PyObjectFromInterface(location, NS_GET_IID(nsIFile));
	if (obLocation==NULL) goto done;
	args = Py_BuildValue("OO", obServMgr, obLocation);
	if (args==NULL) goto done;
	wrap_ret = PyEval_CallObject(func, args);
	if (wrap_ret==NULL) goto done;
	Py_nsISupports::InterfaceFromPyObject(wrap_ret, NS_GET_IID(nsIModule), (nsISupports **)result, PR_FALSE, PR_FALSE);
done:
	nsresult nr = NS_OK;
	if (PyErr_Occurred()) {
		PyXPCOM_LogError("Obtaining the module object from Python failed.\n");
		nr = PyXPCOM_SetCOMErrorFromPyException();
	}
	Py_XDECREF(func);
	Py_XDECREF(obServMgr);
	Py_XDECREF(obLocation);
	Py_XDECREF(wrap_ret);
	Py_XDECREF(mod);
	Py_XDECREF(args);
	return nr;
}

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFile* location,
                                          nsIModule** result)
{
	PyXPCOM_EnsurePythonEnvironment();
	NS_TIMELINE_START_TIMER("PyXPCOM: PyXPCOM NSGetModule entry point");
	nsresult rc = PyXPCOM_NSGetModule(servMgr, location, result);
	NS_TIMELINE_STOP_TIMER("PyXPCOM: PyXPCOM NSGetModule entry point");
	NS_TIMELINE_MARK_TIMER("PyXPCOM: PyXPCOM NSGetModule entry point");
	return rc;
}
