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
#include <nsIServiceManager.h>


static nsIServiceManager *GetI(PyObject *self) {
	nsIID iid = NS_GET_IID(nsIServiceManager);

	if (!Py_nsISupports::Check(self, iid)) {
		PyErr_SetString(PyExc_TypeError, "This object is not the correct interface");
		return NULL;
	}
	return (nsIServiceManager *)Py_nsISupports::GetI(self);
}

static PyObject *PyRegisterService(PyObject *self, PyObject *args)
{
	nsIServiceManager *pI = GetI(self);
	if (pI==NULL)
		return NULL;
	PyObject *obCID, *obInterface;
	if (!PyArg_ParseTuple(args, "OO", &obCID, &obInterface))
		return NULL;

	nsCOMPtr<nsISupports> pis;
	if (!Py_nsISupports::InterfaceFromPyObject(obInterface, NS_GET_IID(nsISupports), getter_AddRefs(pis), PR_FALSE))
		return NULL;
	nsresult r;
	if (PyString_Check(obCID)) {
		const char *val = PyString_AsString(obCID);
		Py_BEGIN_ALLOW_THREADS;
		r = pI->RegisterService(val, pis);
		Py_END_ALLOW_THREADS;
	} else {
		nsCID cid;
		if (!Py_nsIID::IIDFromPyObject(obCID, &cid))
			return NULL;
		Py_BEGIN_ALLOW_THREADS;
		r = pI->RegisterService(cid, pis);
		Py_END_ALLOW_THREADS;
	}
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *PyGetService(PyObject *self, PyObject *args)
{
	nsIServiceManager *pI = GetI(self);
	if (pI==NULL)
		return NULL;
	PyObject *obIID, *obCID;
	if (!PyArg_ParseTuple(args, "OO", &obCID, &obIID))
		return NULL;
	nsIID	cid, iid;
	if (!Py_nsIID::IIDFromPyObject(obIID, &iid))
		return NULL;

	nsISupports *pis;
	nsresult r;
	if (PyString_Check(obCID)) {
		char *val = PyString_AsString(obCID);
		Py_BEGIN_ALLOW_THREADS;
		r = pI->GetService(val, iid, &pis);
		Py_END_ALLOW_THREADS;
	} else {
		if (!Py_nsIID::IIDFromPyObject(obCID, &cid))
			return NULL;
		Py_BEGIN_ALLOW_THREADS;
		r = pI->GetService(cid, iid, &pis);
		Py_END_ALLOW_THREADS;
	}
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	/* Return a type based on the IID (with no extra ref) */
	return Py_nsISupports::PyObjectFromInterface(pis, iid, PR_FALSE);
}

struct PyMethodDef 
PyMethods_IServiceManager[] =
{
	{ "GetService", PyGetService, 1},
	{ "getService", PyGetService, 1},
	{ "RegisterService", PyRegisterService, 1},
	{ "registerService", PyRegisterService, 1},
	{NULL}
};
