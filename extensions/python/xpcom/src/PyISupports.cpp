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

static PRInt32 cInterfaces=0;
static PyObject *g_obFuncMakeInterfaceCount = NULL; // XXX - never released!!!

PRInt32 
_PyXPCOM_GetInterfaceCount(void)
{
	return cInterfaces;
}

Py_nsISupports::Py_nsISupports(nsISupports *punk, const nsIID &iid, PyTypeObject *this_type)
{
	ob_type = this_type;
	m_obj = punk;
	m_iid = iid;
	// refcnt of object managed by caller.
	PR_AtomicIncrement(&cInterfaces);
	PyXPCOM_DLLAddRef();
	_Py_NewReference(this);
}

Py_nsISupports::~Py_nsISupports()
{
	SafeRelease(this);	
	PR_AtomicDecrement(&cInterfaces);
	PyXPCOM_DLLRelease();
}

/*static*/ nsISupports *
Py_nsISupports::GetI(PyObject *self, nsIID *ret_iid)
{
	if (self==NULL) {
		PyErr_SetString(PyExc_ValueError, "The Python object is invalid");
		return NULL;
	}
	Py_nsISupports *pis = (Py_nsISupports *)self;
	if (pis->m_obj==NULL) {
		// This should never be able to happen.
		PyErr_SetString(PyExc_ValueError, "Internal Error - The XPCOM object has been released.");
		return NULL;
	}
	if (ret_iid)
		*ret_iid = pis->m_iid;
	return pis->m_obj;
}

/*static*/ void
Py_nsISupports::SafeRelease(Py_nsISupports *ob)
{
	if (!ob)
		return;
	if (ob->m_obj)
	{
		long rcnt;
		Py_BEGIN_ALLOW_THREADS;
		rcnt = ob->m_obj->Release();
		Py_END_ALLOW_THREADS;

#ifdef _DEBUG_LIFETIMES
		LogF(buf, "   SafeRelease(%ld) -> %s at 0x%0lx, nsISupports at 0x%0lx - Release() returned %ld",GetCurrentThreadId(), ob->ob_type->tp_name,ob, ob->m_obj,rcnt);
#endif
		ob->m_obj = NULL;
	}
}

/* virtual */ PyObject *
Py_nsISupports::getattr(const char *name)
{
	if (strcmp(name, "IID")==0)
		return Py_nsIID::PyObjectFromIID( m_iid );

	PyXPCOM_TypeObject *this_type = (PyXPCOM_TypeObject *)ob_type;
	return Py_FindMethodInChain(&this_type->chain, this, (char *)name);
}

/* virtual */ int
Py_nsISupports::setattr(const char *name, PyObject *v)
{
	char buf[128];
	sprintf(buf, "%s has read-only attributes", ob_type->tp_name );
	PyErr_SetString(PyExc_TypeError, buf);
	return -1;
}

/*static*/ Py_nsISupports *
Py_nsISupports::Constructor(nsISupports *pInitObj, const nsIID &iid)
{
	return new Py_nsISupports(pInitObj, 
				       iid, 
				       type);
}

PRBool
Py_nsISupports::InterfaceFromPyISupports(PyObject *ob, 
                                         const nsIID &iid, 
                                         nsISupports **ppv)
{
	nsISupports *pis;
	PRBool rc = PR_FALSE;
	if ( !Check(ob) )
	{
		PyErr_Format(PyExc_TypeError, "Objects of type '%s' can not be used as COM objects", ob->ob_type->tp_name);
		goto done;
	}
	nsIID already_iid;
	pis = GetI(ob, &already_iid);
	if ( !pis )
		goto done;	/* exception was set by GetI() */
	/* note: we don't (yet) explicitly hold a reference to pis */
	if (iid.Equals(Py_nsIID_NULL)) {
		// a bit of a hack - we are asking for the arbitary interface
		// wrapped by this object, not some other specific interface - 
		// so no QI, just an AddRef();
		Py_BEGIN_ALLOW_THREADS
		pis->AddRef();
		Py_END_ALLOW_THREADS
		*ppv = pis;
	} else {
		// specific interface requested - if it is not already the
		// specific interface, QI for it and discard pis.
		if (iid.Equals(already_iid)) {
			*ppv = pis;
			pis->AddRef();
		} else {
			nsresult r;
			Py_BEGIN_ALLOW_THREADS
			r = pis->QueryInterface(iid, (void **)ppv);
			Py_END_ALLOW_THREADS
			if ( NS_FAILED(r) )
			{
				PyXPCOM_BuildPyException(r);
				goto done;
			}
			/* note: the QI added a ref for the return value */
		}
	}
	rc = PR_TRUE;
done:
	return rc;
}

