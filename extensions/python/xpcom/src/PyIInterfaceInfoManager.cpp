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
#include <nsIInterfaceInfoManager.h>

static nsIInterfaceInfoManager *GetI(PyObject *self) {
	nsIID iid = NS_GET_IID(nsIInterfaceInfoManager);

	if (!Py_nsISupports::Check(self, iid)) {
		PyErr_SetString(PyExc_TypeError, "This object is not the correct interface");
		return NULL;
	}
	return (nsIInterfaceInfoManager *)Py_nsISupports::GetI(self);
}

static PyObject *PyGetInfoForIID(PyObject *self, PyObject *args)
{
	PyObject *obIID = NULL;
	if (!PyArg_ParseTuple(args, "O", &obIID))
		return NULL;

	nsIInterfaceInfoManager *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsIID iid;
	if (!Py_nsIID::IIDFromPyObject(obIID, &iid))
		return NULL;

	nsIInterfaceInfo *pi;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetInfoForIID(&iid, &pi);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	/* Return a type based on the IID (with no extra ref) */
	nsIID new_iid = NS_GET_IID(nsIInterfaceInfo);
	// Can not auto-wrap the interface info manager as it is critical to
	// building the support we need for autowrap.
	return Py_nsISupports::PyObjectFromInterface(pi, new_iid, PR_FALSE, PR_FALSE);
}

static PyObject *PyGetInfoForName(PyObject *self, PyObject *args)
{
	char *name;
	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	nsIInterfaceInfoManager *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsIInterfaceInfo *pi;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetInfoForName(name, &pi);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	/* Return a type based on the IID (with no extra ref) */
	// Can not auto-wrap the interface info manager as it is critical to
	// building the support we need for autowrap.
	return Py_nsISupports::PyObjectFromInterface(pi, NS_GET_IID(nsIInterfaceInfo), PR_FALSE, PR_FALSE);
}

static PyObject *PyGetNameForIID(PyObject *self, PyObject *args)
{
	PyObject *obIID = NULL;
	if (!PyArg_ParseTuple(args, "O", &obIID))
		return NULL;

	nsIInterfaceInfoManager *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsIID iid;
	if (!Py_nsIID::IIDFromPyObject(obIID, &iid))
		return NULL;

	char *ret_name = NULL;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetNameForIID(&iid, &ret_name);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	PyObject *ret = PyString_FromString(ret_name);
	nsMemory::Free(ret_name);
	return ret;
}

static PyObject *PyGetIIDForName(PyObject *self, PyObject *args)
{
	char *name;
	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	nsIInterfaceInfoManager *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsIID *iid_ret;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetIIDForName(name, &iid_ret);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	PyObject *ret = Py_nsIID::PyObjectFromIID(*iid_ret);
	nsMemory::Free(iid_ret);
	return ret;
}

static PyObject *PyEnumerateInterfaces(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	nsIInterfaceInfoManager *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsIEnumerator *pRet;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->EnumerateInterfaces(&pRet);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	return Py_nsISupports::PyObjectFromInterface(pRet, NS_GET_IID(nsIEnumerator), PR_FALSE);
}

// TODO:
// void autoRegisterInterfaces();

PyMethodDef 
PyMethods_IInterfaceInfoManager[] =
{
	{ "GetInfoForIID", PyGetInfoForIID, 1},
	{ "getInfoForIID", PyGetInfoForIID, 1},
	{ "GetInfoForName", PyGetInfoForName, 1},
	{ "getInfoForName", PyGetInfoForName, 1},
	{ "GetIIDForName", PyGetIIDForName, 1},
	{ "getIIDForName", PyGetIIDForName, 1},
	{ "GetNameForIID", PyGetNameForIID, 1},
	{ "getNameForIID", PyGetNameForIID, 1},
	{ "EnumerateInterfaces", PyEnumerateInterfaces, 1},
	{ "enumerateInterfaces", PyEnumerateInterfaces, 1},
	{NULL}
};
