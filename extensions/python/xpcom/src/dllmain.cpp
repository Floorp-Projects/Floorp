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

//
// This code is part of the XPCOM extensions for Python.
//
// Written May 2000 by Mark Hammond.
//
// Based heavily on the Python COM support, which is
// (c) Mark Hammond and Greg Stein.
//
// (c) 2000, ActiveState corp.

#include "PyXPCOM_std.h"
#include "nsDirectoryServiceDefs.h"
#include "nsILocalFile.h"
#include "nsITimelineService.h"

#include "nspr.h" // PR_fprintf

#ifdef XP_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "windows.h"
#endif

#ifdef XP_UNIX
#include <dlfcn.h>
#include <sys/stat.h>
#endif

static PRLock *g_lockMain = nsnull;

PYXPCOM_EXPORT PyObject *PyXPCOM_Error = NULL;
PYXPCOM_EXPORT PRBool PyXPCOM_ModuleInitialized = PR_FALSE;

PyXPCOM_INTERFACE_DEFINE(Py_nsIComponentManager, nsIComponentManager, PyMethods_IComponentManager)
PyXPCOM_INTERFACE_DEFINE(Py_nsIInterfaceInfoManager, nsIInterfaceInfoManager, PyMethods_IInterfaceInfoManager)
PyXPCOM_INTERFACE_DEFINE(Py_nsIEnumerator, nsIEnumerator, PyMethods_IEnumerator)
PyXPCOM_INTERFACE_DEFINE(Py_nsISimpleEnumerator, nsISimpleEnumerator, PyMethods_ISimpleEnumerator)
PyXPCOM_INTERFACE_DEFINE(Py_nsIInterfaceInfo, nsIInterfaceInfo, PyMethods_IInterfaceInfo)
PyXPCOM_INTERFACE_DEFINE(Py_nsIInputStream, nsIInputStream, PyMethods_IInputStream)
PyXPCOM_INTERFACE_DEFINE(Py_nsIClassInfo, nsIClassInfo, PyMethods_IClassInfo)
PyXPCOM_INTERFACE_DEFINE(Py_nsIVariant, nsIVariant, PyMethods_IVariant)

////////////////////////////////////////////////////////////
// Lock/exclusion global functions.
//
PYXPCOM_EXPORT void
PyXPCOM_AcquireGlobalLock(void)
{
	NS_PRECONDITION(g_lockMain != nsnull, "Cant acquire a NULL lock!");
	PR_Lock(g_lockMain);
}

PYXPCOM_EXPORT void
PyXPCOM_ReleaseGlobalLock(void)
{
	NS_PRECONDITION(g_lockMain != nsnull, "Cant release a NULL lock!");
	PR_Unlock(g_lockMain);
}

// Ensure that any paths guaranteed by this package exist on sys.path
// Only called once as we are first loaded into the process.
void AddStandardPaths()
{
	// Put {bin}\Python on the path if it exists.
	nsresult rv;
	nsCOMPtr<nsIFile> aFile;
	// XXX - this needs more thought for XULRunner - we want to stick the global
	// 'python' dir on sys.path (for xpcom etc), but also want to support a way
	// of adding a local application directory (in the case of xulrunner) too.
	// NS_XPCOM_CURRENT_PROCESS_DIR is the XULRunner app dir (ie, where application.ini lives)
	// NS_GRE_DIR is the 'bin' dir for XULRunner itself.
	rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(aFile));
	if (NS_FAILED(rv)) {
		PyXPCOM_LogError("The Python XPCOM loader could not locate the 'bin' directory");
		return;
	}
	aFile->Append(NS_LITERAL_STRING("python"));
	nsAutoString pathBuf;
	aFile->GetPath(pathBuf);
	PyObject *obPath = PySys_GetObject("path");
	if (!obPath) {
		PyXPCOM_LogError("The Python XPCOM loader could not get the Python sys.path variable");
		return;
	}
	// XXX - this should use the file-system encoding...
	NS_LossyConvertUTF16toASCII pathCBuf(pathBuf);
#ifdef NS_DEBUG
	PR_fprintf(PR_STDERR,"The Python XPCOM loader is adding '%s' to sys.path\n",
	           pathCBuf.get());
#endif
	PyObject *newStr = PyString_FromString(pathCBuf.get());
	PyList_Insert(obPath, 0, newStr);
	Py_XDECREF(newStr);
	// And now try and get Python to process this directory as a "site dir" 
	// - ie, look for .pth files, etc
	nsCAutoString cmdBuf(NS_LITERAL_CSTRING("import site;site.addsitedir(r'") + pathCBuf + NS_LITERAL_CSTRING("')\n"));
	if (0 != PyRun_SimpleString((char *)cmdBuf.get())) {
		PyXPCOM_LogError("The directory '%s' could not be added as a site directory", pathCBuf.get());
		PyErr_Clear();
	}
	// and somewhat like Python itself (site, citecustomize), we attempt 
	// to import "sitepyxpcom" ignoring ImportError
	PyObject *mod = PyImport_ImportModule("sitepyxpcom");
	if (NULL==mod) {
		if (!PyErr_ExceptionMatches(PyExc_ImportError))
			PyXPCOM_LogError("Failed to import 'sitepyxpcom'");
		PyErr_Clear();
	} else
		Py_DECREF(mod);
}