PRBool
Py_nsISupports::InterfaceFromPyObject(PyObject *ob, 
					   const nsIID &iid, 
					   nsISupports **ppv, 
					   PRBool bNoneOK,
					   PRBool bTryAutoWrap /* = PR_TRUE */)
{
	if ( ob == NULL )
	{
		// don't overwrite an error message
		if ( !PyErr_Occurred() )
			PyErr_SetString(PyExc_TypeError, "The Python object is invalid");
		return PR_FALSE;
	}
	if ( ob == Py_None )
	{
		if ( bNoneOK )
		{
			*ppv = NULL;
			return PR_TRUE;
		}
		else
		{
			PyErr_SetString(PyExc_TypeError, "None is not a invalid interface object in this context");
			return PR_FALSE;
		}
	}

	// support nsIVariant
	if (iid.Equals(NS_GET_IID(nsIVariant)) || iid.Equals(NS_GET_IID(nsIWritableVariant))) {
		// Check it is not already nsIVariant
		if (PyInstance_Check(ob)) {
			PyObject *sub_ob = PyObject_GetAttrString(ob, "_comobj_");
			if (sub_ob==NULL) {
				PyErr_Clear();
			} else {
				if (InterfaceFromPyISupports(sub_ob, iid, ppv)) {
					Py_DECREF(sub_ob);
					return PR_TRUE;
				}
				PyErr_Clear();
				Py_DECREF(sub_ob);
			}
		}
		*ppv = PyObject_AsVariant(ob);
		return *ppv != NULL;
	}
	// end of variant support.

	if (PyInstance_Check(ob)) {
		// Get the _comobj_ attribute
		PyObject *use_ob = PyObject_GetAttrString(ob, "_comobj_");
		if (use_ob==NULL) {
			PyErr_Clear();
			if (bTryAutoWrap)
				// Try and auto-wrap it - errors will leave Py exception set,
				return PyXPCOM_XPTStub::AutoWrapPythonInstance(ob, iid, ppv);
			PyErr_SetString(PyExc_TypeError, "The Python instance can not be converted to an XPCOM object");
			return PR_FALSE;
		} else
			ob = use_ob;

	} else {
		Py_INCREF(ob);
	}
	PRBool rc = InterfaceFromPyISupports(ob, iid, ppv);
	Py_DECREF(ob);
	return rc;
}


// Interface conversions
/*static*/void
Py_nsISupports::RegisterInterface( const nsIID &iid, PyTypeObject *t)
{
	if (mapIIDToType==NULL)
		mapIIDToType = PyDict_New();

	if (mapIIDToType) {
		PyObject *key = Py_nsIID::PyObjectFromIID(iid);
		if (key)
			PyDict_SetItem(mapIIDToType, key, (PyObject *)t);
		Py_XDECREF(key);
	}
}

