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
 * Contributor(s): Mark Hammond <MarkH@ActiveState.com> (original author)
 *
 */

// pyloader
//
// Not part of the main Python _xpcom package, but a seperate, thin DLL.
//
// The main loader and registrar for Python.  A thin DLL that is designed to live in
// the xpcom "components" directory.  Simply locates and loads the standard
// _xpcom support module and transfers control to that.

#include "xp_core.h"
#include "nsIComponentLoader.h"
#include "nsIRegistry.h"
#include "nsISupports.h"
#include "nsIModule.h"
#include "nsDirectoryServiceDefs.h"
#include "nsILocalFile.h"
#include "nsXPIDLString.h"
 
#include <nsFileStream.h> // For console logging.

#ifdef HAVE_LONG_LONG
#undef HAVE_LONG_LONG
#endif


#ifdef XP_WIN
// Can only assume dynamic loading on Windows.
#define LOADER_LINKS_WITH_PYTHON
#endif


#ifdef LOADER_LINKS_WITH_PYTHON	
#include "Python.h"

static PyThreadState *ptsGlobal = nsnull;
static char *PyTraceback_AsString(PyObject *exc_tb);

#else // LOADER_LINKS_WITH_PYTHON	

static PRBool find_xpcom_module(char *buf, size_t bufsize);

#endif // LOADER_LINKS_WITH_PYTHON	


#ifdef XP_WIN
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

#ifdef XP_UNIX
#include <dlfcn.h>
#include <sys/stat.h>

#endif


typedef nsresult (*pfnPyXPCOM_NSGetModule)(nsIComponentManager *servMgr,
                                          nsIFile* location,
                                          nsIModule** result);


pfnPyXPCOM_NSGetModule pfnEntryPoint = nsnull;


static void LogError(const char *fmt, ...);
static void LogDebug(const char *fmt, ...);

#ifdef LOADER_LINKS_WITH_PYTHON
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
	aFile->Append("python");
	nsXPIDLCString pathBuf;
	aFile->GetPath(getter_Copies(pathBuf));
	PyObject *obPath = PySys_GetObject("path");
	if (!obPath) {
		LogError("The Python XPCOM loader could not get the Python sys.path variable\n");
		return;
	}
    LogDebug("The Python XPCOM loader is adding '%s' to sys.path\n", (const char *)pathBuf);
//    DebugBreak();
	PyObject *newStr = PyString_FromString(pathBuf);
	PyList_Insert(obPath, 0, newStr);
	Py_XDECREF(newStr);
}
#endif

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFile* location,
                                          nsIModule** result)
{
	// What to do for other platforms here?
	// I tried using their nsDll class, but it wont allow
	// a LoadLibrary() - it insists on a full path it can load.
	// So if Im going to the trouble of locating the DLL on Windows,
	// I may as well just do the whole thing myself.
#ifdef LOADER_LINKS_WITH_PYTHON	
	PRBool bDidInitPython = !Py_IsInitialized(); // well, I will next line, anyway :-)
	if (bDidInitPython) {
		Py_Initialize();
		if (!Py_IsInitialized()) {
			LogError("Python initialization failed!\n");
			return NS_ERROR_FAILURE;
		}
		AddStandardPaths();
		PyObject *mod = PyImport_ImportModule("xpcom._xpcom");
		if (mod==NULL) {
			LogError("Could not import the Python XPCOM extension\n");
			return NS_ERROR_FAILURE;
		}
	}
#endif // LOADER_LINKS_WITH_PYTHON	
	if (pfnEntryPoint == nsnull) {

#ifdef XP_WIN

#ifdef DEBUG
		const char *mod_name = "_xpcom_d.pyd";
#else
		const char *mod_name = "_xpcom.pyd";
#endif
		HMODULE hmod = GetModuleHandle(mod_name);
		if (hmod==NULL) {
			LogError("Could not get a handle to the Python XPCOM extension\n");
			return NS_ERROR_FAILURE;
		}
		pfnEntryPoint = (pfnPyXPCOM_NSGetModule)GetProcAddress(hmod, "PyXPCOM_NSGetModule");
#endif // XP_WIN

#ifdef XP_UNIX
		static char module_path[1024];
		if (!find_xpcom_module(module_path, sizeof(module_path)))
			return NS_ERROR_FAILURE;

		void *handle = dlopen(module_path, RTLD_GLOBAL | RTLD_LAZY);
		if (handle==NULL) {
			LogError("Could not open the Python XPCOM extension at '%s' - '%s'\n", module_path, dlerror());
			return NS_ERROR_FAILURE;
		}
		pfnEntryPoint = (pfnPyXPCOM_NSGetModule)dlsym(handle, "PyXPCOM_NSGetModule");
#endif // XP_UNIX
	}
	if (pfnEntryPoint==NULL) {
		LogError("Could not load main Python entry point\n");
		return NS_ERROR_FAILURE;
	}
	
#ifdef LOADER_LINKS_WITH_PYTHON	
	// We abandon the thread-lock, as the first thing Python does
	// is re-establish the lock (the Python thread-state story SUCKS!!!

	if (bDidInitPython)
		ptsGlobal = PyEval_SaveThread();
	// Note this is never restored, and Python is never finalized!
#endif // LOADER_LINKS_WITH_PYTHON	
	return (*pfnEntryPoint)(servMgr, location, result);
}

