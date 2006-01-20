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

#include "nsDirectoryServiceDefs.h"
#include "nsILocalFile.h"

#include "nspr.h" // PR_fprintf

#if (PY_VERSION_HEX >= 0x02030000)
#define PYXPCOM_USE_PYGILSTATE
#endif

static char *PyTraceback_AsString(PyObject *exc_tb);

#ifdef XP_WIN
#include "windows.h"
#endif

#ifdef XP_UNIX
#include <dlfcn.h>
#include <sys/stat.h>

#endif

#include "nsITimelineService.h"

typedef nsresult (*pfnPyXPCOM_NSGetModule)(nsIComponentManager *servMgr,
                                          nsIFile* location,
                                          nsIModule** result);


static void LogError(const char *fmt, ...);
static void LogDebug(const char *fmt, ...);

// Ensure that any paths guaranteed by this package exist on sys.path
// Only called once as we are first loaded into the process.
void AddStandardPaths()
{
	// Put {bin}\Python on the path if it exists.
	nsresult rv;
	nsCOMPtr<nsIFile> aFile;
	rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, getter_AddRefs(aFile));
	if (NS_FAILED(rv)) {
		LogError("The Python XPCOM loader could not locate the 'bin' directory\n");
		return;
	}
	aFile->Append(NS_LITERAL_STRING("python"));
	nsAutoString pathBuf;
	aFile->GetPath(pathBuf);
	PyObject *obPath = PySys_GetObject("path");
	if (!obPath) {
		LogError("The Python XPCOM loader could not get the Python sys.path variable\n");
		return;
	}
	NS_LossyConvertUCS2toASCII pathCBuf(pathBuf);
	LogDebug("The Python XPCOM loader is adding '%s' to sys.path\n", pathCBuf.get());
	PyObject *newStr = PyString_FromString(pathCBuf.get());
	PyList_Insert(obPath, 0, newStr);
	Py_XDECREF(newStr);
	// And now try and get Python to process this directory as a "site dir" 
	// - ie, look for .pth files, etc
	nsCAutoString cmdBuf(NS_LITERAL_CSTRING("import site;site.addsitedir(r'") + pathCBuf + NS_LITERAL_CSTRING("')\n"));
	if (0 != PyRun_SimpleString((char *)cmdBuf.get())) {
		LogError("The directory '%s' could not be added as a site directory", pathCBuf.get());
		PyErr_Clear();
	}
	// and somewhat like Python itself (site, citecustomize), we attempt 
	// to import "sitepyxpcom" ignoring ImportError
	if (NULL==PyImport_ImportModule("sitepyxpcom")) {
		if (!PyErr_ExceptionMatches(PyExc_ImportError))
			LogError("Failed to import 'sitepyxpcom'");
		PyErr_Clear();
	}
}