/*static */PyObject *
Py_nsISupports::PyObjectFromInterfaceOrVariant(nsISupports *pis, 
				      const nsIID &riid, 
				      PRBool bAddRef, 
				      PRBool bMakeNicePyObject /* = PR_TRUE */)
{
	// Quick exit.
	if (pis==NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	if (riid.Equals(NS_GET_IID(nsIVariant))) {
		PyObject *ret = PyObject_FromVariant((nsIVariant *)pis);
		// If we were asked not to add a reference, then there
		// will be a spare reference on pis() - remove it.
		if (!bAddRef)
			pis->Release();
		return ret;
	}
	return PyObjectFromInterface(pis, riid, bAddRef, bMakeNicePyObject);
}

/*static */PyObject *
Py_nsISupports::PyObjectFromInterface(nsISupports *pis, 
				      const nsIID &riid, 
				      PRBool bAddRef, 
				      PRBool bMakeNicePyObject /* = PR_TRUE */)
{
	// Quick exit.
	if (pis==NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyTypeObject *createType = NULL;
	// If the IID is for nsISupports, dont bother with
	// a map lookup as we know the type!
	if (!riid.Equals(NS_GET_IID(nsISupports))) {
		// Look up the map
		PyObject *obiid = Py_nsIID::PyObjectFromIID(riid);
		if (!obiid) return NULL;

		if (mapIIDToType != NULL)
			createType = (PyTypeObject *)PyDict_GetItem(mapIIDToType, obiid);
		Py_DECREF(obiid);
	}
	if (createType==NULL)
		createType = Py_nsISupports::type;
	// Check it is indeed one of our types.
	if (!PyXPCOM_TypeObject::IsType(createType)) {
		PyErr_SetString(PyExc_RuntimeError, "The type map is invalid");
		return NULL;
	}
	// we can now safely cast the thing to a PyComTypeObject and use it
	PyXPCOM_TypeObject *myCreateType = (PyXPCOM_TypeObject *)createType;
	if (myCreateType->ctor==NULL) {
		PyErr_SetString(PyExc_TypeError, "The type does not declare a PyCom constructor");
		return NULL;
	}

	Py_nsISupports *ret = (*myCreateType->ctor)(pis, riid);
#ifdef _DEBUG_LIFETIMES
	PyXPCOM_LogF("XPCOM Object created at 0x%0xld, nsISupports at 0x%0xld",
		ret, ret->m_obj);
#endif
	if (ret && bAddRef && pis) pis->AddRef();
	if (ret && bMakeNicePyObject)
		return MakeInterfaceResult(ret, riid);
	return ret;
}

// Call back into Python, passing a raw nsIInterface object, getting back
// the object to actually pass to Python.
PyObject *
Py_nsISupports::MakeInterfaceResult(PyObject *pyis, 
			     const nsIID &iid)
{
	NS_PRECONDITION(pyis, "NULL pyobject!");
	PyObject *obIID = NULL;
	PyObject *args = NULL;
	PyObject *func = NULL;
	PyObject *mod = NULL;
	PyObject *ret = NULL;

	obIID = Py_nsIID::PyObjectFromIID(iid);
	if (obIID==NULL)
		goto done;

	if (g_obFuncMakeInterfaceCount==NULL) {
		PyObject *mod = PyImport_ImportModule("xpcom.client");
		if (mod) 
			g_obFuncMakeInterfaceCount = PyObject_GetAttrString(mod, "MakeInterfaceResult");
		Py_XDECREF(mod);
	}
	if (g_obFuncMakeInterfaceCount==NULL) goto done;

	args = Py_BuildValue("OO", pyis, obIID);
	if (args==NULL) goto done;
	ret = PyEval_CallObject(g_obFuncMakeInterfaceCount, args);
done:
	if (PyErr_Occurred()) {
		NS_ABORT_IF_FALSE(ret==NULL, "Have an error, but also a return val!");
		PyXPCOM_LogError("Creating an interface object to be used as a result failed\n");
		PyErr_Clear();
	}
	Py_XDECREF(mod);
	Py_XDECREF(args);
	Py_XDECREF(obIID);
	if (ret==NULL) // eek - error - return the original with no refcount mod.
		ret = pyis; 
	else
		// no error - decref the old object
		Py_DECREF(pyis);
	// return our obISupports.  If NULL, we are really hosed and nothing we can do.
	return ret;
}

// @pymethod <o Py_nsISupports>|Py_nsISupports|QueryInterface|Queries an object for a specific interface.
PyObject *
Py_nsISupports::QueryInterface(PyObject *self, PyObject *args)
{
	PyObject *obiid;
	int bWrap = 1;
	// @pyparm IID|iid||The IID requested.
	// @rdesc The result is always a <o Py_nsISupports> object.
	// Any error (including E_NOINTERFACE) will generate a <o com_error> exception.
	if (!PyArg_ParseTuple(args, "O|i:QueryInterface", &obiid, &bWrap))
		return NULL;

	nsIID	iid;
	if (!Py_nsIID::IIDFromPyObject(obiid, &iid))
		return NULL;

	nsISupports *pMyIS = GetI(self);
	if (pMyIS==NULL) return NULL;

	nsISupports *pis;
	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = pMyIS->QueryInterface(iid, (void **)&pis);
	Py_END_ALLOW_THREADS;

	/* Note that this failure may include E_NOINTERFACE */
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	/* Return a type based on the IID (with no extra ref) */
	return PyObjectFromInterface(pis, iid, PR_FALSE, (PRBool)bWrap);
}


// @object Py_nsISupports|The base object for all PythonCOM objects.  Wraps a COM nsISupports interface.
/*static*/ struct PyMethodDef 
Py_nsISupports::methods[] =
{
	{ "queryInterface", Py_nsISupports::QueryInterface, 1, "Queries the object for an interface."},
	{ "QueryInterface", Py_nsISupports::QueryInterface, 1, "An alias for queryInterface."},
	{NULL}
};

/*static*/void Py_nsISupports::InitType(void)
{
	type = new PyXPCOM_TypeObject(
		"nsISupports",
		NULL,
		sizeof(Py_nsISupports),
		methods,
		Constructor);
}

PyXPCOM_TypeObject *Py_nsISupports::type = NULL;
PyObject *Py_nsISupports::mapIIDToType = NULL;
