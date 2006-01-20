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


static nsIInterfaceInfo *GetI(PyObject *self) {
	nsIID iid = NS_GET_IID(nsIInterfaceInfo);

	if (!Py_nsISupports::Check(self, iid)) {
		PyErr_SetString(PyExc_TypeError, "This object is not the correct interface");
		return NULL;
	}
	return (nsIInterfaceInfo *)Py_nsISupports::GetI(self);
}

static PyObject *PyGetName(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetName"))
		return NULL;

	nsIInterfaceInfo *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	char *name;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetName(&name);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	PyObject *ret = PyString_FromString(name);
	nsMemory::Free(name);
	return ret;
}

static PyObject *PyGetIID(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetIID"))
		return NULL;

	nsIInterfaceInfo *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsIID *iid_ret;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetInterfaceIID(&iid_ret);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	PyObject *ret = Py_nsIID::PyObjectFromIID(*iid_ret);
	nsMemory::Free(iid_ret);
	return ret;
}

static PyObject *PyIsScriptable(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":IsScriptable"))
		return NULL;

	nsIInterfaceInfo *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	PRBool b_ret;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->IsScriptable(&b_ret);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	return PyInt_FromLong(b_ret);
}

static PyObject *PyGetParent(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetParent"))
		return NULL;

	nsIInterfaceInfo *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	nsCOMPtr<nsIInterfaceInfo> pRet;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetParent(getter_AddRefs(pRet));
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	return Py_nsISupports::PyObjectFromInterface(pRet, NS_GET_IID(nsIInterfaceInfo), PR_FALSE);
}

static PyObject *PyGetMethodCount(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetMethodCount"))
		return NULL;

	nsIInterfaceInfo *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	PRUint16 ret;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetMethodCount(&ret);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	return PyInt_FromLong(ret);
}


static PyObject *PyGetConstantCount(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetConstantCount"))
		return NULL;

	nsIInterfaceInfo *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	PRUint16 ret;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetConstantCount(&ret);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	return PyInt_FromLong(ret);
}

static PyObject *PyGetMethodInfo(PyObject *self, PyObject *args)
{
	PRUint16 index;
	if (!PyArg_ParseTuple(args, "h:GetMethodInfo", &index))
		return NULL;

	nsIInterfaceInfo *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	PRUint16 nmethods;
	pI->GetMethodCount(&nmethods);
	if (index>=nmethods) {
		PyErr_SetString(PyExc_ValueError, "The method index is out of range");
		return NULL;
	}

	const nsXPTMethodInfo *pRet;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetMethodInfo(index, &pRet);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	return PyObject_FromXPTMethodDescriptor(pRet);
}

static PyObject *PyGetMethodInfoForName(PyObject *self, PyObject *args)
{
	char *name;
	if (!PyArg_ParseTuple(args, "s:GetMethodInfoForName", &name))
		return NULL;

	nsIInterfaceInfo *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	const nsXPTMethodInfo *pRet;
	PRUint16 index;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetMethodInfoForName(name, &index, &pRet);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	PyObject *ret_i = PyObject_FromXPTMethodDescriptor(pRet);
	if (ret_i==NULL)
		return NULL;
	PyObject *real_ret = Py_BuildValue("iO", (int)index, ret_i);
	Py_DECREF(ret_i);
	return real_ret;
}


static PyObject *PyGetConstant(PyObject *self, PyObject *args)
{
	PRUint16 index;
	if (!PyArg_ParseTuple(args, "h:GetConstant", &index))
		return NULL;

	nsIInterfaceInfo *pI = GetI(self);
	if (pI==NULL)
		return NULL;

	const nsXPTConstant *pRet;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pI->GetConstant(index, &pRet);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	return PyObject_FromXPTConstant(pRet);
}