////////////////////////////////////////////////////////////
// This is the main entry point that delegates into Python
nsresult PyXPCOM_NSGetModule(nsIComponentManager *servMgr,
                             nsIFile* location,
                             nsIModule** result)
{
	NS_PRECONDITION(result!=NULL, "null result pointer in PyXPCOM_NSGetModule!");
	NS_PRECONDITION(location!=NULL, "null nsIFile pointer in PyXPCOM_NSGetModule!");
	NS_PRECONDITION(servMgr!=NULL, "null servMgr pointer in PyXPCOM_NSGetModule!");
#ifndef LOADER_LINKS_WITH_PYTHON
	if (!Py_IsInitialized()) {
		Py_Initialize();
		if (!Py_IsInitialized()) {
			PyXPCOM_LogError("Python initialization failed!\n");
			return NS_ERROR_FAILURE;
		}
		PyEval_InitThreads();
#ifndef PYXPCOM_USE_PYGILSTATE
		PyXPCOM_InterpreterState_Ensure();
#endif
		PyEval_SaveThread();
	}
#endif // LOADER_LINKS_WITH_PYTHON	
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
#ifdef XP_UNIX
	// *sob* - seems necessary to open the .so as RTLD_GLOBAL
	dlopen(PYTHON_SO,RTLD_NOW | RTLD_GLOBAL);
#endif
	PRBool bDidInitPython = !Py_IsInitialized(); // well, I will next line, anyway :-)
	if (bDidInitPython) {
		NS_TIMELINE_START_TIMER("PyXPCOM: Python initializing");
		Py_Initialize();
		if (!Py_IsInitialized()) {
			LogError("Python initialization failed!\n");
			return NS_ERROR_FAILURE;
		}
#ifndef NS_DEBUG
		Py_OptimizeFlag = 1;
#endif // NS_DEBUG
		PyEval_InitThreads();
		NS_TIMELINE_STOP_TIMER("PyXPCOM: Python initializing");
		NS_TIMELINE_MARK_TIMER("PyXPCOM: Python initializing");
	}
	// Get the Python interpreter state
	NS_TIMELINE_START_TIMER("PyXPCOM: Python threadstate setup");
#ifndef PYXPCOM_USE_PYGILSTATE
	PyThreadState *threadStateCreated = NULL;
	PyThreadState *threadState = PyThreadState_Swap(NULL);
	if (threadState==NULL) {
		// no thread-state - set one up.
		// *sigh* - what I consider a bug is that Python
		// will deadlock unless we own the lock before creating
		// a new interpreter (it appear Py_NewInterpreter has
		// really only been tested/used with no thread lock
		PyEval_AcquireLock();
		threadState = threadStateCreated = Py_NewInterpreter();
		PyThreadState_Swap(NULL);
	}
	PyEval_ReleaseLock();
	PyEval_AcquireThread(threadState);
#else
	PyGILState_STATE state = PyGILState_Ensure();
#endif // PYXPCOM_USE_PYGILSTATE
#ifdef MOZ_TIMELINE
	// If the timeline service is installed, see if we can install our hooks.
	if (NULL==PyImport_ImportModule("timeline_hook")) {
		if (!PyErr_ExceptionMatches(PyExc_ImportError))
			LogError("Failed to import 'timeline_hook'");
		PyErr_Clear(); // but don't care if we can't.
	}
#endif
	// Add the standard paths always - we may not have been the first to
	// init Python.
	AddStandardPaths();

#ifndef PYXPCOM_USE_PYGILSTATE
	// Abandon the thread-lock, as the first thing Python does
	// is re-establish the lock (the Python thread-state story SUCKS!!!)
	if (threadStateCreated) {
		Py_EndInterpreter(threadStateCreated);
		PyEval_ReleaseLock(); // see Py_NewInterpreter call above 
	} else {
		PyEval_ReleaseThread(threadState);
		PyThreadState *threadStateSave = PyThreadState_Swap(NULL);
		if (threadStateSave)
			PyThreadState_Delete(threadStateSave);
	}
#else
	// If we initialized Python, then we will also have acquired the thread
	// lock.  In that case, we want to leave it unlocked, so other threads
	// are free to run, even if they aren't running Python code.
	PyGILState_Release(bDidInitPython ? PyGILState_UNLOCKED : state);
#endif 

	NS_TIMELINE_STOP_TIMER("PyXPCOM: Python threadstate setup");
	NS_TIMELINE_MARK_TIMER("PyXPCOM: Python threadstate setup");
	NS_TIMELINE_START_TIMER("PyXPCOM: PyXPCOM NSGetModule entry point");
	nsresult rc = PyXPCOM_NSGetModule(servMgr, location, result);
	NS_TIMELINE_STOP_TIMER("PyXPCOM: PyXPCOM NSGetModule entry point");
	NS_TIMELINE_MARK_TIMER("PyXPCOM: PyXPCOM NSGetModule entry point");
	return rc;
}

// The internal helper that actually moves the
// formatted string to the target!

void LogMessage(const char *prefix, const char *pszMessageText)
{
	PR_fprintf(PR_STDERR, "%s", pszMessageText);
}

void LogMessage(const char *prefix, nsACString &text)
{
	LogMessage(prefix, nsPromiseFlatCString(text).get());
}

// A helper for the various logging routines.
static void VLogF(const char *prefix, const char *fmt, va_list argptr)
{
	char buff[512];

	vsprintf(buff, fmt, argptr);

	LogMessage(prefix, buff);
}

static void LogError(const char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	VLogF("PyXPCOM Loader Error: ", fmt, marker);
	// If we have a Python exception, also log that:
	PyObject *exc_typ = NULL, *exc_val = NULL, *exc_tb = NULL;
	PyErr_Fetch( &exc_typ, &exc_val, &exc_tb);
	if (exc_typ) {
		nsCAutoString streamout;

		if (exc_tb) {
			const char *szTraceback = PyTraceback_AsString(exc_tb);
			if (szTraceback == NULL)
				streamout += "Can't get the traceback info!";
			else {
				streamout += "Traceback (most recent call last):\n";
				streamout += szTraceback;
				PyMem_Free((void *)szTraceback);
			}
		}
		PyObject *temp = PyObject_Str(exc_typ);
		if (temp) {
			streamout += PyString_AsString(temp);
			Py_DECREF(temp);
		} else
			streamout += "Can convert exception to a string!";
		streamout += ": ";
		if (exc_val != NULL) {
			temp = PyObject_Str(exc_val);
			if (temp) {
				streamout += PyString_AsString(temp);
				Py_DECREF(temp);
			} else
				streamout += "Can convert exception value to a string!";
		}
		streamout += "\n";
		LogMessage("PyXPCOM Exception:", streamout);
	}
	PyErr_Restore(exc_typ, exc_val, exc_tb);
}
/*** - not currently used - silence compiler warning.
static void LogWarning(const char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	VLogF("PyXPCOM Loader Warning: ", fmt, marker);
}
***/
#ifdef DEBUG
static void LogDebug(const char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	VLogF("PyXPCOM Loader Debug: ", fmt, marker);
}
#else
static void LogDebug(const char *fmt, ...)
{
}
#endif

