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
#include "nsReadableUtils.h"
#include <nsIConsoleService.h>
#include "nspr.h" // PR_fprintf

static char *PyTraceback_AsString(PyObject *exc_tb);

// The internal helper that actually moves the
// formatted string to the target!

void LogMessage(const char *prefix, const char *pszMessageText)
{
	nsCOMPtr<nsIConsoleService> consoleService = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
	NS_ABORT_IF_FALSE(consoleService, "Where is the console service?");
	if (consoleService)
		consoleService->LogStringMessage(NS_ConvertASCIItoUCS2(pszMessageText).get());
	else
		PR_fprintf(PR_STDERR,"%s\n", pszMessageText);
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

void PyXPCOM_LogError(const char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	VLogF("PyXPCOM Error: ", fmt, marker);
	// If we have a Python exception, also log that:
	PyObject *exc_typ = NULL, *exc_val = NULL, *exc_tb = NULL;
	PyErr_Fetch( &exc_typ, &exc_val, &exc_tb);
	if (exc_typ) {
		PyErr_NormalizeException( &exc_typ, &exc_val, &exc_tb);
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
			streamout += "Can't convert exception to a string!";
		streamout += ": ";
		if (exc_val != NULL) {
			temp = PyObject_Str(exc_val);
			if (temp) {
				streamout += PyString_AsString(temp);
				Py_DECREF(temp);
			} else
				streamout += "Can't convert exception value to a string!";
		}
		streamout += "\n";
		LogMessage("PyXPCOM Exception:", streamout);
	}
	PyErr_Restore(exc_typ, exc_val, exc_tb);
}

void PyXPCOM_LogWarning(const char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	VLogF("PyXPCOM Warning: ", fmt, marker);
}

#ifdef DEBUG
void PyXPCOM_LogDebug(const char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	VLogF("PyXPCOM Debug: ", fmt, marker);
}
#endif


PyObject *PyXPCOM_BuildPyException(nsresult r)
{
	// Need the message etc.
	PyObject *evalue = Py_BuildValue("i", r);
	PyErr_SetObject(PyXPCOM_Error, evalue);
	Py_XDECREF(evalue);
	return NULL;
}

nsresult PyXPCOM_SetCOMErrorFromPyException()
{
	if (!PyErr_Occurred())
		// No error occurred
		return NS_OK;
	return NS_ERROR_FAILURE;
}

/* Obtains a string from a Python traceback.
   This is the exact same string as "traceback.print_exc" would return.

   Pass in a Python traceback object (probably obtained from PyErr_Fetch())
   Result is a string which must be free'd using PyMem_Free()
*/
#define TRACEBACK_FETCH_ERROR(what) {errMsg = what; goto done;}

char *PyTraceback_AsString(PyObject *exc_tb)
{
	char *errMsg = NULL; /* holds a local error message */
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
		TRACEBACK_FETCH_ERROR("memory error duplicating the traceback string\n");

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

// See comments in PyXPCOM.h for why we need this!
void PyXPCOM_MakePendingCalls()
{
	while (1) {
		int rc = Py_MakePendingCalls();
		if (rc == 0)
			break;
		// An exception - just report it as normal.
		// Note that a traceback is very unlikely!
		PyXPCOM_LogError("Unhandled exception detected before entering Python.\n");
		PyErr_Clear();
		// And loop around again until we are told everything is done!
	}
}
