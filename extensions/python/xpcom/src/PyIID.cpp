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

// Py_nsIID.cpp -- IID type for Python/XPCOM
//
// This code is part of the XPCOM extensions for Python.
//
// Written May 2000 by Mark Hammond.
//
// Based heavily on the Python COM support, which is
// (c) Mark Hammond and Greg Stein.
//
// (c) 2000, ActiveState corp.
//
// @doc

#include "PyXPCOM_std.h"
#include <nsIInterfaceInfoManager.h>

nsIID Py_nsIID_NULL = {0,0,0,{0,0,0,0,0,0,0,0}};

// @pymethod <o Py_nsIID>|xpcom|IID|Creates a new IID object
PyObject *PyXPCOMMethod_IID(PyObject *self, PyObject *args)
{
	PyObject *obIID;
	PyObject *obBuf;
	if ( PyArg_ParseTuple(args, "O", &obBuf)) {
		if (PyBuffer_Check(obBuf)) {
			PyBufferProcs *pb = NULL;
			pb = obBuf->ob_type->tp_as_buffer;
			void *buf = NULL;
			int size = (*pb->bf_getreadbuffer)(obBuf, 0, &buf);
			if (size != sizeof(nsIID) || buf==NULL) {
				PyErr_Format(PyExc_ValueError, "A buffer object to be converted to an IID must be exactly %d bytes long", sizeof(nsIID));
				return NULL;
			}
			nsIID iid;
			unsigned char *ptr = (unsigned char *)buf;
			iid.m0 = XPT_SWAB32(*((PRUint32 *)ptr));
			ptr = ((unsigned char *)buf) + offsetof(nsIID, m1);
			iid.m1 = XPT_SWAB16(*((PRUint16 *)ptr));
			ptr = ((unsigned char *)buf) + offsetof(nsIID, m2);
			iid.m2 = XPT_SWAB16(*((PRUint16 *)ptr));
			ptr = ((unsigned char *)buf) + offsetof(nsIID, m3);
			for (int i=0;i<8;i++) {
				iid.m3[i] = *((PRUint8 *)ptr);
				ptr += sizeof(PRUint8);
			}
			return new Py_nsIID(iid);
		}
	}
	PyErr_Clear();
	// @pyparm string/Unicode|iidString||A string representation of an IID, or a ContractID.
	if ( !PyArg_ParseTuple(args, "O", &obIID) )
		return NULL;

	nsIID iid;
	if (!Py_nsIID::IIDFromPyObject(obIID, &iid))
		return NULL;
	return new Py_nsIID(iid);
}

/*static*/ PRBool
Py_nsIID::IIDFromPyObject(PyObject *ob, nsIID *pRet) {
	PRBool ok = PR_TRUE;
	nsIID iid;
	if (ob==NULL) {
		PyErr_SetString(PyExc_RuntimeError, "The IID object is invalid!");
		return PR_FALSE;
	}
	if (PyString_Check(ob)) {
		ok = iid.Parse(PyString_AsString(ob));
		if (!ok) {
			PyXPCOM_BuildPyException(NS_ERROR_ILLEGAL_VALUE);
			return PR_FALSE;
		}
	} else if (ob->ob_type == &type) {
		iid = ((Py_nsIID *)ob)->m_iid;
	} else if (PyInstance_Check(ob)) {
		// Get the _iidobj_ attribute
		PyObject *use_ob = PyObject_GetAttrString(ob, "_iidobj_");
		if (use_ob==NULL) {
			PyErr_SetString(PyExc_TypeError, "Only instances with _iidobj_ attributes can be used as IID objects");
			return PR_FALSE;
		}
		if (use_ob->ob_type != &type) {
			Py_DECREF(use_ob);
			PyErr_SetString(PyExc_TypeError, "instance _iidobj_ attributes must be raw IID object");
			return PR_FALSE;
		}
		iid = ((Py_nsIID *)use_ob)->m_iid;
		Py_DECREF(use_ob);
	} else {
		PyErr_Format(PyExc_TypeError, "Objects of type '%s' can not be converted to an IID", ob->ob_type->tp_name);
		ok = PR_FALSE;
	}
	if (ok) *pRet = iid;
	return ok;
}