// The internal helper that actually moves the
// formatted string to the target!

void LogMessage(const char *prefix, const char *pszMessageText)
{
	nsOutputConsoleStream console;
	console << prefix << pszMessageText;
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
#ifdef LOADER_LINKS_WITH_PYTHON	
	// If we have a Python exception, also log that:
	PyObject *exc_typ = NULL, *exc_val = NULL, *exc_tb = NULL;
	PyErr_Fetch( &exc_typ, &exc_val, &exc_tb);
	if (exc_typ) {
		char *string1 = nsnull;
	        nsOutputStringStream streamout(string1);

		if (exc_tb) {
			const char *szTraceback = PyTraceback_AsString(exc_tb);
			if (szTraceback == NULL)
				streamout << "Can't get the traceback info!";
			else {
				streamout << "Traceback (most recent call last):\n";
				streamout << szTraceback;
				PyMem_Free((ANY *)szTraceback);
			}
		}
		PyObject *temp = PyObject_Str(exc_typ);
		if (temp) {
			streamout << PyString_AsString(temp);
			Py_DECREF(temp);
		} else
			streamout << "Can convert exception to a string!";
		streamout << ": ";
		if (exc_val != NULL) {
			temp = PyObject_Str(exc_val);
			if (temp) {
				streamout << PyString_AsString(temp);
				Py_DECREF(temp);
			} else
				streamout << "Can convert exception value to a string!";
		}
		streamout << "\n";
		LogMessage("PyXPCOM Exception:", string1);
	}
	PyErr_Restore(exc_typ, exc_val, exc_tb);
#endif // LOADER_LINKS_WITH_PYTHON	
}

static void LogWarning(const char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	VLogF("PyXPCOM Loader Warning: ", fmt, marker);
}

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

#ifdef LOADER_LINKS_WITH_PYTHON	

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

#else // LOADER_LINKS_WITH_PYTHON	

#ifdef XP_UNIX

// From Python getpath.c
#ifndef S_ISREG
#define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISDIR
#define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#endif

static int
isfile(char *filename)		/* Is file, not directory */
{
    struct stat buf;
    if (stat(filename, &buf) != 0)
        return 0;
    if (!S_ISREG(buf.st_mode))
        return 0;
    return 1;
}


static int
isxfile(char *filename)		/* Is executable file */
{
    struct stat buf;
    if (stat(filename, &buf) != 0)
        return 0;
    if (!S_ISREG(buf.st_mode))
        return 0;
    if ((buf.st_mode & 0111) == 0)
        return 0;
    return 1;
}


static int
isdir(char *filename)			/* Is directory */
{
    struct stat buf;
    if (stat(filename, &buf) != 0)
        return 0;
    if (!S_ISDIR(buf.st_mode))
        return 0;
    return 1;
}


PRBool find_xpcom_module(char *buf, size_t bufsize)
{
	char *pypath = getenv("PYTHONPATH");
	char *searchPath = pypath ? strdup(pypath) : NULL;
	char *tok = searchPath ? strtok(searchPath, ":") : NULL;
	while (tok != NULL) {
		int thissize = bufsize;
		int baselen = strlen(tok);
		strncpy(buf, tok, thissize);
		thissize-=baselen;
		if (thissize > 1 && buf[baselen-1] != '/') {
			buf[baselen++]='/';
		}
		strncpy(buf+baselen, "xpcom/_xpcommodule.so", thissize);
//		LogDebug("Python _xpcom module at '%s'?\n", buf);
		if (isfile(buf)) {
//			LogDebug("Found python _xpcom module at '%s'\n", buf);
			return PR_TRUE;
		}
		tok = strtok(NULL, ":");
	}
	LogError("Failed to find a Python _xpcom module\n");
	return PR_FALSE;
}

#endif // XP_UNIX

#endif // LOADER_LINKS_WITH_PYTHON	

