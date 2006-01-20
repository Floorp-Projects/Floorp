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

// Pyxpt_info.cpp - wrappers for the xpt_info objects.
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

PyObject *PyObject_FromXPTType( const nsXPTType *d)
{
	if (d==nsnull) {
		Py_INCREF(Py_None);
		return Py_None;
	}
    // build an object using the same format as a TypeDescriptor.
	return Py_BuildValue("bzzz", 
		d->flags,
		NULL, NULL, NULL);
}

PyObject *PyObject_FromXPTTypeDescriptor( const XPTTypeDescriptor *d)
{
	if (d==nsnull) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return Py_BuildValue("bbbh", 
		d->prefix.flags,
		d->argnum,
		d->argnum2,
		d->type.iface // this is actually a union!
		);
}

PyObject *PyObject_FromXPTParamDescriptor( const XPTParamDescriptor *d)
{
	if (d==nsnull) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *ob = PyObject_FromXPTTypeDescriptor(&d->type);
	PyObject *ret = Py_BuildValue("bO", d->flags, ob);
	Py_DECREF(ob);
	return ret;
}

PyObject *PyObject_FromXPTMethodDescriptor( const XPTMethodDescriptor *d)
{
	if (d==nsnull) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *ob_params = PyTuple_New(d->num_args);
	if (ob_params==NULL)
		return NULL;
	for (int i=0;i<d->num_args;i++)
		PyTuple_SET_ITEM(ob_params, i, PyObject_FromXPTParamDescriptor(d->params+i));
	PyObject *ob_ret = PyObject_FromXPTParamDescriptor(d->result);
	PyObject *ret = Py_BuildValue("bsOO", d->flags, d->name, ob_params, ob_ret);
	Py_XDECREF(ob_ret);
	Py_XDECREF(ob_params);
	return ret;
}

PyObject *PyObject_FromXPTConstant( const XPTConstDescriptor *c)
{
	if (c==nsnull) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyObject *ob_type = PyObject_FromXPTTypeDescriptor(&c->type);
	if (ob_type==NULL)
		return NULL;
	PyObject *v = NULL;
	switch (c->type.prefix.flags) {
		case TD_INT8:
			v = PyInt_FromLong( c->value.i8 );
			break;
		case TD_INT16:
			v = PyInt_FromLong( c->value.i16 );
			break;
		case TD_INT32:
			v = PyInt_FromLong( c->value.i32 );
			break;
		case TD_INT64:
			v = PyLong_FromLongLong(c->value.i64);
			break;
		case TD_UINT8:
			v = PyInt_FromLong( c->value.ui8 );
			break;
		case TD_UINT16:
			v = PyInt_FromLong( c->value.ui16 );
			break;
		case TD_UINT32:
			v = PyInt_FromLong( c->value.ui32 );
			break;
		case TD_UINT64:
			v = PyLong_FromUnsignedLongLong(c->value.ui64);
			break;
		case TD_FLOAT:
			v = PyFloat_FromDouble(c->value.flt);
			break;
		case TD_DOUBLE:
			v = PyFloat_FromDouble(c->value.dbl);
			break;
		case TD_BOOL:
			v = c->value.bul ? Py_True : Py_False;
			Py_INCREF(v);
			break;
		case TD_CHAR:
			v = PyString_FromStringAndSize(&c->value.ch, 1);
			break;
		case TD_WCHAR:
			v = PyObject_FromNSString((PRUnichar *)&c->value.wch, 1);
			break;
	//    TD_VOID              = 13,  
		case TD_PNSIID:
			v = Py_nsIID::PyObjectFromIID(*c->value.iid);
			break;
	//    TD_DOMSTRING         = 15,
		case TD_PSTRING:
			v = PyString_FromString(c->value.str);
			break;
		case TD_PWSTRING:
			v = PyObject_FromNSString((PRUnichar *)c->value.wstr, nsCRT::strlen((PRUnichar *)c->value.wstr));
			break;
	//    TD_INTERFACE_TYPE    = 18,
	//    TD_INTERFACE_IS_TYPE = 19,
	//    TD_ARRAY             = 20,
	//    TD_PSTRING_SIZE_IS   = 21,
	//    TD_PWSTRING_SIZE_IS  = 22
	//    TD_UTF8STRING        = 23,
	//    TD_CSTRING           = 24,
	//    TD_ASTRING           = 25
		default:
			v = PyString_FromString("Unknown type code!!");
			break;

	}
	PyObject *ret = Py_BuildValue("sbO", c->name, ob_type, v);
	Py_DECREF(ob_type);
	Py_DECREF(v);
	return ret;
}