// @object Py_nsIID|A Python object, representing an IID/CLSID.
// <nl>All pythoncom functions that return a CLSID/IID will return one of these
// objects.  However, in almost all cases, functions that expect a CLSID/IID
// as a param will accept either a string object, or a native Py_nsIID object.
PyTypeObject Py_nsIID::type =
{
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"IID",
	sizeof(Py_nsIID),
	0,
	PyTypeMethod_dealloc,                           /* tp_dealloc */
	0,                                              /* tp_print */
	PyTypeMethod_getattr,                           /* tp_getattr */
	0,                                              /* tp_setattr */
	PyTypeMethod_compare,                           /* tp_compare */
	PyTypeMethod_repr,                              /* tp_repr */
	0,                                              /* tp_as_number */
	0,                                              /* tp_as_sequence */
	0,                                              /* tp_as_mapping */
	PyTypeMethod_hash,                              /* tp_hash */
	0,                                              /* tp_call */
	PyTypeMethod_str,                               /* tp_str */
};

Py_nsIID::Py_nsIID(const nsIID &riid)
{
	ob_type = &type;
	_Py_NewReference(this);
	m_iid = riid;
}

/*static*/PyObject *
Py_nsIID::PyTypeMethod_getattr(PyObject *self, char *name)
{
	Py_nsIID *me = (Py_nsIID *)self;
	if (strcmp(name, "name")==0) {
		char *iid_repr = nsnull;
		nsCOMPtr<nsIInterfaceInfoManager> iim = XPTI_GetInterfaceInfoManager();
		if (iim!=nsnull)
			iim->GetNameForIID(&me->m_iid, &iid_repr);
		if (iid_repr==nsnull)
			iid_repr = me->m_iid.ToString();
		PyObject *ret;
		if (iid_repr != nsnull) {
			ret = PyString_FromString(iid_repr);
			nsMemory::Free(iid_repr);
		} else
			ret = PyString_FromString("<cant get IID info!>");
		return ret;
	}
	return PyErr_Format(PyExc_AttributeError, "IID objects have no attribute '%s'", name);
}

/* static */ int
Py_nsIID::PyTypeMethod_compare(PyObject *self, PyObject *other)
{
	Py_nsIID *s_iid = (Py_nsIID *)self;
	Py_nsIID *o_iid = (Py_nsIID *)other;
	int rc = memcmp(&s_iid->m_iid, &o_iid->m_iid, sizeof(s_iid->m_iid)); 
	return rc == 0 ? 0 : (rc < 0 ? -1 : 1);
}

/* static */ PyObject *
Py_nsIID::PyTypeMethod_repr(PyObject *self)
{
	Py_nsIID *s_iid = (Py_nsIID *)self;
	char buf[256];
	char *sziid = s_iid->m_iid.ToString();
	sprintf(buf, "_xpcom.IID('%s')", sziid);
	nsMemory::Free(sziid);
	return PyString_FromString(buf);
}

/* static */ PyObject *
Py_nsIID::PyTypeMethod_str(PyObject *self)
{
	Py_nsIID *s_iid = (Py_nsIID *)self;
	char *sziid = s_iid->m_iid.ToString();
	PyObject *ret = PyString_FromString(sziid);
	nsMemory::Free(sziid);
	return ret;
}

/* static */long
Py_nsIID::PyTypeMethod_hash(PyObject *self)
{
	const nsIID &iid = ((Py_nsIID *)self)->m_iid;

	long ret = iid.m0 + iid.m1 + iid.m2;
	for (int i=0;i<7;i++)
		ret += iid.m3[i];
	if ( ret == -1 )
		return -2;
	return ret;
}

/*static*/ void
Py_nsIID::PyTypeMethod_dealloc(PyObject *ob)
{
	delete (Py_nsIID *)ob;
}