static PRBool __GetMethodInfoHelper(nsIInterfaceInfo *pii, int mi, int pi, const nsXPTMethodInfo **ppmi)
{
	PRUint16 nmethods=0;
	pii->GetMethodCount(&nmethods);
	if (mi<0 || mi>=nmethods) {
		PyErr_SetString(PyExc_ValueError, "The method index is out of range");
		return PR_FALSE;
	}
	const nsXPTMethodInfo *pmi;
	nsresult r = pii->GetMethodInfo(mi, &pmi);
	if ( NS_FAILED(r) ) {
		PyXPCOM_BuildPyException(r);
		return PR_FALSE;
	}

	int nparams=0;
	nparams = pmi->GetParamCount();
	if (pi<0 || pi>=nparams) {
		PyErr_SetString(PyExc_ValueError, "The param index is out of range");
		return PR_FALSE;
	}
	*ppmi = pmi;
	return PR_TRUE;
}

static PyObject *PyGetInfoForParam(PyObject *self, PyObject *args)
{
	nsIInterfaceInfo *pii = GetI(self);
	if (pii==NULL)
		return NULL;
	PRUint16 mi, pi;
	if (!PyArg_ParseTuple(args, "hh:GetInfoForParam", &mi, &pi))
		return NULL;
	const nsXPTMethodInfo *pmi;
	if (!__GetMethodInfoHelper(pii, mi, pi, &pmi))
		return NULL;
	const nsXPTParamInfo& param_info = pmi->GetParam((PRUint8)pi);
	nsCOMPtr<nsIInterfaceInfo> pnewii;
	nsresult n = pii->GetInfoForParam(mi, &param_info, getter_AddRefs(pnewii));
	if (NS_FAILED(n))
		return PyXPCOM_BuildPyException(n);
	return Py_nsISupports::PyObjectFromInterface(pnewii, NS_GET_IID(nsIInterfaceInfo));
}

static PyObject *PyGetIIDForParam(PyObject *self, PyObject *args)
{
	nsIInterfaceInfo *pii = GetI(self);
	if (pii==NULL)
		return NULL;
	PRUint16 mi, pi;
	if (!PyArg_ParseTuple(args, "hh:GetIIDForParam", &mi, &pi))
		return NULL;
	const nsXPTMethodInfo *pmi;
	if (!__GetMethodInfoHelper(pii, mi, pi, &pmi))
		return NULL;
	const nsXPTParamInfo& param_info = pmi->GetParam((PRUint8)pi);
	nsIID *piid;
	nsresult n = pii->GetIIDForParam(mi, &param_info, &piid);
	if (NS_FAILED(n) || piid==nsnull)
		return PyXPCOM_BuildPyException(n);
	PyObject *rc = Py_nsIID::PyObjectFromIID(*piid);
	nsMemory::Free((void*)piid);
	return rc;
}

static PyObject *PyGetTypeForParam(PyObject *self, PyObject *args)
{
	nsIInterfaceInfo *pii = GetI(self);
	if (pii==NULL)
		return NULL;
	PRUint16 mi, pi, dim;
	if (!PyArg_ParseTuple(args, "hhh:GetTypeForParam", &mi, &pi, &dim))
		return NULL;
	const nsXPTMethodInfo *pmi;
	if (!__GetMethodInfoHelper(pii, mi, pi, &pmi))
		return NULL;
	nsXPTType datumType;
	const nsXPTParamInfo& param_info = pmi->GetParam((PRUint8)pi);
	nsresult n = pii->GetTypeForParam(mi, &param_info, dim, &datumType);
	if (NS_FAILED(n))
		return PyXPCOM_BuildPyException(n);
	return PyObject_FromXPTType(&datumType);
}

static PyObject *PyGetSizeIsArgNumberForParam(PyObject *self, PyObject *args)
{
	nsIInterfaceInfo *pii = GetI(self);
	if (pii==NULL)
		return NULL;
	PRUint16 mi, pi, dim;
	if (!PyArg_ParseTuple(args, "hhh:GetSizeIsArgNumberForParam", &mi, &pi, &dim))
		return NULL;
	const nsXPTMethodInfo *pmi;
	if (!__GetMethodInfoHelper(pii, mi, pi, &pmi))
		return NULL;
        PRUint8 ret;
	const nsXPTParamInfo& param_info = pmi->GetParam((PRUint8)pi);
	nsresult n = pii->GetSizeIsArgNumberForParam(mi, &param_info, dim, &ret);
	if (NS_FAILED(n))
		return PyXPCOM_BuildPyException(n);
	return PyInt_FromLong(ret);
}