static PRBool bIsInitialized = PR_FALSE;
// Our 'entry point' into initialization - just call this any time you
// like, and the world will be setup!
PYXPCOM_EXPORT void
PyXPCOM_EnsurePythonEnvironment(void)
{
	// Must be thread-safe - but only while set to FALSE - so check for
	// set before getting the lock - then check again after!
	if (bIsInitialized)
		return;
	CEnterLeaveXPCOMFramework _celf;
	if (bIsInitialized)
		return; // another thread beat us to the init.

#if defined(XP_UNIX) && !defined(XP_MACOSX)
	/* *sob* - seems necessary to open the .so as RTLD_GLOBAL.  Without
	this we see:
	    Traceback (most recent call last):
	      File "<string>", line 1, in ?
	      File "/usr/lib/python2.4/logging/__init__.py", line 29, in ?
	        import sys, os, types, time, string, cStringIO, traceback
	    ImportError: /usr/lib/python2.4/lib-dynload/time.so: undefined
	                                                symbol: PyExc_IOError

	On osx, ShaneC writes that is it unnecessary (and fails anyway since 
	PYTHON_SO is wrong.)  More clues about this welcome!
	*/

	dlopen(PYTHON_SO,RTLD_NOW | RTLD_GLOBAL);
#endif

	PRBool bDidInitPython = !Py_IsInitialized(); // well, I will next line, anyway :-)
	if (bDidInitPython) {
		NS_TIMELINE_START_TIMER("PyXPCOM: Python initializing");
		Py_Initialize(); // NOTE: We never finalize Python!!
#ifndef NS_DEBUG
		Py_OptimizeFlag = 1;
#endif // NS_DEBUG
		// Must force Python to start using thread locks, as
		// this is certainly a threaded environment we are playing in
		PyEval_InitThreads();

		NS_TIMELINE_STOP_TIMER("PyXPCOM: Python initializing");
		NS_TIMELINE_MARK_TIMER("PyXPCOM: Python initializing");
	}
	// Get the Python interpreter state
	NS_TIMELINE_START_TIMER("PyXPCOM: Python threadstate setup");
	PyGILState_STATE state = PyGILState_Ensure();
#ifdef MOZ_TIMELINE
	// If the timeline service is installed, see if we can install our hooks.
	if (NULL==PyImport_ImportModule("timeline_hook")) {
		if (!PyErr_ExceptionMatches(PyExc_ImportError))
			PyXPCOM_LogError("Failed to import 'timeline_hook'");
		PyErr_Clear(); // but don't care if we can't.
	}
#endif

	// Make sure we have _something_ as sys.argv.
	if (PySys_GetObject("argv")==NULL) {
		PyObject *path = PyList_New(0);
		PyObject *str = PyString_FromString("");
		PyList_Append(path, str);
		PySys_SetObject("argv", path);
		Py_XDECREF(path);
		Py_XDECREF(str);
	}

	// Add the standard extra paths we assume
	AddStandardPaths();

	// The exception object pyxpcom uses.
	if (PyXPCOM_Error == NULL) {
		PRBool rc = PR_FALSE;
		PyObject *mod = NULL;

		mod = PyImport_ImportModule("xpcom");
		if (mod!=NULL) {
			PyXPCOM_Error = PyObject_GetAttrString(mod, "Exception");
			Py_DECREF(mod);
		}
		rc = (PyXPCOM_Error != NULL);
	}

	// Register our custom interfaces.
	Py_nsISupports::InitType();
	Py_nsIComponentManager::InitType();
	Py_nsIInterfaceInfoManager::InitType();
	Py_nsIEnumerator::InitType();
	Py_nsISimpleEnumerator::InitType();
	Py_nsIInterfaceInfo::InitType();
	Py_nsIInputStream::InitType();
	Py_nsIClassInfo::InitType();
	Py_nsIVariant::InitType();

	bIsInitialized = PR_TRUE;
	// import the xpcom module itself to setup the loggers etc.
	// We must do this after setting bIsInitialized, as it too tries to
	// initialize!
	PyImport_ImportModule("xpcom");

	// If we initialized Python, then we will also have acquired the thread
	// lock.  In that case, we want to leave it unlocked, so other threads
	// are free to run, even if they aren't running Python code.
	PyGILState_Release(bDidInitPython ? PyGILState_UNLOCKED : state);

	NS_TIMELINE_STOP_TIMER("PyXPCOM: Python threadstate setup");
	NS_TIMELINE_MARK_TIMER("PyXPCOM: Python threadstate setup");
}

void pyxpcom_construct(void)
{
	// Create the lock we will use to ensure startup thread
	// safetly, but don't actually initialize Python yet.
	g_lockMain = PR_NewLock();
	return; // PR_TRUE;
}

void pyxpcom_destruct(void)
{
	PR_DestroyLock(g_lockMain);
}

// Yet another attempt at cross-platform library initialization and finalization.
struct DllInitializer {
	DllInitializer() {
		pyxpcom_construct();
	}
	~DllInitializer() {
		pyxpcom_destruct();
	}
} dll_initializer;
