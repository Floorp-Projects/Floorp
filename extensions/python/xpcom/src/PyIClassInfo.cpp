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
 * ActiveState Tool Corp. Portions created by ActiveState Tool Corp. are Copyright (C)  2001 ActiveState Tool Corp.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
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
// Written May 2001 by Mark Hammond.
//
// Based heavily on the Python COM support, which is
// (c) Mark Hammond and Greg Stein.
//
// (c) 2001, ActiveState corp.

#include "PyXPCOM_std.h"
#include "nsIClassInfo.h"

static nsIClassInfo *_GetI(PyObject *self) {
	nsIID iid = NS_GET_IID(nsIClassInfo);

	if (!Py_nsISupports::Check(self, iid)) {
		PyErr_SetString(PyExc_TypeError, "This object is not the correct interface");
		return NULL;
	}
	return (nsIClassInfo *)Py_nsISupports::GetI(self);
}

static PyObject *PyGetInterfaces(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	nsIClassInfo *pI = _GetI(self);
	if (pI==NULL)
		return NULL;

	nsIID** iidArray = nsnull;
	PRUint32 iidCount = 0;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetInterfaces(&iidCount, &iidArray);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	PyObject *ret = PyTuple_New(iidCount);
	if (ret==NULL)
		return NULL;
	for (PRUint32 i=0;i<iidCount;i++)
		PyTuple_SET_ITEM( ret, i, Py_nsIID::PyObjectFromIID(*(iidArray[i])) );
	return ret;
}

static PyObject *PyGetHelperForLanguage(PyObject *self, PyObject *args)
{
	PRUint32 language = nsIProgrammingLanguage::PYTHON;
	if (!PyArg_ParseTuple(args, "|i", &language))
		return NULL;
	nsIClassInfo *pI = _GetI(self);
	if (pI==NULL)
		return NULL;

	nsresult r;
	nsISupports *pi;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetHelperForLanguage(language, &pi);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	return Py_nsISupports::PyObjectFromInterface(pi, NS_GET_IID(nsISupports), PR_FALSE);
}

static PyObject *MakeStringOrNone(char *v)
{
	if (v)
		return PyString_FromString(v);
	Py_INCREF(Py_None);
	return Py_None;
}

#define GETATTR_CHECK_RESULT(nr) if (NS_FAILED(nr)) return PyXPCOM_BuildPyException(nr)

PyObject *
Py_nsIClassInfo::getattr(const char *name)
{
	nsIClassInfo *pI = _GetI(this);
	if (pI==NULL)
		return NULL;

	nsresult nr;
	PyObject *ret = NULL;
	if (strcmp(name, "contractID")==0) {
		char *str_ret = NULL;
		Py_BEGIN_ALLOW_THREADS;
		nr = pI->GetContractID(&str_ret);
		Py_END_ALLOW_THREADS;
		GETATTR_CHECK_RESULT(nr);
		ret = MakeStringOrNone(str_ret);
		nsMemory::Free(str_ret);
	} else if (strcmp(name, "classDescription")==0) {
		char *str_ret = NULL;
		Py_BEGIN_ALLOW_THREADS;
		nr = pI->GetClassDescription(&str_ret);
		Py_END_ALLOW_THREADS;
		GETATTR_CHECK_RESULT(nr);
		ret = MakeStringOrNone(str_ret);
		nsMemory::Free(str_ret);
	} else if (strcmp(name, "classID")==0) {
		nsIID *iid = NULL;
		Py_BEGIN_ALLOW_THREADS;
		nr = pI->GetClassID(&iid);
		Py_END_ALLOW_THREADS;
		GETATTR_CHECK_RESULT(nr);
		ret = Py_nsIID::PyObjectFromIID(*iid);
		nsMemory::Free(iid);
	} else if (strcmp(name, "implementationLanguage")==0) {
		PRUint32 i;
		Py_BEGIN_ALLOW_THREADS;
		nr = pI->GetImplementationLanguage(&i);
		Py_END_ALLOW_THREADS;
		GETATTR_CHECK_RESULT(nr);
		ret = PyInt_FromLong(i);
	} else {
		ret = Py_nsISupports::getattr(name);
	}
	return ret;
}

int
Py_nsIClassInfo::setattr(const char *name, PyObject *v)
{
	return Py_nsISupports::setattr(name, v);

}

struct PyMethodDef 
PyMethods_IClassInfo[] =
{
	{ "getInterfaces", PyGetInterfaces, 1},
	{ "GetInterfaces", PyGetInterfaces, 1},
	{ "getHelperForLanguage", PyGetHelperForLanguage, 1},
	{ "GetHelperForLanguage", PyGetHelperForLanguage, 1},
	{NULL}
};
