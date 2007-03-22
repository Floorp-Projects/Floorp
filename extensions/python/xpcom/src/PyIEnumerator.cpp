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
 *   Mark Hammond <MarkH@ActiveState.com> (original author)
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
#include <nsIEnumerator.h>

static nsIEnumerator *GetI(PyObject *self) {
	nsIID iid = NS_GET_IID(nsIEnumerator);

	if (!Py_nsISupports::Check(self, iid)) {
		PyErr_SetString(PyExc_TypeError, "This object is not the correct interface");
		return NULL;
	}
	return (nsIEnumerator *)Py_nsISupports::GetI(self);
}

static PyObject *PyFirst(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":First"))
		return NULL;

	nsIEnumerator *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->First();
	Py_END_ALLOW_THREADS;
	return PyInt_FromLong(r);
}

static PyObject *PyNext(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":Next"))
		return NULL;

	nsIEnumerator *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->Next();
	Py_END_ALLOW_THREADS;
	return PyInt_FromLong(r);
}

static PyObject *PyCurrentItem(PyObject *self, PyObject *args)
{
	PyObject *obIID = NULL;
	if (!PyArg_ParseTuple(args, "|O:CurrentItem", &obIID))
		return NULL;

	nsIID iid(NS_GET_IID(nsISupports));
	if (obIID != NULL && !Py_nsIID::IIDFromPyObject(obIID, &iid))
		return NULL;
	nsIEnumerator *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsISupports *pRet = nsnull;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->CurrentItem(&pRet);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	if (obIID) {
		nsISupports *temp;
		Py_BEGIN_ALLOW_THREADS;
		r = pRet->QueryInterface(iid, (void **)&temp);
		pRet->Release();
		Py_END_ALLOW_THREADS;
		if ( NS_FAILED(r) ) {
			return PyXPCOM_BuildPyException(r);
		}
		pRet = temp;
	}
	PyObject *ret = Py_nsISupports::PyObjectFromInterface(pRet, iid);
	NS_IF_RELEASE(pRet);
	return ret;
}

// A method added for Python performance if you really need
// it.  Allows you to fetch a block of objects in one
// hit, allowing the loop to remain implemented in C.
static PyObject *PyFetchBlock(PyObject *self, PyObject *args)
{
	PyObject *obIID = NULL;
	int n_wanted;
	int n_fetched = 0;
	if (!PyArg_ParseTuple(args, "i|O:FetchBlock", &n_wanted, &obIID))
		return NULL;

	nsIID iid(NS_GET_IID(nsISupports));
	if (obIID != NULL && !Py_nsIID::IIDFromPyObject(obIID, &iid))
		return NULL;
	nsIEnumerator *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	// We want to fetch with the thread-lock released,
	// but this means we can not append to the PyList
	nsISupports **fetched = new nsISupports*[n_wanted];
	if (fetched==nsnull) {
		PyErr_NoMemory();
		return NULL;
	}
	memset(fetched, 0, sizeof(nsISupports *) * n_wanted);
	nsresult r = NS_OK;
	Py_BEGIN_ALLOW_THREADS;
	for (;n_fetched<n_wanted;) {
		nsISupports *pNew;
		r = pI->CurrentItem(&pNew);
		if (NS_FAILED(r)) {
			r = 0; // Normal enum end
			break;
		}
		if (obIID) {
			nsISupports *temp;
			r = pNew->QueryInterface(iid, (void **)&temp);
			pNew->Release();
			if ( NS_FAILED(r) ) {
				break;
			}
			pNew = temp;
		}
		fetched[n_fetched] = pNew;
		n_fetched++; // must increment before breaking out.
		if (NS_FAILED(pI->Next()))
			break; // not an error condition.
	}
	Py_END_ALLOW_THREADS;
	PyObject *ret;
	if (NS_SUCCEEDED(r)) {
		ret = PyList_New(n_fetched);
		if (ret)
			for (int i=0;i<n_fetched;i++) {
				PyObject *new_ob = Py_nsISupports::PyObjectFromInterface(fetched[i], iid);
				NS_IF_RELEASE(fetched[i]);
				PyList_SET_ITEM(ret, i, new_ob);
			}
	} else
		ret = PyXPCOM_BuildPyException(r);

	if ( ret == NULL ) {
		// Free the objects we consumed.
		for (int i=0;i<n_fetched;i++)
			fetched[i]->Release();

	}
	delete [] fetched;
	return ret;
}

static PyObject *PyIsDone(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":IsDone"))
		return NULL;

	nsIEnumerator *pI = GetI(self);
	nsresult r;
	if (pI==NULL)
		return NULL;

	Py_BEGIN_ALLOW_THREADS;
	r = pI->IsDone();
	Py_END_ALLOW_THREADS;
	if (NS_FAILED(r))
		return PyXPCOM_BuildPyException(r);
	PyObject *ret = r==NS_OK ? Py_True : Py_False;
	Py_INCREF(ret);
	return ret;
}

struct PyMethodDef 
PyMethods_IEnumerator[] =
{
	{ "First", PyFirst, 1},
	{ "first", PyFirst, 1},
	{ "Next", PyNext, 1},
	{ "next", PyNext, 1},
	{ "CurrentItem", PyCurrentItem, 1},
	{ "currentItem", PyCurrentItem, 1},
	{ "IsDone", PyIsDone, 1},
	{ "isDone", PyIsDone, 1},
	{ "FetchBlock", PyFetchBlock, 1},
	{ "fetchBlock", PyFetchBlock, 1},
	{NULL}
};