/* Obtains a string from a Python traceback.
   This is the exact same string as "traceback.print_exc" would return.

   Pass in a Python traceback object (probably obtained from PyErr_Fetch())
   Result is a string which must be free'd using PyMem_Free()
*/
#define TRACEBACK_FETCH_ERROR(what) {errMsg = what; goto done;}

char *PyTraceback_AsString(PyObject *exc_tb)
{
	char *errMsg = NULL; /* a static that hold a local error message */
	char *result = NULL; /* a valid, allocated result. */
	PyObject *modStringIO = NULL;
	PyObject *modTB = NULL;
	PyObject *obFuncStringIO = NULL;
	PyObject *obStringIO = NULL;
	PyObject *obFuncTB = NULL;
	PyObject *argsTB = NULL;
	PyObject *obResult = NULL;

	/* Import the modules we need - cStringIO and traceback */
	modStringIO = PyImport_ImportModule("cStringIO");
	if (modStringIO==NULL)
		TRACEBACK_FETCH_ERROR("cant import cStringIO\n");

	modTB = PyImport_ImportModule("traceback");
	if (modTB==NULL)
		TRACEBACK_FETCH_ERROR("cant import traceback\n");
	/* Construct a cStringIO object */
	obFuncStringIO = PyObject_GetAttrString(modStringIO, "StringIO");
	if (obFuncStringIO==NULL)
		TRACEBACK_FETCH_ERROR("cant find cStringIO.StringIO\n");
	obStringIO = PyObject_CallObject(obFuncStringIO, NULL);
	if (obStringIO==NULL)
		TRACEBACK_FETCH_ERROR("cStringIO.StringIO() failed\n");
	/* Get the traceback.print_exception function, and call it. */
	obFuncTB = PyObject_GetAttrString(modTB, "print_tb");
	if (obFuncTB==NULL)
		TRACEBACK_FETCH_ERROR("cant find traceback.print_tb\n");

	argsTB = Py_BuildValue("OOO", 
			exc_tb  ? exc_tb  : Py_None,
			Py_None, 
			obStringIO);
	if (argsTB==NULL) 
		TRACEBACK_FETCH_ERROR("cant make print_tb arguments\n");

	obResult = PyObject_CallObject(obFuncTB, argsTB);
	if (obResult==NULL) 
		TRACEBACK_FETCH_ERROR("traceback.print_tb() failed\n");
	/* Now call the getvalue() method in the StringIO instance */
	Py_DECREF(obFuncStringIO);
	obFuncStringIO = PyObject_GetAttrString(obStringIO, "getvalue");
	if (obFuncStringIO==NULL)
		TRACEBACK_FETCH_ERROR("cant find getvalue function\n");
	Py_DECREF(obResult);
	obResult = PyObject_CallObject(obFuncStringIO, NULL);
	if (obResult==NULL) 
		TRACEBACK_FETCH_ERROR("getvalue() failed.\n");

	/* And it should be a string all ready to go - duplicate it. */
	if (!PyString_Check(obResult))
			TRACEBACK_FETCH_ERROR("getvalue() did not return a string\n");

	{ // a temp scope so I can use temp locals.
	char *tempResult = PyString_AsString(obResult);
	result = (char *)PyMem_Malloc(strlen(tempResult)+1);
	if (result==NULL)
		TRACEBACK_FETCH_ERROR("memory error duplicating the traceback string");

	strcpy(result, tempResult);
	} // end of temp scope.
done:
	/* All finished - first see if we encountered an error */
	if (result==NULL && errMsg != NULL) {
		result = (char *)PyMem_Malloc(strlen(errMsg)+1);
		if (result != NULL)
			/* if it does, not much we can do! */
			strcpy(result, errMsg);
	}
	Py_XDECREF(modStringIO);
	Py_XDECREF(modTB);
	Py_XDECREF(obFuncStringIO);
	Py_XDECREF(obStringIO);
	Py_XDECREF(obFuncTB);
	Py_XDECREF(argsTB);
	Py_XDECREF(obResult);
	return result;
}