static PyObject *PyGetLengthIsArgNumberForParam(PyObject *self, PyObject *args)
{
	nsIInterfaceInfo *pii = GetI(self);
	if (pii==NULL)
		return NULL;
	PRUint16 mi, pi, dim;
	if (!PyArg_ParseTuple(args, "hhh:GetLengthIsArgNumberForParam", &mi, &pi, &dim))
		return NULL;
	const nsXPTMethodInfo *pmi;
	if (!__GetMethodInfoHelper(pii, mi, pi, &pmi))
		return NULL;
        PRUint8 ret;
	const nsXPTParamInfo& param_info = pmi->GetParam((PRUint8)pi);
	nsresult n = pii->GetLengthIsArgNumberForParam(mi, &param_info, dim, &ret);
	if (NS_FAILED(n))
		return PyXPCOM_BuildPyException(n);
	return PyInt_FromLong(ret);
}

static PyObject *PyGetInterfaceIsArgNumberForParam(PyObject *self, PyObject *args)
{
	nsIInterfaceInfo *pii = GetI(self);
	if (pii==NULL)
		return NULL;
	PRUint16 mi, pi;
	if (!PyArg_ParseTuple(args, "hhh:GetInterfaceIsArgNumberForParam", &mi, &pi))
		return NULL;
	const nsXPTMethodInfo *pmi;
	if (!__GetMethodInfoHelper(pii, mi, pi, &pmi))
		return NULL;
        PRUint8 ret;
	const nsXPTParamInfo& param_info = pmi->GetParam((PRUint8)pi);
	nsresult n = pii->GetInterfaceIsArgNumberForParam(mi, &param_info, &ret);
	if (NS_FAILED(n))
		return PyXPCOM_BuildPyException(n);
	return PyInt_FromLong(ret);
}

struct PyMethodDef 
PyMethods_IInterfaceInfo[] =
{
	{ "GetName", PyGetName, 1},
	{ "GetIID", PyGetIID, 1},
	{ "IsScriptable", PyIsScriptable, 1},
	{ "GetParent", PyGetParent, 1},
	{ "GetMethodCount", PyGetMethodCount, 1},
	{ "GetConstantCount", PyGetConstantCount, 1},
	{ "GetMethodInfo", PyGetMethodInfo, 1},
	{ "GetMethodInfoForName", PyGetMethodInfoForName, 1},
	{ "GetConstant", PyGetConstant, 1},
	{ "GetInfoForParam", PyGetInfoForParam, 1},
	{ "GetIIDForParam", PyGetIIDForParam, 1},
	{ "GetTypeForParam", PyGetTypeForParam, 1},
	{ "GetSizeIsArgNumberForParam", PyGetSizeIsArgNumberForParam, 1},
	{ "GetLengthIsArgNumberForParam", PyGetLengthIsArgNumberForParam, 1},
	{ "GetInterfaceIsArgNumberForParam", PyGetInterfaceIsArgNumberForParam, 1},
	{NULL}
};

/*
  NS_IMETHOD GetMethodInfo(PRUint16 index, const nsXPTMethodInfo * *info) = 0;
  NS_IMETHOD GetMethodInfoForName(const char *methodName, PRUint16 *index, const nsXPTMethodInfo * *info) = 0;
  NS_IMETHOD GetConstant(PRUint16 index, const nsXPTConstant * *constant) = 0;
  NS_IMETHOD GetInfoForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, nsIInterfaceInfo **_retval) = 0;
  NS_IMETHOD GetIIDForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, nsIID * *_retval) = 0;
  NS_IMETHOD GetTypeForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint16 dimension, nsXPTType *_retval) = 0;
  NS_IMETHOD GetSizeIsArgNumberForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint16 dimension, PRUint8 *_retval) = 0;
  NS_IMETHOD GetLengthIsArgNumberForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint16 dimension, PRUint8 *_retval) = 0;
  NS_IMETHOD GetInterfaceIsArgNumberForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint8 *_retval) = 0;

*/
