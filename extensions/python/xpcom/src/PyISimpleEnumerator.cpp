/* Copyright (c) 2000-2001 ActiveState Tool Corporation.
   See the file LICENSE.txt for licensing information. */

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
#include <nsISimpleEnumerator.h>

static nsISimpleEnumerator *GetI(PyObject *self) {
	nsIID iid = NS_GET_IID(nsISimpleEnumerator);

	if (!Py_nsISupports::Check(self, iid)) {
		PyErr_SetString(PyExc_TypeError, "This object is not the correct interface");
		return NULL;
	}
	return (nsISimpleEnumerator *)Py_nsISupports::GetI(self);
}


static PyObject *PyHasMoreElements(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":HasMoreElements"))
		return NULL;

	nsISimpleEnumerator *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsresult r;
	PRBool more;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->HasMoreElements(&more);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	return PyInt_FromLong(more);
}

static PyObject *PyGetNext(PyObject *self, PyObject *args)
{
	PyObject *obIID = NULL;
	if (!PyArg_ParseTuple(args, "|O:GetNext", &obIID))
		return NULL;

	nsIID iid(NS_GET_IID(nsISupports));
	if (obIID != NULL && !Py_nsIID::IIDFromPyObject(obIID, &iid))
		return NULL;
	nsISimpleEnumerator *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsISupports *pRet = nsnull;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetNext(&pRet);
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
	return Py_nsISupports::PyObjectFromInterface(pRet, iid, PR_FALSE);
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
	nsISimpleEnumerator *pI = GetI(self);
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
	nsresult r;
	PRBool more;
	Py_BEGIN_ALLOW_THREADS;
	for (;n_fetched<n_wanted;) {
		r = pI->HasMoreElements(&more);
		if (NS_FAILED(r))
			break; // this _is_ an error!
		if (!more)
			break; // Normal enum end.
		nsISupports *pNew;
		r = pI->GetNext(&pNew);
		if (NS_FAILED(r)) // IS an error
			break;
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
		n_fetched++;
	}
	Py_END_ALLOW_THREADS;
	PyObject *ret;
	if (NS_SUCCEEDED(r)) {
		ret = PyList_New(n_fetched);
		if (ret)
			for (int i=0;i<n_fetched;i++) {
				PyObject *new_ob = Py_nsISupports::PyObjectFromInterface(fetched[i], iid, PR_FALSE);
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


struct PyMethodDef 
PyMethods_ISimpleEnumerator[] =
{
	{ "HasMoreElements", PyHasMoreElements, 1},
	{ "hasMoreElements", PyHasMoreElements, 1},
	{ "GetNext", PyGetNext, 1},
	{ "getNext", PyGetNext, 1},
	{ "FetchBlock", PyFetchBlock, 1},
	{ "fetchBlock", PyFetchBlock, 1},
	{NULL}
};
