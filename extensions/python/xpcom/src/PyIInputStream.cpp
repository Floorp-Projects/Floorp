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
// Written September 2000 by Mark Hammond.
//
// Based heavily on the Python COM support, which is
// (c) Mark Hammond and Greg Stein.
//
// (c) 2000, ActiveState corp.

#include "PyXPCOM_std.h"
#include "nsIInputStream.h"

// Prevents us needing to use an nsIScriptableInputStream
// (and even that can't read binary data!!!)

static nsIInputStream *GetI(PyObject *self) {
	nsIID iid = NS_GET_IID(nsIInputStream);

	if (!Py_nsISupports::Check(self, iid)) {
		PyErr_SetString(PyExc_TypeError, "This object is not the correct interface");
		return NULL;
	}
	return (nsIInputStream *)Py_nsISupports::GetI(self);
}

static PyObject *DoPyRead_Buffer(nsIInputStream *pI, PyObject *obBuffer, PRUint32 n)
{
	PRUint32 nread;
	void *buf;
	PRUint32 buf_len;
	if (PyObject_AsWriteBuffer(obBuffer, &buf, (int *)&buf_len) != 0) {
		PyErr_Clear();
		PyErr_SetString(PyExc_TypeError, "The buffer object does not have a write buffer!");
		return NULL;
	}
	if (n==(PRUint32)-1) {
		n = buf_len;
	} else {
		if (n > buf_len) {
			NS_WARNING("Warning: PyIInputStream::read() was passed an integer size greater than the size of the passed buffer!  Buffer size used.\n");
			n = buf_len;
		}
	}
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->Read((char *)buf, n, &nread);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	return PyInt_FromLong(nread);
}

static PyObject *DoPyRead_Size(nsIInputStream *pI, PRUint32 n)
{
	if (n==(PRUint32)-1) {
		nsresult r;
		Py_BEGIN_ALLOW_THREADS;
		r = pI->Available(&n);
		Py_END_ALLOW_THREADS;
		if (NS_FAILED(r))
			return PyXPCOM_BuildPyException(r);
	}
	if (n==0) { // mozilla will assert if we alloc zero bytes.
		return PyBuffer_New(0);
	}
	char *buf = (char *)nsMemory::Alloc(n);
	if (buf==NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	nsresult r;
	PRUint32 nread;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->Read(buf, n, &nread);
	Py_END_ALLOW_THREADS;
	PyObject *rc = NULL;
	if ( NS_SUCCEEDED(r) ) {
		rc = PyBuffer_New(nread);
		if (rc != NULL) {
			void *ob_buf;
			PRUint32 buf_len;
			if (PyObject_AsWriteBuffer(rc, &ob_buf, (int *)&buf_len) != 0) {
				// should never fail - we just created it!
				return NULL;
			}
			if (buf_len != nread) {
				PyErr_SetString(PyExc_RuntimeError, "New buffer isnt the size we create it!");
				return NULL;
			}
			memcpy(ob_buf, buf, nread);
		}
	} else
		PyXPCOM_BuildPyException(r);
	nsMemory::Free(buf);
	return rc;
}

static PyObject *PyRead(PyObject *self, PyObject *args)
{
	PyObject *obBuffer = NULL;
	PRUint32 n = (PRUint32)-1;

	nsIInputStream *pI = GetI(self);
	if (pI==NULL)
		return NULL;
	if (PyArg_ParseTuple(args, "|i", (int *)&n))
		// This worked - no args, or just an int.
		return DoPyRead_Size(pI, n);
	// try our other supported arg format.
	PyErr_Clear();
	if (!PyArg_ParseTuple(args, "O|i", &obBuffer, (int *)&n)) {
		PyErr_Clear();
		PyErr_SetString(PyExc_TypeError, "'read()' must be called as (buffer_ob, int_size=-1) or (int_size=-1)");
		return NULL;
	}
	return DoPyRead_Buffer(pI, obBuffer, n);
}


struct PyMethodDef 
PyMethods_IInputStream[] =
{
	{ "read", PyRead, 1},
	// The rest are handled as normal
	{NULL}
};
