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
#include <nsXPCOM.h>
#include <nsISupportsPrimitives.h>


static PyTypeObject PyInterfaceType_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,			/* Number of items for varobject */
	"interface-type",			/* Name of this type */
	sizeof(PyTypeObject),	/* Basic object size */
	0,			/* Item size for varobject */
	0,			/*tp_dealloc*/
	0,			/*tp_print*/
	PyType_Type.tp_getattr, /*tp_getattr*/
	0,			/*tp_setattr*/
	0,			/*tp_compare*/
	PyType_Type.tp_repr,	/*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
	0,			/*tp_call*/
	0,			/*tp_str*/
	0,			/*tp_xxx1*/
	0,			/*tp_xxx2*/
	0,			/*tp_xxx3*/
	0,			/*tp_xxx4*/
	"Define the behavior of a PythonCOM Interface type.",
};

/*static*/ PRBool
PyXPCOM_TypeObject::IsType(PyTypeObject *t)
{
	return t->ob_type == &PyInterfaceType_Type;
}

////////////////////////////////////////////////////////////////////
//
// The type methods
//
/*static*/PyObject *
PyXPCOM_TypeObject::Py_getattr(PyObject *self, char *name)
{
	return ((Py_nsISupports *)self)->getattr(name);
}

/*static*/int
PyXPCOM_TypeObject::Py_setattr(PyObject *op, char *name, PyObject *v)
{
	return ((Py_nsISupports *)op)->setattr(name, v);
}

// @pymethod int|Py_nsISupports|__cmp__|Implements XPCOM rules for object identity.
/*static*/int
PyXPCOM_TypeObject::Py_cmp(PyObject *self, PyObject *other)
{
	// @comm NOTE: Copied from COM - have not confirmed these rules are true for XPCOM
	// @comm As per the XPCOM rules for object identity, both objects are queried for nsISupports, and these values compared.
	// The only meaningful test is for equality - the result of other comparisons is undefined
	// (ie, determined by the object's relative addresses in memory.
	nsISupports *pUnkOther;
	nsISupports *pUnkThis;
	if (!Py_nsISupports::InterfaceFromPyObject(self, NS_GET_IID(nsISupports), &pUnkThis, PR_FALSE))
		return -1;
	if (!Py_nsISupports::InterfaceFromPyObject(other, NS_GET_IID(nsISupports), &pUnkOther, PR_FALSE)) {
		pUnkThis->Release();
		return -1;
	}
	int rc = pUnkThis==pUnkOther ? 0 :
		(pUnkThis < pUnkOther ? -1 : 1);
	pUnkThis->Release();
	pUnkOther->Release();
	return rc;
}

// @pymethod int|Py_nsISupports|__hash__|Implement a hash-code for the XPCOM object using XPCOM identity rules.
/*static*/long PyXPCOM_TypeObject::Py_hash(PyObject *self)
{
	// We always return the value of the nsISupports *.
	nsISupports *pUnkThis;
	if (!Py_nsISupports::InterfaceFromPyObject(self, NS_GET_IID(nsISupports), &pUnkThis, PR_FALSE))
		return -1;
	long ret = _Py_HashPointer(pUnkThis);
	pUnkThis->Release();
	return ret;
}

// @method string|Py_nsISupports|__repr__|Called to create a representation of a Py_nsISupports object
/*static */PyObject *
PyXPCOM_TypeObject::Py_repr(PyObject *self)
{
	// @comm The repr of this object displays both the object's address, and its attached nsISupports's address
	Py_nsISupports *pis = (Py_nsISupports *)self;
	// Try and get the IID name.
	char *iid_repr;
	nsCOMPtr<nsIInterfaceInfoManager> iim = XPTI_GetInterfaceInfoManager();
	if (iim!=nsnull)
		iim->GetNameForIID(&pis->m_iid, &iid_repr);
	if (iid_repr==nsnull)
		// no IIM available, or it doesnt know the name.
		iid_repr = pis->m_iid.ToString();
	// XXX - need some sort of buffer overflow.
	char buf[512];
	sprintf(buf, "<XPCOM object (%s) at 0x%p/0x%p>", iid_repr, self, pis->m_obj);
	nsMemory::Free(iid_repr);
	return PyString_FromString(buf);
}

/*static */PyObject *
PyXPCOM_TypeObject::Py_str(PyObject *self)
{
	Py_nsISupports *pis = (Py_nsISupports *)self;
	nsresult rv;
	char *val = NULL;
	Py_BEGIN_ALLOW_THREADS;
	{ // scope to kill pointer while thread-lock released.
	nsCOMPtr<nsISupportsCString> ss( do_QueryInterface(pis->m_obj, &rv ));
	if (NS_SUCCEEDED(rv))
		rv = ss->ToString(&val);
	} // end-scope 
	Py_END_ALLOW_THREADS;
	PyObject *ret;
	if (NS_FAILED(rv))
		ret = Py_repr(self);
	else
		ret = PyString_FromString(val);
	if (val) nsMemory::Free(val);
	return ret;
}

/* static */void
PyXPCOM_TypeObject::Py_dealloc(PyObject *self)
{
	delete (Py_nsISupports *)self;
}

PyXPCOM_TypeObject::PyXPCOM_TypeObject( const char *name, PyXPCOM_TypeObject *pBase, int typeSize, struct PyMethodDef* methodList, PyXPCOM_I_CTOR thector)
{
	static const PyTypeObject type_template = {
		PyObject_HEAD_INIT(&PyInterfaceType_Type)
		0,                                           /*ob_size*/
		"XPCOMTypeTemplate",                         /*tp_name*/
		sizeof(Py_nsISupports),                 /*tp_basicsize*/
		0,                                           /*tp_itemsize*/
		Py_dealloc,                                  /* tp_dealloc */
		0,                                           /* tp_print */
		Py_getattr,                                  /* tp_getattr */
		Py_setattr,                                  /* tp_setattr */
		Py_cmp,                                      /* tp_compare */
		Py_repr,                                     /* tp_repr */
    		0,                                           /* tp_as_number*/
		0,                                           /* tp_as_sequence */
		0,                                           /* tp_as_mapping */
		Py_hash,					     /* tp_hash */
		0,                                           /* tp_call */
		Py_str,                                      /* tp_str */
	};

	*((PyTypeObject *)this) = type_template;

	chain.methods = methodList;
	chain.link = pBase ? &pBase->chain : NULL;

	baseType = pBase;
	ctor = thector;

	// cast away const, as Python doesnt use it.
	tp_name = (char *)name;
	tp_basicsize = typeSize;
}

PyXPCOM_TypeObject::~PyXPCOM_TypeObject()
{
}
