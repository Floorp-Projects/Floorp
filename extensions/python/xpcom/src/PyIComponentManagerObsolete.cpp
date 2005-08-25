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

static nsIComponentManagerObsolete *GetI(PyObject *self) {
	static const nsIID iid = NS_GET_IID(nsIComponentManagerObsolete);

	if (!Py_nsISupports::Check(self, iid)) {
		PyErr_SetString(PyExc_TypeError, "This object is not the correct interface");
		return NULL;
	}
	return (nsIComponentManagerObsolete *)Py_nsISupports::GetI(self);
}

static PyObject *PyCreateInstanceByContractID(PyObject *self, PyObject *args)
{
	char *pid, *notyet = NULL;
	PyObject *obIID = NULL;
	if (!PyArg_ParseTuple(args, "s|zO", &pid, &notyet, &obIID))
		return NULL;
	if (notyet != NULL) {
		PyErr_SetString(PyExc_ValueError, "2nd arg must be none");
		return NULL;
	}
	nsIComponentManagerObsolete *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsIID	iid;
	if (obIID==NULL)
		iid = NS_GET_IID(nsISupports);
	else
		if (!Py_nsIID::IIDFromPyObject(obIID, &iid))
			return NULL;

	nsISupports *pis;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->CreateInstanceByContractID(pid, NULL, iid, (void **)&pis);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	/* Return a type based on the IID (with no extra ref) */
	return Py_nsISupports::PyObjectFromInterface(pis, iid, PR_FALSE, PR_FALSE);
}

static PyObject *PyContractIDToClassID(PyObject *self, PyObject *args)
{
	char *pid;
	if (!PyArg_ParseTuple(args, "s", &pid))
		return NULL;
	nsIComponentManagerObsolete *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsIID	iid;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->ContractIDToClassID(pid, &iid);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	return Py_nsIID::PyObjectFromIID(iid);
}

static PyObject *PyCLSIDToContractID(PyObject *self, PyObject *args)
{
	PyObject *obIID;
	if (!PyArg_ParseTuple(args, "O", &obIID))
		return NULL;

	nsIID iid;
	if (!Py_nsIID::IIDFromPyObject(obIID, &iid))
		return NULL;
	char *ret_pid = nsnull;
	char *ret_class = nsnull;
	nsIComponentManagerObsolete *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->CLSIDToContractID(iid, &ret_class, &ret_pid);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	PyObject *ob_pid = PyString_FromString(ret_pid);
	PyObject *ob_class = PyString_FromString(ret_class);
	PyObject *ret = Py_BuildValue("OO", ob_pid, ob_class);
	nsMemory::Free(ret_pid);
	nsMemory::Free(ret_class);
	Py_XDECREF(ob_pid);
	Py_XDECREF(ob_class);
	return ret;
}

static PyObject *PyEnumerateCLSIDs(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	nsIComponentManagerObsolete *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsIEnumerator *pRet;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->EnumerateCLSIDs(&pRet);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	return Py_nsISupports::PyObjectFromInterface(pRet, NS_GET_IID(nsIEnumerator), PR_FALSE);
}

static PyObject *PyEnumerateContractIDs(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;

	nsIComponentManagerObsolete *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsIEnumerator *pRet;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->EnumerateContractIDs(&pRet);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	return Py_nsISupports::PyObjectFromInterface(pRet, NS_GET_IID(nsIEnumerator), PR_FALSE);
}

struct PyMethodDef 
PyMethods_IComponentManagerObsolete[] =
{
	{ "CreateInstanceByContractID", PyCreateInstanceByContractID, 1},
	{ "createInstanceByContractID", PyCreateInstanceByContractID, 1},
	{ "EnumerateCLSIDs",        PyEnumerateCLSIDs, 1},
	{ "enumerateCLSIDs",        PyEnumerateCLSIDs, 1},
	{ "EnumerateContractIDs",       PyEnumerateContractIDs, 1},
	{ "enumerateContractIDs",       PyEnumerateContractIDs, 1},
	{ "ContractIDToClassID",        PyContractIDToClassID, 1},
	{ "contractIDToClassID",        PyContractIDToClassID, 1},
	{ "CLSIDToContractID",          PyCLSIDToContractID, 1},
	{NULL}
};
