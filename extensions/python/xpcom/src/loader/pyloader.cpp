/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
 * the specific language governing rights and limitations under the License.
 *
 * The Original Code is the Python XPCOM language bindings.
 *
 * The Initial Developer of the Original Code is ActiveState Tool Corp.
 * Portions created by ActiveState Tool Corp. are Copyright (C) 2000, 2001
 * ActiveState Tool Corp.  All Rights Reserved.
 *
 * Contributor(s): Mark Hammond <mhammond@skippinet.com.au> (original author)
 *
 */

// pyloader
//
// Not part of the main Python _xpcom package, but a separate, thin DLL.
//
// The main loader and registrar for Python.  A thin DLL that is designed to live in
// the xpcom "components" directory.  Simply locates and loads the standard
// _xpcom support module and transfers control to that.

#include "nsIComponentLoader.h"
#include "nsIRegistry.h"
#include "nsISupports.h"
#include "nsIModule.h"
#include "nsDirectoryServiceDefs.h"
#include "nsILocalFile.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "stdlib.h"
#include "stdarg.h"

#include "nsReadableUtils.h"
#include "nsCRT.h"
#include <nsFileStream.h> // For console logging.
#include "nspr.h" // PR_fprintf

#ifdef HAVE_LONG_LONG
#undef HAVE_LONG_LONG
#endif

#include "Python.h"

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


pfnPyXPCOM_NSGetModule pfnEntryPoint = nsnull;


static void LogError(const char *fmt, ...);
static void LogDebug(const char *fmt, ...);

// Ensure that any paths guaranteed by this package exist on sys.path
// Only called once as we are first loaded into the process.
void AddStandardPaths()
{
	// Put {bin}\Python on the path if it exists.
	nsCOMPtr<nsIFile> aFile;
	nsCOMPtr<nsILocalFile> aLocalFile;
	NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, getter_AddRefs(aFile));
	if (!aFile) {
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
		AddStandardPaths();
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
	if (pfnEntryPoint == nsnull) {
		PyObject *mod = PyImport_ImportModule("xpcom._xpcom");
		if (mod==NULL) {
			LogError("Could not import the Python XPCOM extension\n");
			return NS_ERROR_FAILURE;
		}
		PyObject *obpfn = PyObject_GetAttrString(mod, "_NSGetModule_FuncPtr");
		void *pfn = NULL;
		if (obpfn) {
			NS_ABORT_IF_FALSE(PyLong_Check(obpfn)||PyInt_Check(obpfn), "xpcom._NSGetModule_FuncPtr is not a long!");
			pfn = PyLong_AsVoidPtr(obpfn);
		}
		pfnEntryPoint = (pfnPyXPCOM_NSGetModule)pfn;
	}
	if (pfnEntryPoint==NULL) {
		LogError("Could not load main Python entry point\n");
		return NS_ERROR_FAILURE;
	}

#ifdef MOZ_TIMELINE
	// If the timeline service is installed, see if we can install our hooks.
	if (NULL==PyImport_ImportModule("timeline_hook")) {
		if (!PyErr_ExceptionMatches(PyExc_ImportError))
			LogError("Failed to import 'timeline_hook'");
		PyErr_Clear(); // but don't care if we can't.
	}
#endif
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
	nsresult rc = (*pfnEntryPoint)(servMgr, location, result);
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
	char *c = ToNewCString(text);
	LogMessage(prefix, c);
	nsCRT::free(c);
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
