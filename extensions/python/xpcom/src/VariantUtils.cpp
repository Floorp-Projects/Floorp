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
#include <nsIInterfaceInfoManager.h>
#include <nsAString.h>
#include <nsString.h>
#include <nsReadableUtils.h>

// ------------------------------------------------------------------------
// nsString utilities
// ------------------------------------------------------------------------
// one day we may know what they look like.
inline
PRBool
IsNullDOMString( const nsAString& aString )
{
	return PR_FALSE;
}

inline
PRBool
IsNullDOMString( const nsACString& aString )
{
	return PR_FALSE;
}

PyObject *PyObject_FromNSString( const nsACString &s, PRBool bAssumeUTF8 = PR_FALSE )
{
	PyObject *ret;
	if (IsNullDOMString(s)) {
		ret = Py_None;
		Py_INCREF(Py_None);
	} else {
		if (bAssumeUTF8) {
			char *temp = ToNewCString(s);
			ret = PyUnicode_DecodeUTF8(temp, s.Length(), NULL);
			nsMemory::Free(temp);
		} else {
			ret = PyString_FromStringAndSize(NULL, s.Length());
			// Need "CopyAsciiTo"!?
			nsACString::const_iterator fromBegin, fromEnd;
			char* dest = PyString_AS_STRING(ret);
			copy_string(s.BeginReading(fromBegin), s.EndReading(fromEnd), dest);
		}
	}
	return ret;
}

PyObject *PyObject_FromNSString( const nsAString &s )
{
	PyObject *ret;
	if (IsNullDOMString(s)) {
		ret = Py_None;
		Py_INCREF(Py_None);
	} else {
		ret = PyUnicodeUCS2_FromUnicode( NULL, s.Length() );
		CopyUnicodeTo(s, 0, PyUnicodeUCS2_AsUnicode(ret), s.Length());
	}
	return ret;
}

// Array utilities
static PRUint32 GetArrayElementSize( PRUint8 t)
{
	PRUint32 ret;
	switch (t & XPT_TDP_TAGMASK) {
		case nsXPTType::T_U8:
		case nsXPTType::T_I8:
			ret = sizeof(PRInt8); 
			break;
		case nsXPTType::T_I16:
		case nsXPTType::T_U16:
			ret = sizeof(PRInt16); 
			break;
		case nsXPTType::T_I32:
		case nsXPTType::T_U32:
			ret = sizeof(PRInt32); 
			break;
		case nsXPTType::T_I64:
		case nsXPTType::T_U64:
			ret = sizeof(PRInt64); 
			break;
		case nsXPTType::T_FLOAT:
			ret = sizeof(float); 
			break;
		case nsXPTType::T_DOUBLE:
			ret = sizeof(double); 
			break;
		case nsXPTType::T_BOOL:
			ret = sizeof(PRBool); 
			break;
		case nsXPTType::T_CHAR:
			ret = sizeof(char); 
			break;
		case nsXPTType::T_WCHAR:
			ret = sizeof(PRUnichar); 
			break;
		case nsXPTType::T_IID:
		case nsXPTType::T_CHAR_STR:
		case nsXPTType::T_WCHAR_STR:
		case nsXPTType::T_INTERFACE:
		case nsXPTType::T_DOMSTRING:
		case nsXPTType::T_INTERFACE_IS:
		case nsXPTType::T_PSTRING_SIZE_IS:
		case nsXPTType::T_CSTRING:
		case nsXPTType::T_ASTRING:
		case nsXPTType::T_UTF8STRING:

			ret = sizeof( void * );
			break;
	default:
		NS_ABORT_IF_FALSE(0, "Unknown array type code!");
		ret = 0;
		break;
	}
	return ret;
}

void FreeSingleArray(void *array_ptr, PRUint32 sequence_size, PRUint8 array_type)
{
	// Free each array element - NOT the array itself
	// Thus, we only need to free arrays or pointers.
	void **p = (void **)array_ptr;
	PRUint32 i;
	switch(array_type & XPT_TDP_TAGMASK) {
		case nsXPTType::T_IID:
		case nsXPTType::T_CHAR_STR:
		case nsXPTType::T_WCHAR_STR:
			for (i=0; i<sequence_size; i++)
				if (p[i]) nsMemory::Free(p[i]);
			break;
		case nsXPTType::T_INTERFACE:
		case nsXPTType::T_INTERFACE_IS:
			for (i=0; i<sequence_size; i++)
				if (p[i]) {
					Py_BEGIN_ALLOW_THREADS; // MUST release thread-lock, incase a Python COM object that re-acquires.
					((nsISupports *)p[i])->Release();
					Py_END_ALLOW_THREADS;
				}
			break;

		// Ones we know need no deallocation
		case nsXPTType::T_U8:
		case nsXPTType::T_I8:
		case nsXPTType::T_I16:
		case nsXPTType::T_U16:
		case nsXPTType::T_I32:
		case nsXPTType::T_U32:
		case nsXPTType::T_I64:
		case nsXPTType::T_U64:
		case nsXPTType::T_FLOAT:
		case nsXPTType::T_DOUBLE:
		case nsXPTType::T_BOOL:
		case nsXPTType::T_CHAR:
		case nsXPTType::T_WCHAR:
			break;

		// And a warning should new type codes appear, as they may need deallocation.
		default:
			PyXPCOM_LogWarning("Deallocating unknown type %d (0x%x) - possible memory leak\n");
			break;
	}
}

#define FILL_SIMPLE_POINTER( type, val ) *((type *)pthis) = (type)(val);
#define BREAK_FALSE {rc=PR_FALSE;break;}


PRBool FillSingleArray(void *array_ptr, PyObject *sequence_ob, PRUint32 sequence_size, PRUint32 array_element_size, PRUint8 array_type)
{
	PRUint8 *pthis = (PRUint8 *)array_ptr;
	NS_ABORT_IF_FALSE(pthis, "Don't have a valid array to fill!");
	PRBool rc = PR_TRUE;
	// We handle T_U8 specially as a string/Unicode.
	// If it is NOT a string, we just fall through and allow the standard
	// sequence unpack code process it (just slower!)
	if ( array_type == nsXPTType::T_U8 && 
		(PyString_Check(sequence_ob) || PyUnicode_Check(sequence_ob))) {

		PRBool release_seq;
		if (PyUnicode_Check(sequence_ob)) {
			release_seq = PR_TRUE;
			sequence_ob = PyObject_Str(sequence_ob);
		} else
			release_seq = PR_FALSE;
		if (!sequence_ob) // presumably a memory error, or Unicode encoding error.
			return PR_FALSE;
		memcpy(pthis, PyString_AS_STRING(sequence_ob), sequence_size);
		if (release_seq)
			Py_DECREF(sequence_ob);
		return PR_TRUE;
	}

	for (PRUint32 i=0; rc && i<sequence_size; i++,pthis += array_element_size) {
		PyObject *val = PySequence_GetItem(sequence_ob, i);
		PyObject *val_use = NULL;
		if (val==NULL)
			return PR_FALSE;
		switch(array_type) {
			  case nsXPTType::T_I8:
				if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE;
				FILL_SIMPLE_POINTER( PRInt8, PyInt_AsLong(val_use) );
				break;
			  case nsXPTType::T_I16:
				if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE;
				FILL_SIMPLE_POINTER( PRInt16, PyInt_AsLong(val_use) );
				break;
			  case nsXPTType::T_I32:
				if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE;
				FILL_SIMPLE_POINTER( PRInt32, PyInt_AsLong(val_use) );
				break;
			  case nsXPTType::T_I64:
				if ((val_use=PyNumber_Long(val))==NULL) BREAK_FALSE;
				FILL_SIMPLE_POINTER( PRInt64, PyLong_AsLongLong(val_use) );
				break;
			  case nsXPTType::T_U8:
				if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE;
				FILL_SIMPLE_POINTER( PRUint8, PyInt_AsLong(val_use) );
				break;
			  case nsXPTType::T_U16:
				if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE;
				FILL_SIMPLE_POINTER( PRUint16, PyInt_AsLong(val_use) );
				break;
			  case nsXPTType::T_U32:
				if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE;
				FILL_SIMPLE_POINTER( PRUint32, PyInt_AsLong(val_use) );
				break;
			  case nsXPTType::T_U64:
				if ((val_use=PyNumber_Long(val))==NULL) BREAK_FALSE;
				FILL_SIMPLE_POINTER( PRUint64, PyLong_AsUnsignedLongLong(val_use) );
				break;
			  case nsXPTType::T_FLOAT:
				if ((val_use=PyNumber_Float(val))==NULL) BREAK_FALSE
				FILL_SIMPLE_POINTER( float, PyFloat_AsDouble(val_use) );
				break;
			  case nsXPTType::T_DOUBLE:
				if ((val_use=PyNumber_Float(val))==NULL) BREAK_FALSE
				FILL_SIMPLE_POINTER( double, PyFloat_AsDouble(val_use) );
				break;
			  case nsXPTType::T_BOOL:
				if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE
				FILL_SIMPLE_POINTER( PRBool, PyInt_AsLong(val_use) );
				break;
			  case nsXPTType::T_CHAR:
				if (!PyString_Check(val) && !PyUnicode_Check(val)) {
					PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
					BREAK_FALSE;
				}
				if ((val_use = PyObject_Str(val))==NULL)
					BREAK_FALSE;
				// Sanity check should PyObject_Str() ever loosen its semantics wrt Unicode!
				NS_ABORT_IF_FALSE(PyString_Check(val_use), "PyObject_Str didnt return a string object!");
				FILL_SIMPLE_POINTER( char, *PyString_AS_STRING(val_use) );
				break;

			  case nsXPTType::T_WCHAR:
				if (!PyString_Check(val) && !PyUnicode_Check(val)) {
					PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
					BREAK_FALSE;
				}
				if ((val_use = PyUnicodeUCS2_FromObject(val))==NULL)
					BREAK_FALSE;
				NS_ABORT_IF_FALSE(PyUnicode_Check(val_use), "PyUnicodeUCS2_FromObject didnt return a Unicode object!");
				FILL_SIMPLE_POINTER( PRUnichar, *PyUnicode_AS_UNICODE(val_use) );
				break;

			  case nsXPTType::T_IID: {
				nsIID iid;
				if (!Py_nsIID::IIDFromPyObject(val, &iid))
					BREAK_FALSE;
				nsIID **pp = (nsIID **)pthis;
				// If there is an existing IID, free it.
				if (*pp)
					nsMemory::Free(*pp);
				*pp = (nsIID *)nsMemory::Alloc(sizeof(nsIID));
				if (*pp==NULL) {
					PyErr_NoMemory();
					BREAK_FALSE;
				}
				memcpy(*pp, &iid, sizeof(iid));
				break;
				}

		//          case nsXPTType::T_BSTR:

			  case nsXPTType::T_CHAR_STR: {
				// If it is an existing string, free it.
				char **pp = (char **)pthis;
				if (*pp)
					nsMemory::Free(*pp);
				*pp = nsnull;

				if (val == Py_None)
					break; // Remains NULL.
				if (!PyString_Check(val) && !PyUnicode_Check(val)) {
					PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
					BREAK_FALSE;
				}
				if ((val_use = PyObject_Str(val))==NULL)
					BREAK_FALSE;
				// Sanity check should PyObject_Str() ever loosen its semantics wrt Unicode!
				NS_ABORT_IF_FALSE(PyString_Check(val_use), "PyObject_Str didnt return a string object!");

				const char *sz = PyString_AS_STRING(val_use);
				int nch = PyString_GET_SIZE(val_use);

				*pp = (char *)nsMemory::Alloc(nch+1);
				if (*pp==NULL) {
					PyErr_NoMemory();
					BREAK_FALSE;
				}
				strncpy(*pp, sz, nch+1);
				break;
				}
			  case nsXPTType::T_WCHAR_STR: {
				// If it is an existing string, free it.
				PRUnichar **pp = (PRUnichar **)pthis;
				if (*pp)
					nsMemory::Free(*pp);
				*pp = nsnull;
				if (val == Py_None)
					break; // Remains NULL.
				if (!PyString_Check(val) && !PyUnicode_Check(val)) {
					PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
					BREAK_FALSE;
				}
				if ((val_use = PyUnicodeUCS2_FromObject(val))==NULL)
					BREAK_FALSE;
				NS_ABORT_IF_FALSE(PyUnicode_Check(val_use), "PyUnicodeUCS2_FromObject didnt return a Unicode object!");
				const PRUnichar *sz = PyUnicode_AS_UNICODE(val_use);
				int nch = PyUnicode_GET_SIZE(val_use);

				*pp = (PRUnichar *)nsMemory::Alloc(sizeof(PRUnichar) * (nch+1));
				if (*pp==NULL) {
					PyErr_NoMemory();
					BREAK_FALSE;
				}
				memcpy(*pp, sz, sizeof(PRUnichar) * (nch + 1));
				break;
				}
			  case nsXPTType::T_INTERFACE_IS: // hmm - ignoring the IID can't be good :(
			  case nsXPTType::T_INTERFACE:  {
				// We do allow NULL here, even tho doing so will no-doubt crash some objects.
				// (but there will certainly be objects out there that will allow NULL :-(
				nsISupports *pnew;
				if (!Py_nsISupports::InterfaceFromPyObject(val, NS_GET_IID(nsISupports), &pnew, PR_TRUE))
					BREAK_FALSE;
				nsISupports **pp = (nsISupports **)pthis;
				if (*pp) {
					Py_BEGIN_ALLOW_THREADS; // MUST release thread-lock, incase a Python COM object that re-acquires.
					(*pp)->Release();
					Py_END_ALLOW_THREADS;
				}
				*pp = pnew; // ref-count added by InterfaceFromPyObject
				break;
				}
			default:
				// try and limp along in this case.
				// leave rc TRUE
				PyXPCOM_LogWarning("Converting Python object for an array element - The object type (0x%x) is unknown - leaving param alone!\n", array_type);
				break;
		}
		Py_XDECREF(val_use);
		Py_DECREF(val);
	}
	return rc;	
}

PyObject *UnpackSingleArray(void *array_ptr, PRUint32 sequence_size, PRUint8 array_type, nsIID *iid)
{
	if (array_ptr==NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	if (array_type == nsXPTType::T_U8)
		return PyString_FromStringAndSize( (char *)array_ptr, sequence_size );

	PRUint32 array_element_size = GetArrayElementSize(array_type);
	PyObject *list_ret = PyList_New(sequence_size);
	PRUint8 *pthis = (PRUint8 *)array_ptr;
	for (PRUint32 i=0; i<sequence_size; i++,pthis += array_element_size) {
		PyObject *val = NULL;
		switch(array_type) {
			  case nsXPTType::T_I8:
				val = PyInt_FromLong( *((PRInt8 *)pthis) );
				break;
			  case nsXPTType::T_I16:
				val = PyInt_FromLong( *((PRInt16 *)pthis) );
				break;
			  case nsXPTType::T_I32:
				val = PyInt_FromLong( *((PRInt32 *)pthis) );
				break;
			  case nsXPTType::T_I64:
				val = PyLong_FromLongLong( *((PRInt64 *)pthis) );
				break;
			  // case nsXPTType::T_U8 - handled above!
			  case nsXPTType::T_U16:
				val = PyInt_FromLong( *((PRUint16 *)pthis) );
				break;
			  case nsXPTType::T_U32:
				val = PyInt_FromLong( *((PRUint32 *)pthis) );
				break;
			  case nsXPTType::T_U64:
				val = PyLong_FromUnsignedLongLong( *((PRUint64 *)pthis) );
				break;
			  case nsXPTType::T_FLOAT:
				val = PyFloat_FromDouble( *((float*)pthis) );
				break;
			  case nsXPTType::T_DOUBLE:
				val = PyFloat_FromDouble( *((double*)pthis) );
				break;
			  case nsXPTType::T_BOOL:
				val = (*((PRBool *)pthis)) ? Py_True : Py_False;
				Py_INCREF(val);
				break;
			  case nsXPTType::T_IID:
				val = Py_nsIID::PyObjectFromIID( **((nsIID **)pthis) );
				break;

			  case nsXPTType::T_CHAR_STR: {
				char **pp = (char **)pthis;
				if (*pp==NULL) {
					Py_INCREF(Py_None);
					val = Py_None;
				} else
					val = PyString_FromString(*pp);
				break;
				}
			  case nsXPTType::T_WCHAR_STR: {
				PRUnichar **pp = (PRUnichar **)pthis;
				if (*pp==NULL) {
					Py_INCREF(Py_None);
					val = Py_None;
				} else
					val = PyUnicodeUCS2_FromUnicode( *pp, nsCRT::strlen(*pp) );
				break;
				}
			  case nsXPTType::T_INTERFACE_IS:
			  case nsXPTType::T_INTERFACE: {
				nsISupports **pp = (nsISupports **)pthis;
				val = Py_nsISupports::PyObjectFromInterface(*pp, iid ? *iid : NS_GET_IID(nsISupports), PR_TRUE);
				break;
				}
			  default: {
				char buf[128];
				sprintf(buf, "Unknown XPCOM array type flags (0x%x)", array_type);
				PyXPCOM_LogWarning("%s - returning a string object with this message!\n", buf);
				val = PyString_FromString(buf);
				break;
				}
		}
		if (val==NULL) {
			NS_ABORT_IF_FALSE(PyErr_Occurred(), "NULL result in array conversion, but no error set!");
			return NULL;
		}
		PyList_SET_ITEM(list_ret, i, val); // ref-count consumed.
	}
	return list_ret;
}


// ------------------------------------------------------------------------
// nsIVariant utilities
// ------------------------------------------------------------------------
struct BVFTResult {
	BVFTResult() {pis = NULL;iid=Py_nsIID_NULL;}
	nsISupports *pis;
	nsIID iid;
};

static PRUint16 BestVariantTypeForPyObject( PyObject *ob, BVFTResult *pdata = NULL) 
{
	nsISupports *ps = NULL;
	nsIID iid;
	// start with some fast concrete checks.
	if (ob==Py_None)
		return nsIDataType::VTYPE_EMPTY;
	if (ob==Py_True || ob == Py_False)
		return nsIDataType::VTYPE_BOOL;
	if (PyInt_Check(ob))
		return nsIDataType::VTYPE_INT32;
	if (PyLong_Check(ob))
		return nsIDataType::VTYPE_INT64;
	if (PyFloat_Check(ob))
		return nsIDataType::VTYPE_DOUBLE;
	if (PyString_Check(ob))
		return nsIDataType::VTYPE_STRING_SIZE_IS;
	if (PyUnicode_Check(ob))
		return nsIDataType::VTYPE_WSTRING_SIZE_IS;
	if (PyTuple_Check(ob) || PyList_Check(ob))
		return PySequence_Length(ob) ? nsIDataType::VTYPE_ARRAY : nsIDataType::VTYPE_EMPTY_ARRAY;
	// Now do expensive or abstract checks.
	if (Py_nsISupports::InterfaceFromPyObject(ob, NS_GET_IID(nsISupports), &ps, PR_TRUE)) {
		if (pdata) {
			pdata->pis = ps;
			pdata->iid = NS_GET_IID(nsISupports);
		} else
			ps->Release();
		return nsIDataType::VTYPE_INTERFACE_IS;
	} else
		PyErr_Clear();
	if (Py_nsIID::IIDFromPyObject(ob, &iid)) {
		if (pdata)
			pdata->iid = iid;
		return nsIDataType::VTYPE_ID;
	} else
		PyErr_Clear();
	if (PySequence_Check(ob))
		return PySequence_Length(ob) ? nsIDataType::VTYPE_ARRAY : nsIDataType::VTYPE_EMPTY_ARRAY;
	return (PRUint16)-1;
}

nsIVariant *PyObject_AsVariant( PyObject *ob)
{
	nsresult nr = NS_ERROR_UNEXPECTED;
	nsCOMPtr<nsIWritableVariant> v = do_CreateInstance("@mozilla.org/variant;1", &nr); 
	if (NS_FAILED(nr)) {
		PyXPCOM_BuildPyException(nr);
		return NULL;
	}
	// *sigh* - I tried the abstract API (PyNumber_Check, etc)
	// but our COM instances too often qualify.
	BVFTResult cvt_result;
	PRUint16 dt = BestVariantTypeForPyObject(ob, &cvt_result);
	switch (dt) {
		case nsIDataType::VTYPE_BOOL:
			nr = v->SetAsBool(ob==Py_True);
			break;
		case nsIDataType::VTYPE_INT32:
			nr = v->SetAsInt32(PyInt_AsLong(ob));
			break;
		case nsIDataType::VTYPE_INT64:
			nr = v->SetAsInt64(PyLong_AsLongLong(ob));
			break;
		case nsIDataType::VTYPE_DOUBLE:
			nr = v->SetAsDouble(PyFloat_AsDouble(ob));
			break;
		case nsIDataType::VTYPE_STRING_SIZE_IS:
			nr = v->SetAsStringWithSize(PyString_Size(ob), PyString_AsString(ob));
			break;
		case nsIDataType::VTYPE_WSTRING_SIZE_IS:
			nr = v->SetAsWStringWithSize(PyUnicodeUCS2_GetSize(ob), PyUnicodeUCS2_AsUnicode(ob));
			break;
		case nsIDataType::VTYPE_INTERFACE_IS:
		{
			nsISupports *ps = cvt_result.pis;
			nr = v->SetAsInterface(cvt_result.iid, ps);
			if (ps) {
				Py_BEGIN_ALLOW_THREADS; // MUST release thread-lock, incase a Python COM object that re-acquires.
				ps->Release();
				Py_END_ALLOW_THREADS;
			}
			break;
		}
		case nsIDataType::VTYPE_ID:
			nr = v->SetAsID(cvt_result.iid);
			break;
		case nsIDataType::VTYPE_ARRAY:
		{
			int seq_length = PySequence_Length(ob);
			NS_ABORT_IF_FALSE(seq_length!=0, "VTYPE_ARRAY assumes at least one element!");
			PyObject *first = PySequence_GetItem(ob, 0);
			if (!first) break;
			int array_type = BestVariantTypeForPyObject(first);
			Py_DECREF(first);
			// Arrays can't handle all types.  This means we lost embedded NULLs.
			// This should really be fixed in XPCOM.
			if (array_type == nsIDataType::VTYPE_STRING_SIZE_IS) array_type = nsIDataType::VTYPE_CHAR_STR;
			if (array_type == nsIDataType::VTYPE_WSTRING_SIZE_IS) array_type = nsIDataType::VTYPE_WCHAR_STR;
			PRUint32 element_size = GetArrayElementSize(array_type);
			int cb_buffer_pointer = seq_length * element_size;
			void *buffer_pointer;
			if ((buffer_pointer = (void *)nsMemory::Alloc(cb_buffer_pointer)) == nsnull) {
				PyErr_NoMemory();
				nr = NS_ERROR_UNEXPECTED;
				break;
			}
			memset(buffer_pointer, 0, cb_buffer_pointer);
			if (FillSingleArray(buffer_pointer, ob, seq_length, element_size, array_type)) {
				nr = v->SetAsArray(array_type, &NS_GET_IID(nsISupports), seq_length, buffer_pointer);
				FreeSingleArray(buffer_pointer, seq_length, array_type);
			} else
				nr = NS_ERROR_UNEXPECTED;
			nsMemory::Free(buffer_pointer);
			break;
		}
		case nsIDataType::VTYPE_EMPTY:
			v->SetAsEmpty();
			break;
		case nsIDataType::VTYPE_EMPTY_ARRAY:
			v->SetAsEmptyArray();
			break;
		case (PRUint16)-1:
			PyErr_Format(PyExc_TypeError, "Objects of type '%s' can not be converted to an nsIVariant", ob->ob_type->tp_name);
			return NULL;
		default:
			NS_ABORT_IF_FALSE(0, "BestVariantTypeForPyObject() returned a variant type not handled here!");
			PyErr_Format(PyExc_TypeError, "Objects of type '%s' can not be converted to an nsIVariant", ob->ob_type->tp_name);
			return NULL;
	}
	nsIVariant *ret;
	v->QueryInterface(NS_GET_IID(nsIVariant), (void **)&ret);
	return ret;
}

static PyObject *MyBool_FromBool(PRBool v)
{
	PyObject *ret = v ? Py_True : Py_False;
	Py_INCREF(ret);
	return ret;
}
static PyObject *MyObject_FromInterface(nsISupports *p)
{
	return Py_nsISupports::PyObjectFromInterface(p, NS_GET_IID(nsISupports), PR_FALSE);
}

#define GET_FROM_V(Type, FuncGet, FuncConvert) { \
	Type t; \
	if (NS_FAILED(nr = FuncGet( &t ))) goto done;\
	ret = FuncConvert(t);\
	break; \
}

PyObject *PyObject_FromVariantArray( nsIVariant *v)
{
	nsresult nr;
	NS_PRECONDITION(v, "NULL variant!");
	if (!v)
		return PyXPCOM_BuildPyException(NS_ERROR_INVALID_POINTER);
#ifdef NS_DEBUG
	PRUint16 dt;
	nr = v->GetDataType(&dt);
	NS_ABORT_IF_FALSE(dt == nsIDataType::VTYPE_ARRAY, "expected an array variant");
#endif
	nsIID iid;
	void *p;
	PRUint16 type;
	PRUint32 count;
	nr = v->GetAsArray(&type, &iid, &count, &p);
	if (NS_FAILED(nr)) return PyXPCOM_BuildPyException(nr);
	PyObject *ret = UnpackSingleArray(p, count, (PRUint8)type, &iid);
	FreeSingleArray(p, count, (PRUint8)type);
	nsMemory::Free(p);
	return ret;
}

PyObject *PyObject_FromVariant( nsIVariant *v)
{
	if (!v) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	PRUint16 dt;
	nsresult nr;
	PyObject *ret = NULL;
	nr = v->GetDataType(&dt);
	if (NS_FAILED(nr)) goto done;
	switch (dt) {
		case nsIDataType::VTYPE_VOID:
		case nsIDataType::VTYPE_EMPTY:
		case nsIDataType::VTYPE_EMPTY_ARRAY:
			ret = Py_None;
			Py_INCREF(Py_None);
			break;
		case nsIDataType::VTYPE_ARRAY:
			ret = PyObject_FromVariantArray(v);
			break;
		case nsIDataType::VTYPE_INT8:
		case nsIDataType::VTYPE_INT16:
		case nsIDataType::VTYPE_INT32:
			GET_FROM_V(PRInt32, v->GetAsInt32, PyInt_FromLong);
		case nsIDataType::VTYPE_UINT8:
		case nsIDataType::VTYPE_UINT16:
		case nsIDataType::VTYPE_UINT32:
			GET_FROM_V(PRUint32, v->GetAsUint32, PyLong_FromUnsignedLong);
		case nsIDataType::VTYPE_INT64:
			GET_FROM_V(PRInt64, v->GetAsInt64, PyLong_FromLongLong);
		case nsIDataType::VTYPE_UINT64:
			GET_FROM_V(PRUint64, v->GetAsUint64, PyLong_FromUnsignedLongLong);
		case nsIDataType::VTYPE_FLOAT:
		case nsIDataType::VTYPE_DOUBLE:
			GET_FROM_V(double, v->GetAsDouble, PyFloat_FromDouble);
		case nsIDataType::VTYPE_BOOL:
			GET_FROM_V(PRBool, v->GetAsBool, MyBool_FromBool);
		default:
			PyXPCOM_LogWarning("Converting variant to Python object - variant type '%d' unknown - using string.\n", dt);
		// Fall through to the string case
		case nsIDataType::VTYPE_CHAR:
		case nsIDataType::VTYPE_CHAR_STR:
		case nsIDataType::VTYPE_STRING_SIZE_IS:
		case nsIDataType::VTYPE_CSTRING: {
			nsCAutoString s;
			if (NS_FAILED(nr=v->GetAsACString(s))) goto done;
			ret = PyObject_FromNSString(s);
			break;
		}
		case nsIDataType::VTYPE_WCHAR:
		case nsIDataType::VTYPE_DOMSTRING:
		case nsIDataType::VTYPE_WSTRING_SIZE_IS:
		case nsIDataType::VTYPE_ASTRING: {
			nsAutoString s;
			if (NS_FAILED(nr=v->GetAsAString(s))) goto done;
			ret = PyObject_FromNSString(s);
			break;
		}
		case nsIDataType::VTYPE_ID:
			GET_FROM_V(nsIID, v->GetAsID, Py_nsIID::PyObjectFromIID);
		case nsIDataType::VTYPE_INTERFACE:
			GET_FROM_V(nsISupports *, v->GetAsISupports, MyObject_FromInterface);
		case nsIDataType::VTYPE_INTERFACE_IS: {
			nsISupports *p;
			nsIID *iid;
			if (NS_FAILED(nr=v->GetAsInterface(&iid, (void **)&p))) goto done;
			ret = Py_nsISupports::PyObjectFromInterface(p, *iid, PR_FALSE);
			break;
		}
	}
done:
	if (NS_FAILED(nr)) {
		NS_ABORT_IF_FALSE(ret==NULL, "Have an error, but also a return val!");
		PyXPCOM_BuildPyException(nr);
	}
	return ret;
}

// ------------------------------------------------------------------------
// TypeDescriptor helper class
// ------------------------------------------------------------------------
class PythonTypeDescriptor {
public:
	PythonTypeDescriptor() {
		param_flags = type_flags = argnum = argnum2 = 0;
		extra = NULL;
		is_auto_out = PR_FALSE;
		is_auto_in = PR_FALSE;
		have_set_auto = PR_FALSE;
	}
	~PythonTypeDescriptor() {
		Py_XDECREF(extra);
	}
	PRUint8 param_flags;
	PRUint8 type_flags;
	PRUint8 argnum;                 /* used for iid_is and size_is */
	PRUint8 argnum2;                /* used for length_is */
	PyObject *extra; // The IID object, or the type of the array.
	// Extra items to help our processing.
	// Is this auto-filled by some other "in" param?
	PRBool is_auto_in;
	// Is this auto-filled by some other "out" param?
	PRBool is_auto_out;
	// If is_auto_out, have I already filled it?  Used when multiple
	// params share a size_is fields - first time sets it, subsequent
	// time check it.
	PRBool have_set_auto; 
};

static int ProcessPythonTypeDescriptors(PythonTypeDescriptor *pdescs, int num)
{
	// Loop over the array, checking all the params marked as having an arg.
	// If these args nominate another arg as the size_is param, then
	// we reset the size_is param to _not_ requiring an arg.
	int i;
	for (i=0;i<num;i++) {
		PythonTypeDescriptor &ptd = pdescs[i];
		// Can't use XPT_TDP_TAG() - it uses a ".flags" reference in the macro.
		switch (ptd.type_flags & XPT_TDP_TAGMASK) {
			case nsXPTType::T_ARRAY:
				NS_ABORT_IF_FALSE(ptd.argnum < num, "Bad dependent index");
				if (ptd.argnum2 < num) {
					if (XPT_PD_IS_IN(ptd.param_flags))
						pdescs[ptd.argnum2].is_auto_in = PR_TRUE;
					if (XPT_PD_IS_OUT(ptd.param_flags))
						pdescs[ptd.argnum2].is_auto_out = PR_TRUE;
				}
				break;
			case nsXPTType::T_PSTRING_SIZE_IS:
			case nsXPTType::T_PWSTRING_SIZE_IS:
				NS_ABORT_IF_FALSE(ptd.argnum < num, "Bad dependent index");
				if (ptd.argnum < num) {
					if (XPT_PD_IS_IN(ptd.param_flags))
						pdescs[ptd.argnum].is_auto_in = PR_TRUE;
					if (XPT_PD_IS_OUT(ptd.param_flags))
						pdescs[ptd.argnum].is_auto_out = PR_TRUE;
				}
				break;
			default:
				break;
		}
	}
	int total_params_needed = 0;
	for (i=0;i<num;i++)
		if (XPT_PD_IS_IN(pdescs[i].param_flags) && !pdescs[i].is_auto_in && !XPT_PD_IS_DIPPER(pdescs[i].param_flags))
			total_params_needed++;

	return total_params_needed;
}

/*************************************************************************
**************************************************************************

Helpers when CALLING interfaces.

**************************************************************************
*************************************************************************/

PyXPCOM_InterfaceVariantHelper::PyXPCOM_InterfaceVariantHelper()
{
	m_var_array=nsnull;
	m_buffer_array=nsnull;
	m_pyparams=nsnull;
	m_num_array = 0;
}

PyXPCOM_InterfaceVariantHelper::~PyXPCOM_InterfaceVariantHelper()
{
	Py_XDECREF(m_pyparams);
	for (int i=0;i<m_num_array;i++) {
		if (m_var_array) {
			nsXPTCVariant &ns_v = m_var_array[i];
			if (ns_v.IsValInterface()) {
				if (ns_v.val.p) {
					Py_BEGIN_ALLOW_THREADS; // MUST release thread-lock, incase a Python COM object that re-acquires.
					((nsISupports *)ns_v.val.p)->Release();
					Py_END_ALLOW_THREADS;
				}
			}
			if (ns_v.IsValDOMString() && ns_v.val.p) {
				delete (const nsAString *)ns_v.val.p;
			}
			if (ns_v.IsValCString() && ns_v.val.p) {
				delete (const nsACString *)ns_v.val.p;
			}
			if (ns_v.IsValUTF8String() && ns_v.val.p) {
				delete (const nsACString *)ns_v.val.p;
			}
			if (ns_v.IsValArray()) {
				nsXPTCVariant &ns_v = m_var_array[i];
				if (ns_v.val.p) {
					PRUint8 array_type = (PRUint8)PyInt_AsLong(m_python_type_desc_array[i].extra);
					PRUint32 seq_size = GetSizeIs(i, PR_FALSE);
					FreeSingleArray(ns_v.val.p, seq_size, array_type);
				}
			}
			// IsOwned must be the last check of the loop, as
			// this frees the underlying data used above (eg, by the array free process)
			if (ns_v.IsValAllocated() && !ns_v.IsValInterface() && !ns_v.IsValDOMString()) {
				NS_ABORT_IF_FALSE(ns_v.IsPtrData(), "expecting a pointer to free");
				nsMemory::Free(ns_v.val.p);
			}
		}
		if (m_buffer_array && m_buffer_array[i])
			nsMemory::Free(m_buffer_array[i]);
	}
	delete [] m_python_type_desc_array;
	delete [] m_buffer_array;
	delete [] m_var_array;
}

PRBool PyXPCOM_InterfaceVariantHelper::Init(PyObject *obParams)
{
	PRBool ok = PR_FALSE;
	int i;
	int total_params_needed = 0;
	if (!PySequence_Check(obParams) || PySequence_Length(obParams)!=2) {
		PyErr_Format(PyExc_TypeError, "Param descriptors must be a sequence of exactly length 2");
		return PR_FALSE;
	}
	PyObject *typedescs = PySequence_GetItem(obParams, 0);
	if (typedescs==NULL)
		return PR_FALSE;
	// NOTE: The length of the typedescs may be different than the
	// args actually passed.  The typedescs always include all
	// hidden params (such as "size_is"), while the actual 
	// args never include this.
	m_num_array = PySequence_Length(typedescs);
	if (PyErr_Occurred()) goto done;

	m_pyparams = PySequence_GetItem(obParams, 1);
	if (m_pyparams==NULL) goto done;

	m_python_type_desc_array = new PythonTypeDescriptor[m_num_array];
	if (!m_python_type_desc_array) goto done;

	// Pull apart the type descs and stash them.
	for (i=0;i<m_num_array;i++) {
		PyObject *desc_object = PySequence_GetItem(typedescs, i);
		if (desc_object==NULL)
			goto done;

		// Pull apart the typedesc tuple back into a structure we can work with.
		PythonTypeDescriptor &ptd = m_python_type_desc_array[i];
		PRBool this_ok = PyArg_ParseTuple(desc_object, "bbbbO:type_desc", 
					&ptd.param_flags, &ptd.type_flags, &ptd.argnum, &ptd.argnum2, &ptd.extra);
		Py_DECREF(desc_object);
		if (!this_ok) goto done;
		Py_INCREF(ptd.extra);

	}
	total_params_needed = ProcessPythonTypeDescriptors(m_python_type_desc_array, m_num_array);
	// OK - check we got the number of args we expected.
	// If not, its really an internal error rather than the user.
	if (PySequence_Length(m_pyparams) != total_params_needed) {
		PyErr_Format(PyExc_ValueError, "The type descriptions indicate %d args are needed, but %d were provided",
			total_params_needed, PySequence_Length(m_pyparams));
		goto done;
	}

	// Init the other arrays.
	m_var_array = new nsXPTCVariant[m_num_array];
	if (!m_var_array) goto done;
	memset(m_var_array, 0, m_num_array * sizeof(m_var_array[0]));

	m_buffer_array = new void *[m_num_array];
	if (!m_buffer_array) goto done;
	memset(m_buffer_array, 0, m_num_array * sizeof(m_buffer_array[0]));

	ok = PR_TRUE;
done:
	if (!ok && !PyErr_Occurred())
		PyErr_NoMemory();

	Py_XDECREF(typedescs);
	return ok;
}


PRBool PyXPCOM_InterfaceVariantHelper::FillArray()
{
	int param_index = 0;
	int i;
	for (i=0;i<m_num_array;i++) {
		PythonTypeDescriptor &ptd = m_python_type_desc_array[i];
		// stash the type_flags into the variant, and remember how many extra bits of info we have.
		m_var_array[i].type = ptd.type_flags;
		if (XPT_PD_IS_IN(ptd.param_flags) && !ptd.is_auto_in && !XPT_PD_IS_DIPPER(ptd.param_flags)) {
			if (!FillInVariant(ptd, i, param_index))
				return PR_FALSE;
			param_index++;
		}
		if ((XPT_PD_IS_OUT(ptd.param_flags) && !ptd.is_auto_out) || XPT_PD_IS_DIPPER(ptd.param_flags)) {
			if (!PrepareOutVariant(ptd, i))
				return PR_FALSE;
		}
	}
	// There may be out "size_is" params we havent touched yet
	// (ie, as the param itself is marked "out", we never got to
	// touch the associated "size_is".
	// Final loop to handle this.
	for (i=0;i<m_num_array;i++) {
		PythonTypeDescriptor &ptd = m_python_type_desc_array[i];
		if (ptd.is_auto_out && !ptd.have_set_auto) {
			// Call PrepareOutVariant to ensure buffers etc setup.
			if (!PrepareOutVariant(ptd, i))
				return PR_FALSE;
		}
	}
	return PR_TRUE;
}


PRBool PyXPCOM_InterfaceVariantHelper::SetSizeIs( int var_index, PRBool is_arg1, PRUint32 new_size)
{
	NS_ABORT_IF_FALSE(var_index < m_num_array, "var_index param is invalid");
	PRUint8 argnum = is_arg1 ? 
		m_python_type_desc_array[var_index].argnum :
		m_python_type_desc_array[var_index].argnum2;
	NS_ABORT_IF_FALSE(argnum < m_num_array, "size_is param is invalid");
	PythonTypeDescriptor &td_size = m_python_type_desc_array[argnum];
	NS_ABORT_IF_FALSE(td_size.is_auto_in || td_size.is_auto_out, "Setting size_is, but param is not marked as auto!");
	NS_ABORT_IF_FALSE( (td_size.type_flags & XPT_TDP_TAGMASK) == nsXPTType::T_U32, "size param must be Uint32");
	nsXPTCVariant &ns_v = m_var_array[argnum];

	if (!td_size.have_set_auto) {
		ns_v.type = td_size.type_flags;
		ns_v.val.u32 = new_size;
		// In case it is "out", setup the necessary pointers.
		PrepareOutVariant(td_size, argnum);
		td_size.have_set_auto = PR_TRUE;
	} else {
		if (ns_v.val.u32 != new_size) {
			PyErr_Format(PyExc_ValueError, "Array lengths inconsistent; array size previously set to %d, but second array is of size %d", ns_v.val.u32, new_size);
			return PR_FALSE;
		}
	}
	return PR_TRUE;
}

PRUint32 PyXPCOM_InterfaceVariantHelper::GetSizeIs( int var_index, PRBool is_arg1)
{
	NS_ABORT_IF_FALSE(var_index < m_num_array, "var_index param is invalid");
	PRUint8 argnum = is_arg1 ? 
		m_python_type_desc_array[var_index].argnum :
		m_python_type_desc_array[var_index].argnum2;
	NS_ABORT_IF_FALSE(argnum < m_num_array, "size_is param is invalid");
	NS_ABORT_IF_FALSE( (m_python_type_desc_array[argnum].type_flags & XPT_TDP_TAGMASK) == nsXPTType::T_U32, "size param must be Uint32");
	PRBool is_out = XPT_PD_IS_OUT(m_python_type_desc_array[argnum].param_flags);
	nsXPTCVariant &ns_v = m_var_array[argnum];
	return is_out ? *((PRUint32 *)ns_v.ptr) : ns_v.val.u32;
}

#define MAKE_VALUE_BUFFER(size) \
	if ((this_buffer_pointer = (void *)nsMemory::Alloc((size))) == nsnull) { \
		PyErr_NoMemory(); \
		BREAK_FALSE; \
	}

PRBool PyXPCOM_InterfaceVariantHelper::FillInVariant(const PythonTypeDescriptor &td, int value_index, int param_index)
{
	PRBool rc = PR_TRUE;
	// Get a reference to the variant we are filling for convenience.
	nsXPTCVariant &ns_v = m_var_array[value_index];
	NS_ABORT_IF_FALSE(ns_v.type == td.type_flags, "Expecting variant all setup for us");

	// We used to avoid passing internal buffers to PyString etc objects
	// for 2 reasons: paranoia (so incorrect external components couldn't break
	// Python) and simplicity (in vs in-out issues, etc)
	// However, at least one C++ implemented component (nsITimelineService)
	// uses a "char *", and keys on the address (assuming that the same
	// *pointer* is passed rather than value.  Therefore, we have a special case
	// - T_CHAR_STR that is "in" gets the Python string pointer passed.
	void *&this_buffer_pointer = m_buffer_array[value_index]; // Freed at object destruction with PyMem_Free()
	NS_ABORT_IF_FALSE(this_buffer_pointer==nsnull, "We appear to already have a buffer");
	int cb_this_buffer_pointer = 0;
	if (XPT_PD_IS_IN(td.param_flags)) {
		NS_ABORT_IF_FALSE(!td.is_auto_in, "Param is 'auto-in', but we are filling it normally!");
		PyObject *val_use = NULL; // a temp object converters can use, and will be DECREF'd
		PyObject *val = PySequence_GetItem(m_pyparams, param_index);
		NS_WARN_IF_FALSE(val, "Have an 'in' param, but no Python value!");
		if (val==NULL) {
			PyErr_Format(PyExc_ValueError, "Param %d is marked as 'in', but no value was given", value_index);
			return PR_FALSE;
		}
		switch (XPT_TDP_TAG(ns_v.type)) {
		  case nsXPTType::T_I8:
			if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE
			ns_v.val.i8 = (PRInt8)PyInt_AsLong(val_use);
			break;
		  case nsXPTType::T_I16:
			if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE
			ns_v.val.i16 = (PRInt16)PyInt_AsLong(val_use);
			break;
		  case nsXPTType::T_I32:
			if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE
			ns_v.val.i32 = (PRInt32)PyInt_AsLong(val_use);
			break;
		  case nsXPTType::T_I64:
			if ((val_use=PyNumber_Long(val))==NULL) BREAK_FALSE
			ns_v.val.i64 = (PRInt64)PyLong_AsLongLong(val_use);
			break;
		  case nsXPTType::T_U8:
			if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE
			ns_v.val.u8 = (PRUint8)PyInt_AsLong(val_use);
			break;
		  case nsXPTType::T_U16:
			if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE
			ns_v.val.u16 = (PRUint16)PyInt_AsLong(val_use);
			break;
		  case nsXPTType::T_U32:
			if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE
			ns_v.val.u32 = (PRUint32)PyInt_AsLong(val_use);
			break;
		  case nsXPTType::T_U64:
			if ((val_use=PyNumber_Long(val))==NULL) BREAK_FALSE
			ns_v.val.u64 = (PRUint64)PyLong_AsUnsignedLongLong(val_use);
			break;
		  case nsXPTType::T_FLOAT:
			if ((val_use=PyNumber_Float(val))==NULL) BREAK_FALSE
			ns_v.val.f = (float)PyFloat_AsDouble(val_use);
			break;
		  case nsXPTType::T_DOUBLE:
			if ((val_use=PyNumber_Float(val))==NULL) BREAK_FALSE
			ns_v.val.d = PyFloat_AsDouble(val_use);
			break;
		  case nsXPTType::T_BOOL:
			if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE
			ns_v.val.b = (PRBool)PyInt_AsLong(val_use);
			break;
		  case nsXPTType::T_CHAR:{
			if (!PyString_Check(val) && !PyUnicode_Check(val)) {
				PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
				BREAK_FALSE;
			}
			if ((val_use = PyObject_Str(val))==NULL)
				BREAK_FALSE;
			// Sanity check should PyObject_Str() ever loosen its semantics wrt Unicode!
			NS_ABORT_IF_FALSE(PyString_Check(val_use), "PyObject_Str didnt return a string object!");
			if (PyString_GET_SIZE(val_use) != 1) {
				PyErr_SetString(PyExc_ValueError, "Must specify a one character string for a character");
				BREAK_FALSE;
			}

			ns_v.val.c = *PyString_AS_STRING(val_use);
			break;
			}

		  case nsXPTType::T_WCHAR: {
			if (!PyString_Check(val) && !PyUnicode_Check(val)) {
				PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
				BREAK_FALSE;
			}
			if ((val_use = PyUnicodeUCS2_FromObject(val))==NULL)
				BREAK_FALSE;
			// Sanity check should PyObject_Str() ever loosen its semantics wrt Unicode!
			NS_ABORT_IF_FALSE(PyUnicode_Check(val_use), "PyUnicodeUCS2_FromObject didnt return a unicode object!");
			if (PyUnicode_GET_SIZE(val_use) != 1) {
				PyErr_SetString(PyExc_ValueError, "Must specify a one character string for a character");
				BREAK_FALSE;
			}
			ns_v.val.wc = *PyUnicode_AS_UNICODE(val_use);
			break;
			}
	//          case nsXPTType::T_VOID:              /* fall through */
		  case nsXPTType::T_IID:
			nsIID iid;
			MAKE_VALUE_BUFFER(sizeof(nsIID));
			if (!Py_nsIID::IIDFromPyObject(val, &iid))
				BREAK_FALSE;
			memcpy(this_buffer_pointer, &iid, sizeof(iid));
			ns_v.val.p = this_buffer_pointer;
			break;
		  case nsXPTType::T_ASTRING:
		  case nsXPTType::T_DOMSTRING: {
			if (val==Py_None) {
				ns_v.val.p = new nsString();
			} else {
				if (!PyString_Check(val) && !PyUnicode_Check(val)) {
					PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
					BREAK_FALSE;
				}
				if ((val_use = PyUnicodeUCS2_FromObject(val))==NULL)
					BREAK_FALSE;
				// Sanity check should PyObject_Str() ever loosen its semantics wrt Unicode!
				NS_ABORT_IF_FALSE(PyUnicode_Check(val_use), "PyUnicodeUCS2_FromObject didnt return a unicode object!");
				ns_v.val.p = new nsString(PyUnicode_AS_UNICODE(val_use), 
				                          PyUnicode_GET_SIZE(val_use));
			}
			if (!ns_v.val.p) {
				PyErr_NoMemory();
				BREAK_FALSE;
			}
			// We created it - flag as such for cleanup.
			ns_v.flags |= nsXPTCVariant::VAL_IS_DOMSTR;
			break;
		  }
		  case nsXPTType::T_CSTRING:
		  case nsXPTType::T_UTF8STRING: {
			PRBool bIsUTF8 = XPT_TDP_TAG(ns_v.type) == nsXPTType::T_UTF8STRING;
			if (val==Py_None) {
				ns_v.val.p = new nsCString();
			} else {
				// strings are assumed to already be UTF8 encoded.
				if (PyString_Check(val)) {
					val_use = val;
					Py_INCREF(val);
				// Unicode objects are encoded by us.
				} else if (PyUnicode_Check(val)) {
					if (bIsUTF8)
						val_use = PyUnicode_AsUTF8String(val);
					else
						val_use = PyObject_Str(val);
				} else {
					PyErr_SetString(PyExc_TypeError, "UTF8 parameters must be string or Unicode objects");
					BREAK_FALSE;
				}
				if (!val_use)
					BREAK_FALSE;
				ns_v.val.p = new nsCString(PyString_AS_STRING(val_use), 
				                           PyString_GET_SIZE(val_use));
			}

			if (!ns_v.val.p) {
				PyErr_NoMemory();
				BREAK_FALSE;
			}
			// We created it - flag as such for cleanup.
			ns_v.flags |= bIsUTF8 ? nsXPTCVariant::VAL_IS_UTF8STR : nsXPTCVariant::VAL_IS_CSTR;
			break;
			}
		  case nsXPTType::T_CHAR_STR: {
			if (val==Py_None) {
				ns_v.val.p = nsnull;
				break;
			}
			// If an "in" char *, and we have a PyString, then pass the
			// pointer (hoping everyone else plays by the rules too.
			if (!XPT_PD_IS_OUT(td.param_flags) && PyString_Check(val)) {
				ns_v.val.p = PyString_AS_STRING(val);
				break;
			}
			   
			if (!PyString_Check(val) && !PyUnicode_Check(val)) {
				PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
				BREAK_FALSE;
			}
			if ((val_use = PyObject_Str(val))==NULL)
				BREAK_FALSE;
			// Sanity check should PyObject_Str() ever loosen its semantics wrt Unicode!
			NS_ABORT_IF_FALSE(PyString_Check(val_use), "PyObject_Str didnt return a string object!");

			cb_this_buffer_pointer = PyString_GET_SIZE(val_use)+1;
			MAKE_VALUE_BUFFER(cb_this_buffer_pointer);
			memcpy(this_buffer_pointer, PyString_AS_STRING(val_use), cb_this_buffer_pointer);
			ns_v.val.p = this_buffer_pointer;
			break;
			}

		  case nsXPTType::T_WCHAR_STR: {
			if (val==Py_None) {
				ns_v.val.p = nsnull;
				break;
			}
			// If an "in" char *, and we have a PyString, then pass the
			// pointer (hoping everyone else plays by the rules too.
			if (!XPT_PD_IS_OUT(td.param_flags) && PyUnicode_Check(val)) {
				ns_v.val.p = PyUnicode_AS_UNICODE(val);
				break;
			}
			if (!PyString_Check(val) && !PyUnicode_Check(val)) {
				PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
				BREAK_FALSE;
			}
			if ((val_use = PyUnicodeUCS2_FromObject(val))==NULL)
				BREAK_FALSE;
			NS_ABORT_IF_FALSE(PyUnicode_Check(val_use), "PyUnicodeUCS2_FromObject didnt return a Unicode object!");
			cb_this_buffer_pointer = (PyUnicode_GET_SIZE(val_use)+1) * sizeof(Py_UNICODE);
			MAKE_VALUE_BUFFER(cb_this_buffer_pointer);
			memcpy(this_buffer_pointer, PyUnicode_AS_UNICODE(val_use), cb_this_buffer_pointer);
			ns_v.val.p = this_buffer_pointer;
			break;
			}
		  case nsXPTType::T_INTERFACE:  {
			nsIID iid;
			if (!Py_nsIID::IIDFromPyObject(td.extra, &iid))
				BREAK_FALSE;
			if (!Py_nsISupports::InterfaceFromPyObject(
			                       val, 
			                       iid, 
			                       (nsISupports **)&ns_v.val.p, 
			                       PR_TRUE))
				BREAK_FALSE;
			// We have added a reference - flag as such for cleanup.
			ns_v.flags |= nsXPTCVariant::VAL_IS_IFACE;
			break;
			}
		  case nsXPTType::T_INTERFACE_IS: {
			nsIID iid;
			nsXPTCVariant &ns_viid = m_var_array[td.argnum];
			NS_WARN_IF_FALSE(XPT_TDP_TAG(ns_viid.type)==nsXPTType::T_IID, "The INTERFACE_IS iid describer isnt an IID!");
			// This is a pretty serious problem, but not Python's fault!
			// Just return an nsISupports and hope the caller does whatever
			// QI they need before using it.
			if (XPT_TDP_TAG(ns_viid.type)==nsXPTType::T_IID &&
			    XPT_PD_IS_IN(ns_viid.type)) {
				nsIID *piid = (nsIID *)ns_viid.val.p;
				if (piid==NULL)
					// Also serious, but like below, not our fault!
					iid = NS_GET_IID(nsISupports);
				else
					iid = *piid;
			} else
				// Use NULL IID to avoid a QI in this case.
				iid = Py_nsIID_NULL;
			if (!Py_nsISupports::InterfaceFromPyObject(
				               val, 
				               iid, 
			                       (nsISupports **)&ns_v.val.p, 
			                       PR_TRUE))
				BREAK_FALSE;
			// We have added a reference - flag as such for cleanup.
			ns_v.flags |= nsXPTCVariant::VAL_IS_IFACE;
			break;
			}
		  case nsXPTType::T_PSTRING_SIZE_IS: {
			if (val==Py_None) {
				ns_v.val.p = nsnull;
				break;
			}
			if (!PyString_Check(val) && !PyUnicode_Check(val)) {
				PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
				BREAK_FALSE;
			}
			if ((val_use = PyObject_Str(val))==NULL)
				BREAK_FALSE;
			// Sanity check should PyObject_Str() ever loosen its semantics wrt Unicode!
			NS_ABORT_IF_FALSE(PyString_Check(val_use), "PyObject_Str didnt return a string object!");

			cb_this_buffer_pointer = PyString_GET_SIZE(val_use);
			MAKE_VALUE_BUFFER(cb_this_buffer_pointer);
			memcpy(this_buffer_pointer, PyString_AS_STRING(val_use), cb_this_buffer_pointer);
			ns_v.val.p = this_buffer_pointer;
			rc = SetSizeIs(value_index, PR_TRUE, cb_this_buffer_pointer);
			break;
			}

		case nsXPTType::T_PWSTRING_SIZE_IS: {
			if (val==Py_None) {
				ns_v.val.p = nsnull;
				break;
			}
			if (!PyString_Check(val) && !PyUnicode_Check(val)) {
				PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
				BREAK_FALSE;
			}
			if ((val_use = PyUnicodeUCS2_FromObject(val))==NULL)
				BREAK_FALSE;
			// Sanity check should PyObject_Str() ever loosen its semantics wrt Unicode!
			NS_ABORT_IF_FALSE(PyUnicode_Check(val_use), "PyObject_Unicode didnt return a unicode object!");

			cb_this_buffer_pointer = PyUnicode_GET_SIZE(val_use) * sizeof(PRUnichar);
			MAKE_VALUE_BUFFER(cb_this_buffer_pointer);
			memcpy(this_buffer_pointer, PyUnicode_AS_UNICODE(val_use), cb_this_buffer_pointer);
			ns_v.val.p = this_buffer_pointer;
			rc = SetSizeIs(value_index, PR_TRUE, PyUnicode_GET_SIZE(val_use) );
			break;
			}
		case nsXPTType::T_ARRAY: {
			if (val==Py_None) {
				ns_v.val.p = nsnull;
				break;
			}
			if (!PyInt_Check(td.extra)) {
				PyErr_SetString(PyExc_TypeError, "The array info is not valid");
				BREAK_FALSE;
			}
			if (!PySequence_Check(val)) {
				PyErr_SetString(PyExc_TypeError, "This parameter must be a sequence");
				BREAK_FALSE;
			}
			int array_type = PyInt_AsLong(td.extra);
			PRUint32 element_size = GetArrayElementSize(array_type);
			int seq_length = PySequence_Length(val);
			cb_this_buffer_pointer = seq_length * element_size;
			if (cb_this_buffer_pointer==0) 
				// prevent assertions allocing zero bytes.  Can't use NULL.
				cb_this_buffer_pointer = 1; 
			MAKE_VALUE_BUFFER(cb_this_buffer_pointer);
			memset(this_buffer_pointer, 0, cb_this_buffer_pointer);
			rc = FillSingleArray(this_buffer_pointer, val, seq_length, element_size, array_type&XPT_TDP_TAGMASK);
			if (!rc) break;
			rc = SetSizeIs(value_index, PR_FALSE, seq_length);
			if (!rc) break;
			ns_v.flags |= nsXPTCVariant::VAL_IS_ARRAY;
			ns_v.val.p = this_buffer_pointer;
			break;
			}
		default:
			PyErr_Format(PyExc_TypeError, "The object type (0x%x) is unknown", XPT_TDP_TAG(ns_v.type));
			rc = PR_FALSE;
			break;
		}
		Py_DECREF(val); // Cant be NULL!
		Py_XDECREF(val_use);
	}
	return rc && !PyErr_Occurred();
}

PRBool PyXPCOM_InterfaceVariantHelper::PrepareOutVariant(const PythonTypeDescriptor &td, int value_index)
{
	PRBool rc = PR_TRUE;
	nsXPTCVariant &ns_v = m_var_array[value_index];
	void *&this_buffer_pointer = m_buffer_array[value_index]; // Freed at object destruction with PyMem_Free()
	// Do the out param thang...
	if (XPT_PD_IS_OUT(td.param_flags) || XPT_PD_IS_DIPPER(td.param_flags)) {
		NS_ABORT_IF_FALSE(ns_v.ptr == NULL, "already have a pointer!");
		ns_v.ptr = &ns_v;
		ns_v.flags |= nsXPTCVariant::PTR_IS_DATA;

		// Special flags based on the data type
		switch (XPT_TDP_TAG(ns_v.type)) {
		  case nsXPTType::T_I8:
		  case nsXPTType::T_I16:
		  case nsXPTType::T_I32:
		  case nsXPTType::T_I64:
		  case nsXPTType::T_U8:
		  case nsXPTType::T_U16:
		  case nsXPTType::T_U32:
		  case nsXPTType::T_U64:
		  case nsXPTType::T_FLOAT:
		  case nsXPTType::T_DOUBLE:
		  case nsXPTType::T_BOOL:
		  case nsXPTType::T_CHAR:
		  case nsXPTType::T_WCHAR:
		  case nsXPTType::T_VOID:
			break;

		  case nsXPTType::T_INTERFACE:
		  case nsXPTType::T_INTERFACE_IS:
			NS_ABORT_IF_FALSE(this_buffer_pointer==NULL, "Can't have an interface and a buffer pointer!");
			ns_v.flags |= nsXPTCVariant::VAL_IS_IFACE;
			ns_v.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
			break;
		  case nsXPTType::T_ARRAY:
			ns_v.flags |= nsXPTCVariant::VAL_IS_ARRAY;
			ns_v.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
			// Even if ns_val.p already setup as part of "in" processing,
			// we need to ensure setup for out.
			NS_ABORT_IF_FALSE(ns_v.val.p==nsnull || ns_v.val.p==this_buffer_pointer, "Garbage in our pointer?");
			ns_v.val.p = this_buffer_pointer;
			this_buffer_pointer = nsnull;
			break;
		  case nsXPTType::T_PWSTRING_SIZE_IS:
		  case nsXPTType::T_PSTRING_SIZE_IS:
		  case nsXPTType::T_WCHAR_STR:
		  case nsXPTType::T_CHAR_STR:
		  case nsXPTType::T_IID:
			// If we stashed a value in the this_buffer_pointer, and
			// we are passing it as an OUT param, we do _not_ want to
			// treat it as a temporary buffer.
			// For example, if we pass an IID or string as an IN param,
			// we allocate a buffer for the value, but this is NOT cleaned up
			// via normal VARIANT cleanup rules - hence we clean it up ourselves.
			// If the param is IN/OUT, then the buffer falls under the normal variant
			// rules (ie, is flagged as VAL_IS_ALLOCD), so we dont clean it as a temporary.
			// (it may have been changed under us - we free the _new_ value.
			// Even if ns_val.p already setup as part of "in" processing,
			// we need to ensure setup for out.
			NS_ABORT_IF_FALSE(ns_v.val.p==nsnull || ns_v.val.p==this_buffer_pointer, "Garbage in our pointer?");
			ns_v.val.p = this_buffer_pointer;
			ns_v.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
			this_buffer_pointer = nsnull;
			break;
		  case nsXPTType::T_DOMSTRING:
		  case nsXPTType::T_ASTRING: {
			  NS_ABORT_IF_FALSE(ns_v.val.p==nsnull, "T_DOMTSTRINGS cant be out and have a value (ie, no in/outs are allowed!");
			  NS_ABORT_IF_FALSE(XPT_PD_IS_DIPPER(td.param_flags) && XPT_PD_IS_IN(td.param_flags), "out DOMStrings must really be in dippers!");
			  ns_v.flags |= nsXPTCVariant::VAL_IS_DOMSTR;
			  // Dippers are really treated like "in" params.
			  ns_v.ptr = new nsString();
			  ns_v.val.p = ns_v.ptr; // VAL_IS_* says the .p is what gets freed
			  if (!ns_v.ptr) {
				  PyErr_NoMemory();
				  rc = PR_FALSE;
			  }
			  break;
			}
		  case nsXPTType::T_CSTRING:
		  case nsXPTType::T_UTF8STRING: {
			  NS_ABORT_IF_FALSE(ns_v.val.p==nsnull, "T_DOMTSTRINGS cant be out and have a value (ie, no in/outs are allowed!");
			  NS_ABORT_IF_FALSE(XPT_PD_IS_DIPPER(td.param_flags) && XPT_PD_IS_IN(td.param_flags), "out DOMStrings must really be in dippers!");
			  ns_v.flags |= ( XPT_TDP_TAG(ns_v.type)==nsXPTType::T_CSTRING ? nsXPTCVariant::VAL_IS_CSTR : nsXPTCVariant::VAL_IS_UTF8STR);
			  ns_v.ptr = new nsCString();
			  ns_v.val.p = ns_v.ptr; // VAL_IS_* says the .p is what gets freed
			  if (!ns_v.ptr) {
				  PyErr_NoMemory();
				  rc = PR_FALSE;
			  }
			  break;
			}
		  default:
			NS_ABORT_IF_FALSE(0, "Unknown type - don't know how to prepare the output value");
			break; // Nothing to do!
		}
	}
	return rc;
}

PyObject *PyXPCOM_InterfaceVariantHelper::MakeSinglePythonResult(int index)
{
	nsXPTCVariant &ns_v = m_var_array[index];
	PyObject *ret = nsnull;
	NS_ABORT_IF_FALSE(ns_v.IsPtrData() || ns_v.IsValDOMString(), "expecting a pointer if you want a result!");

	// Re-fetch the type descriptor.
	PythonTypeDescriptor &td = m_python_type_desc_array[index];
	// Make sure the type tag of the variant hasnt changed on us.
	NS_ABORT_IF_FALSE(ns_v.type==td.type_flags, "variant type has changed under us!");

	// If the pointer is NULL, we can get out now!
	if (ns_v.ptr==nsnull) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	switch (XPT_TDP_TAG(ns_v.type)) {
	  case nsXPTType::T_I8:
		ret = PyInt_FromLong( *((PRInt8 *)ns_v.ptr) );
		break;
	  case nsXPTType::T_I16:
		ret = PyInt_FromLong( *((PRInt16 *)ns_v.ptr) );
		break;
	  case nsXPTType::T_I32:
		ret = PyInt_FromLong( *((PRInt32 *)ns_v.ptr) );
		break;
	  case nsXPTType::T_I64:
		ret = PyLong_FromLongLong( *((PRInt64 *)ns_v.ptr) );
		break;
	  case nsXPTType::T_U8:
		ret = PyInt_FromLong( *((PRUint8 *)ns_v.ptr) );
		break;
	  case nsXPTType::T_U16:
		ret = PyInt_FromLong( *((PRUint16 *)ns_v.ptr) );
		break;
	  case nsXPTType::T_U32:
		ret = PyInt_FromLong( *((PRUint32 *)ns_v.ptr) );
		break;
	  case nsXPTType::T_U64:
		ret = PyLong_FromUnsignedLongLong( *((PRUint64 *)ns_v.ptr) );
		break;
	  case nsXPTType::T_FLOAT:
		ret = PyFloat_FromDouble( *((float *)ns_v.ptr) );
		break;
	  case nsXPTType::T_DOUBLE:
		ret = PyFloat_FromDouble( *((double *)ns_v.ptr) );
		break;
	  case nsXPTType::T_BOOL:
		ret = *((PRBool *)ns_v.ptr) ? Py_True : Py_False;
		Py_INCREF(ret);
		break;
	  case nsXPTType::T_CHAR:
		ret = PyString_FromStringAndSize( ((char *)ns_v.ptr), 1 );
		break;

	  case nsXPTType::T_WCHAR:
		ret = PyUnicodeUCS2_FromUnicode( ((PRUnichar *)ns_v.ptr), 1 );
		break;
//	  case nsXPTType::T_VOID:
	  case nsXPTType::T_IID: 
		ret = Py_nsIID::PyObjectFromIID( **((nsIID **)ns_v.ptr) );
		break;
	  case nsXPTType::T_ASTRING:
	  case nsXPTType::T_DOMSTRING: {
		nsAString *rs = (nsAString *)ns_v.ptr;
		ret = PyObject_FromNSString(*rs);
		break;
		}
	  case nsXPTType::T_UTF8STRING:
	  case nsXPTType::T_CSTRING: {
		nsCString *rs = (nsCString *)ns_v.ptr;
		ret = PyObject_FromNSString(*rs, XPT_TDP_TAG(ns_v.type)==nsXPTType::T_UTF8STRING);
		break;
		}

	  case nsXPTType::T_CHAR_STR:
		if (*((char **)ns_v.ptr) == NULL) {
			ret = Py_None;
			Py_INCREF(Py_None);
		} else
			ret = PyString_FromString( *((char **)ns_v.ptr) );
		break;

	  case nsXPTType::T_WCHAR_STR: {
		PRUnichar *us = *((PRUnichar **)ns_v.ptr);
		if (us == NULL) {
			ret = Py_None;
			Py_INCREF(Py_None);
		} else
			ret = PyUnicodeUCS2_FromUnicode( us, nsCRT::strlen(us));
		break;
		}
	  case nsXPTType::T_INTERFACE: {
		nsIID iid;
		if (!Py_nsIID::IIDFromPyObject(td.extra, &iid))
			break;
		nsISupports *iret = *((nsISupports **)ns_v.ptr);
		// We _do_ add a reference here, as our cleanup code will
		// remove this reference should we own it.
		ret = Py_nsISupports::PyObjectFromInterfaceOrVariant(iret, iid, PR_TRUE);
		break;
		}
	  case nsXPTType::T_INTERFACE_IS: {
		nsIID iid;
		nsXPTCVariant &ns_viid = m_var_array[td.argnum];
		NS_WARN_IF_FALSE(XPT_TDP_TAG(ns_viid.type)==nsXPTType::T_IID, "The INTERFACE_IS iid describer isnt an IID!");
		if (XPT_TDP_TAG(ns_viid.type)==nsXPTType::T_IID) {
			nsIID *piid = (nsIID *)ns_viid.val.p;
			if (piid==NULL)
				// Also serious, but like below, not our fault!
				iid = NS_GET_IID(nsISupports);
			else
				iid = *piid;
		} else
			// This is a pretty serious problem, but not Python's fault!
			// Just return an nsISupports and hope the caller does whatever
			// QI they need before using it.
			iid = NS_GET_IID(nsISupports);
		nsISupports *iret = *((nsISupports **)ns_v.ptr);
		// We _do_ add a reference here, as our cleanup code will
		// remove this reference should we own it.
		ret = Py_nsISupports::PyObjectFromInterfaceOrVariant(iret, iid, PR_TRUE);
		break;
		}
	  case nsXPTType::T_ARRAY: {
		if ( (* ((void **)ns_v.ptr)) == NULL) {
			ret = Py_None;
			Py_INCREF(Py_None);
		}
		if (!PyInt_Check(td.extra)) {
			PyErr_SetString(PyExc_TypeError, "The array info is not valid");
			break;
		}
		PRUint8 array_type = (PRUint8)PyInt_AsLong(td.extra);
		PRUint32 seq_size = GetSizeIs(index, PR_FALSE);
		ret = UnpackSingleArray(* ((void **)ns_v.ptr), seq_size, array_type&XPT_TDP_TAGMASK, NULL);
		break;
		}

	  case nsXPTType::T_PSTRING_SIZE_IS:
		if (*((char **)ns_v.ptr) == NULL) {
			ret = Py_None;
			Py_INCREF(Py_None);
		} else {
			PRUint32 string_size = GetSizeIs(index, PR_TRUE);
			ret = PyString_FromStringAndSize( *((char **)ns_v.ptr), string_size );
		}
		break;

	  case nsXPTType::T_PWSTRING_SIZE_IS:
		if (*((PRUnichar **)ns_v.ptr) == NULL) {
			ret = Py_None;
			Py_INCREF(Py_None);
		} else {
			PRUint32 string_size = GetSizeIs(index, PR_TRUE);
			ret = PyUnicodeUCS2_FromUnicode( *((PRUnichar **)ns_v.ptr), string_size );
		}
		break;
	default:
		PyErr_Format(PyExc_ValueError, "Unknown XPCOM type code (0x%x)", XPT_TDP_TAG(ns_v.type));
		/* ret remains nsnull */
		break;
	}
	return ret;
}


PyObject *PyXPCOM_InterfaceVariantHelper::MakePythonResult()
{
	// First we count the results.
	int i = 0;
	int n_results = 0;
	PyObject *ret = NULL;
	PRBool have_retval = PR_FALSE;
	for (i=0;i<m_num_array;i++) {
		PythonTypeDescriptor &td = m_python_type_desc_array[i];
		if (!td.is_auto_out) {
			if (XPT_PD_IS_OUT(td.param_flags) || XPT_PD_IS_DIPPER(td.param_flags))
				n_results++;
			if (XPT_PD_IS_RETVAL(td.param_flags))
				have_retval = PR_TRUE;
		}
	}
	if (n_results==0) {
		ret = Py_None;
		Py_INCREF(ret);
	} else {
		if (n_results > 1) {
			ret = PyTuple_New(n_results);
			if (ret==NULL)
				return NULL;
		}
		int ret_index = 0;
		int max_index = m_num_array;
		// Stick the retval at the front if we have have
		if (have_retval && n_results > 1) {
			PyObject *val = MakeSinglePythonResult(m_num_array-1);
			if (val==NULL) {
				Py_DECREF(ret);
				return NULL;
			}
			PyTuple_SET_ITEM(ret, 0, val);
			max_index--;
			ret_index++;

		}
		for (i=0;ret_index < n_results && i < max_index;i++) {
			if (!m_python_type_desc_array[i].is_auto_out) {
				if (XPT_PD_IS_OUT(m_python_type_desc_array[i].param_flags) || XPT_PD_IS_DIPPER(m_python_type_desc_array[i].param_flags)) {
					PyObject *val = MakeSinglePythonResult(i);
					if (val==NULL) {
						Py_XDECREF(ret);
						return NULL;
					}
					if (n_results > 1) {
						PyTuple_SET_ITEM(ret, ret_index, val);
						ret_index++;
					} else {
						NS_ABORT_IF_FALSE(ret==NULL, "shouldnt already have a ret!");
						ret = val;
					}
				}
			}
		}

	}
	return ret;
}

/*************************************************************************
**************************************************************************

 Helpers when IMPLEMENTING interfaces.

**************************************************************************
*************************************************************************/

PyXPCOM_GatewayVariantHelper::PyXPCOM_GatewayVariantHelper( PyG_Base *gw, int method_index, const nsXPTMethodInfo *info, nsXPTCMiniVariant* params )
{
	m_params = params;
	m_info = info;
	// no references added - this class is only alive for
	// a single gateway invocation
	m_gateway = gw; 
	m_method_index = method_index;
	m_python_type_desc_array = NULL;
	m_num_type_descs = 0;
}

PyXPCOM_GatewayVariantHelper::~PyXPCOM_GatewayVariantHelper()
{
	delete [] m_python_type_desc_array;
}

PyObject *PyXPCOM_GatewayVariantHelper::MakePyArgs()
{
	NS_PRECONDITION(sizeof(XPTParamDescriptor) == sizeof(nsXPTParamInfo), "We depend on nsXPTParamInfo being a wrapper over the XPTParamDescriptor struct");
	// Setup our array of Python typedescs, and determine the number of objects we
	// pass to Python.
	m_num_type_descs = m_info->num_args;
	m_python_type_desc_array = new PythonTypeDescriptor[m_num_type_descs];
	if (m_python_type_desc_array==nsnull)
		return PyErr_NoMemory();

	// First loop to count the number of objects
	// we pass to Python
	int i;
	for (i=0;i<m_info->num_args;i++) {
		nsXPTParamInfo *pi = (nsXPTParamInfo *)m_info->params+i;
		PythonTypeDescriptor &td = m_python_type_desc_array[i];
		td.param_flags = pi->flags;
		td.type_flags = pi->type.prefix.flags;
		td.argnum = pi->type.argnum;
		td.argnum2 = pi->type.argnum2;
	}
	int num_args = ProcessPythonTypeDescriptors(m_python_type_desc_array, m_num_type_descs);
	PyObject *ret = PyTuple_New(num_args);
	if (ret==NULL)
		return NULL;
	int this_arg = 0;
	for (i=0;i<m_num_type_descs;i++) {
		PythonTypeDescriptor &td = m_python_type_desc_array[i];
		if (XPT_PD_IS_IN(td.param_flags) && !td.is_auto_in && !XPT_PD_IS_DIPPER(td.param_flags)) {
			PyObject *sub = MakeSingleParam( i, td );
			if (sub==NULL) {
				Py_DECREF(ret);
				return NULL;
			}
			NS_ABORT_IF_FALSE(this_arg>=0 && this_arg<num_args, "We are going off the end of the array!");
			PyTuple_SET_ITEM(ret, this_arg, sub);
			this_arg++;
		}
	}
	return ret;
}

PRBool PyXPCOM_GatewayVariantHelper::CanSetSizeIs( int var_index, PRBool is_arg1 )
{
	NS_ABORT_IF_FALSE(var_index < m_num_type_descs, "var_index param is invalid");
	PRUint8 argnum = is_arg1 ? 
		m_python_type_desc_array[var_index].argnum :
		m_python_type_desc_array[var_index].argnum2;
	NS_ABORT_IF_FALSE(argnum < m_num_type_descs, "size_is param is invalid");
	return XPT_PD_IS_OUT(m_python_type_desc_array[argnum].param_flags);
}

PRBool PyXPCOM_GatewayVariantHelper::SetSizeIs( int var_index, PRBool is_arg1, PRUint32 new_size)
{
	NS_ABORT_IF_FALSE(var_index < m_num_type_descs, "var_index param is invalid");
	PRUint8 argnum = is_arg1 ? 
		m_python_type_desc_array[var_index].argnum :
		m_python_type_desc_array[var_index].argnum2;
	NS_ABORT_IF_FALSE(argnum < m_num_type_descs, "size_is param is invalid");
	PythonTypeDescriptor &td_size = m_python_type_desc_array[argnum];
	NS_ABORT_IF_FALSE( XPT_PD_IS_OUT(td_size.param_flags), "size param must be out if we want to set it!");
	NS_ABORT_IF_FALSE(td_size.is_auto_out, "Setting size_is, but param is not marked as auto!");

	nsXPTCMiniVariant &ns_v = m_params[argnum];
	NS_ABORT_IF_FALSE( (td_size.type_flags & XPT_TDP_TAGMASK) == nsXPTType::T_U32, "size param must be Uint32");
	NS_ABORT_IF_FALSE(ns_v.val.p, "NULL pointer for size_is value!");
	if (ns_v.val.p) {
		if (!td_size.have_set_auto) {
			*((PRUint32 *)ns_v.val.p) = new_size;
			td_size.have_set_auto = PR_TRUE;
		} else {
			if (*((PRUint32 *)ns_v.val.p) != new_size ) {
				PyErr_Format(PyExc_ValueError, "Array lengths inconsistent; array size previously set to %d, but second array is of size %d", ns_v.val.u32, new_size);
				return PR_FALSE;
			}
		}
	}
	return PR_TRUE;
}

PRUint32 PyXPCOM_GatewayVariantHelper::GetSizeIs( int var_index, PRBool is_arg1)
{
	NS_ABORT_IF_FALSE(var_index < m_num_type_descs, "var_index param is invalid");
	PRUint8 argnum = is_arg1 ? 
		m_python_type_desc_array[var_index].argnum :
		m_python_type_desc_array[var_index].argnum2;
	NS_ABORT_IF_FALSE(argnum < m_num_type_descs, "size_is param is invalid");
	if (argnum >= m_num_type_descs) {
		PyErr_SetString(PyExc_ValueError, "dont have a valid size_is indicator for this param");
		return PR_FALSE;
	}
	PRBool is_out = XPT_PD_IS_OUT(m_python_type_desc_array[argnum].param_flags);
	nsXPTCMiniVariant &ns_v = m_params[argnum];
	NS_ABORT_IF_FALSE( (m_python_type_desc_array[argnum].type_flags & XPT_TDP_TAGMASK) == nsXPTType::T_U32, "size param must be Uint32");
	return is_out ? *((PRUint32 *)ns_v.val.p) : ns_v.val.u32;
}

#undef DEREF_IN_OR_OUT
#define DEREF_IN_OR_OUT( element, ret_type ) (ret_type)(is_out ? *((ret_type *)ns_v.val.p) : (ret_type)(element))

PyObject *PyXPCOM_GatewayVariantHelper::MakeSingleParam(int index, PythonTypeDescriptor &td)
{
	NS_PRECONDITION(XPT_PD_IS_IN(td.param_flags), "Must be an [in] param!");
	nsXPTCMiniVariant &ns_v = m_params[index];
	PyObject *ret = NULL;
	PRBool is_out = XPT_PD_IS_OUT(td.param_flags);

	switch (td.type_flags & XPT_TDP_TAGMASK) {
	  case nsXPTType::T_I8:
		ret = PyInt_FromLong( DEREF_IN_OR_OUT(ns_v.val.i8, PRInt8 ) );
		break;
	  case nsXPTType::T_I16:
		ret = PyInt_FromLong( DEREF_IN_OR_OUT(ns_v.val.i16, PRInt16) );
		break;
	  case nsXPTType::T_I32:
		ret = PyInt_FromLong( DEREF_IN_OR_OUT(ns_v.val.i32, PRInt32) );
		break;
	  case nsXPTType::T_I64:
		ret = PyLong_FromLongLong( DEREF_IN_OR_OUT(ns_v.val.i64, PRInt64) );
		break;
	  case nsXPTType::T_U8:
		ret = PyInt_FromLong( DEREF_IN_OR_OUT(ns_v.val.u8, PRUint8) );
		break;
	  case nsXPTType::T_U16:
		ret = PyInt_FromLong( DEREF_IN_OR_OUT(ns_v.val.u16, PRUint16) );
		break;
	  case nsXPTType::T_U32:
		ret = PyInt_FromLong( DEREF_IN_OR_OUT(ns_v.val.u32, PRUint32) );
		break;
	  case nsXPTType::T_U64:
		ret = PyLong_FromUnsignedLongLong( DEREF_IN_OR_OUT(ns_v.val.u64, PRUint64) );
		break;
	  case nsXPTType::T_FLOAT:
		ret = PyFloat_FromDouble(  DEREF_IN_OR_OUT(ns_v.val.f, float) );
		break;
	  case nsXPTType::T_DOUBLE:
		ret = PyFloat_FromDouble(  DEREF_IN_OR_OUT(ns_v.val.d, double) );
		break;
	  case nsXPTType::T_BOOL: {
		PRBool temp = DEREF_IN_OR_OUT(ns_v.val.b, PRBool);
		ret = temp ? Py_True : Py_False;
		Py_INCREF(ret);
		break;
		}
	  case nsXPTType::T_CHAR: {
		char temp = DEREF_IN_OR_OUT(ns_v.val.c, char);
		ret = PyString_FromStringAndSize(&temp, 1);
		break;
		}
	  case nsXPTType::T_WCHAR: {
		PRUnichar temp = (PRUnichar)DEREF_IN_OR_OUT(ns_v.val.wc, PRUnichar);
		ret = PyUnicodeUCS2_FromUnicode(&temp, 1);
		break;
		}
//	  case nsXPTType::T_VOID:
	  case nsXPTType::T_IID: {
		  ret = Py_nsIID::PyObjectFromIID( * DEREF_IN_OR_OUT(ns_v.val.p, const nsIID *) );
		  break;
		}
	  case nsXPTType::T_ASTRING:
	  case nsXPTType::T_DOMSTRING: {
		NS_ABORT_IF_FALSE(is_out || !XPT_PD_IS_DIPPER(td.param_flags), "DOMStrings can't be inout");
		const nsAString *rs = (const nsAString *)ns_v.val.p;
		ret = PyObject_FromNSString(*rs);
		break;
		}
	  case nsXPTType::T_CSTRING:
	  case nsXPTType::T_UTF8STRING: {
		NS_ABORT_IF_FALSE(is_out || !XPT_PD_IS_DIPPER(td.param_flags), "DOMStrings can't be inout");
		const nsCString *rs = (const nsCString *)ns_v.val.p;
		ret = PyObject_FromNSString(*rs, (td.type_flags & XPT_TDP_TAGMASK)==nsXPTType::T_UTF8STRING);
		break;
		}
	  case nsXPTType::T_CHAR_STR: {
		char *t = DEREF_IN_OR_OUT(ns_v.val.p, char *);
		if (t==NULL) {
			ret = Py_None;
			Py_INCREF(Py_None);
		} else
			ret = PyString_FromString(t);
		break;
		}

	  case nsXPTType::T_WCHAR_STR: {
		PRUnichar *us = DEREF_IN_OR_OUT(ns_v.val.p, PRUnichar *);
		if (us==NULL) {
			ret = Py_None;
			Py_INCREF(Py_None);
		} else
			ret = PyUnicodeUCS2_FromUnicode( us, nsCRT::strlen(us));
		break;
		}
	  case nsXPTType::T_INTERFACE_IS: // our Python code does it :-)
	  case nsXPTType::T_INTERFACE: {
		nsISupports *iret = DEREF_IN_OR_OUT(ns_v.val.p, nsISupports *);
		nsXPTParamInfo *pi = (nsXPTParamInfo *)m_info->params+index;
		ret = m_gateway->MakeInterfaceParam(iret, NULL, m_method_index, pi, index);
		break;
		}
/***
		nsISupports *iret = DEREF_IN_OR_OUT(ns_v.val.p, nsISupports *);
		nsXPTParamInfo *pi = (nsXPTParamInfo *)m_info->params+index;
		nsXPTCMiniVariant &ns_viid = m_params[td.argnum];
		NS_ABORT_IF_FALSE((m_python_type_desc_array[td.argnum].type_flags & XPT_TDP_TAGMASK) == nsXPTType::T_IID, "The INTERFACE_IS iid describer isnt an IID!");
		const nsIID * iid = NULL;
		if (XPT_PD_IS_IN(m_python_type_desc_array[td.argnum].param_flags))
			// may still be inout!
			iid = DEREF_IN_OR_OUT(ns_v.val.p, const nsIID *);

		ret = m_gateway->MakeInterfaceParam(iret, iid, m_method_index, pi, index);
		break;
		}
****/
	  case nsXPTType::T_ARRAY: {
		void *t = DEREF_IN_OR_OUT(ns_v.val.p, void *);
		if (t==NULL) {
			ret = Py_None;
			Py_INCREF(Py_None);
		} else {
			PRUint8 array_type;
			nsresult ns = GetArrayType(index, &array_type);
			if (NS_FAILED(ns)) {
				PyXPCOM_BuildPyException(ns);
				break;
			}
			PRUint32 seq_size = GetSizeIs(index, PR_FALSE);
			ret = UnpackSingleArray(t, seq_size, array_type&XPT_TDP_TAGMASK, NULL);
		}
		break;
		}
	  case nsXPTType::T_PSTRING_SIZE_IS: {
		char *t = DEREF_IN_OR_OUT(ns_v.val.p, char *);
		PRUint32 string_size = GetSizeIs(index, PR_TRUE);
		if (t==NULL) {
			ret = Py_None;
			Py_INCREF(Py_None);
		} else
			ret = PyString_FromStringAndSize(t, string_size);
		break;
		}
	  case nsXPTType::T_PWSTRING_SIZE_IS: {
		PRUnichar *t = DEREF_IN_OR_OUT(ns_v.val.p, PRUnichar *);
		PRUint32 string_size = GetSizeIs(index, PR_TRUE);
		if (t==NULL) {
			ret = Py_None;
			Py_INCREF(Py_None);
		} else
			ret = PyUnicodeUCS2_FromUnicode(t, string_size);
		break;
		}
	default:
		// As this is called by external components,
		// we return _something_ rather than failing before any user code has run!
		{
		char buf[128];
		sprintf(buf, "Unknown XPCOM type flags (0x%x)", td.type_flags);
		PyXPCOM_LogWarning("%s - returning a string object with this message!\n", buf);
		ret = PyString_FromString(buf);
		break;
		}
	}
	return ret;
}

nsresult PyXPCOM_GatewayVariantHelper::GetArrayType(PRUint8 index, PRUint8 *ret)
{
	nsCOMPtr<nsIInterfaceInfoManager> iim = XPTI_GetInterfaceInfoManager();
	NS_ABORT_IF_FALSE(iim != nsnull, "Cant get interface from IIM!");
	if (iim==nsnull)
		return NS_ERROR_FAILURE;

	nsCOMPtr<nsIInterfaceInfo> ii;
	nsresult rc = iim->GetInfoForIID( &m_gateway->m_iid, getter_AddRefs(ii));
	if (NS_FAILED(rc))
		return rc;
	nsXPTType datumType;
	const nsXPTParamInfo& param_info = m_info->GetParam((PRUint8)index);
	rc = ii->GetTypeForParam(m_method_index, &param_info, 1, &datumType);
	if (NS_FAILED(rc))
		return rc;
	*ret = datumType.flags;
	return NS_OK;
}

PRBool PyXPCOM_GatewayVariantHelper::GetIIDForINTERFACE_ID(int index, const nsIID **ppret)
{
	// Not sure if the IID pointed at by by this is allows to be
	// in or out, so we will allow it.
	nsXPTParamInfo *pi = (nsXPTParamInfo *)m_info->params+index;
	nsXPTType typ = pi->GetType();
	NS_WARN_IF_FALSE(XPT_TDP_TAG(typ) == nsXPTType::T_IID, "INTERFACE_IS IID param isnt an IID!");
	NS_ABORT_IF_FALSE(typ.IsPointer(), "Expecting to re-fill a pointer value.");
	if (XPT_TDP_TAG(typ) != nsXPTType::T_IID)
		*ppret = &NS_GET_IID(nsISupports);
	else {
		nsXPTCMiniVariant &ns_v = m_params[index];
		if (pi->IsOut()) {
			nsIID **pp = (nsIID **)ns_v.val.p;
			if (pp && *pp)
				*ppret = *pp;
			else
				*ppret = &NS_GET_IID(nsISupports);
		} else if (pi->IsIn()) {
			nsIID *p = (nsIID *)ns_v.val.p;
			if (p)
				*ppret = p;
			else
				*ppret = &NS_GET_IID(nsISupports);
		} else {
			NS_ERROR("Param is not in or out!");
			*ppret = &NS_GET_IID(nsISupports);
		}
	}
	return PR_TRUE;
}

nsIInterfaceInfo *PyXPCOM_GatewayVariantHelper::GetInterfaceInfo()
{
	if (!m_interface_info) {
		nsCOMPtr<nsIInterfaceInfoManager> iim = 
				dont_AddRef(XPTI_GetInterfaceInfoManager());
		if (iim)
			iim->GetInfoForIID(&m_gateway->m_iid, getter_AddRefs(m_interface_info));
	}
	return m_interface_info;
}

#undef FILL_SIMPLE_POINTER
#define FILL_SIMPLE_POINTER( type, ob ) *((type *)ns_v.val.p) = (type)(ob);

nsresult PyXPCOM_GatewayVariantHelper::BackFillVariant( PyObject *val, int index)
{
	nsXPTParamInfo *pi = (nsXPTParamInfo *)m_info->params+index;
	NS_ABORT_IF_FALSE(pi->IsOut() || pi->IsDipper(), "The value must be marked as [out] (or a dipper) to be back-filled!");
	NS_ABORT_IF_FALSE(!pi->IsShared(), "Dont know how to back-fill a shared out param");
	nsXPTCMiniVariant &ns_v = m_params[index];

	nsXPTType typ = pi->GetType();
	PyObject* val_use = NULL;

	NS_ABORT_IF_FALSE(pi->IsDipper() || ns_v.val.p, "No space for result!");
	if (!pi->IsDipper() && !ns_v.val.p) return NS_ERROR_INVALID_POINTER;

	PRBool rc = PR_TRUE;
	switch (XPT_TDP_TAG(typ)) {
	  case nsXPTType::T_I8:
		if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE;
		FILL_SIMPLE_POINTER( PRInt8, PyInt_AsLong(val_use) );
		break;
	  case nsXPTType::T_I16:
		if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE;
		FILL_SIMPLE_POINTER( PRInt16, PyInt_AsLong(val_use) );
		break;
	  case nsXPTType::T_I32:
		if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE;
		FILL_SIMPLE_POINTER( PRInt32, PyInt_AsLong(val_use) );
		break;
	  case nsXPTType::T_I64:
		if ((val_use=PyNumber_Long(val))==NULL) BREAK_FALSE;
		FILL_SIMPLE_POINTER( PRInt64, PyLong_AsLongLong(val_use) );
		break;
	  case nsXPTType::T_U8:
		if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE;
		FILL_SIMPLE_POINTER( PRUint8, PyInt_AsLong(val_use) );
		break;
	  case nsXPTType::T_U16:
		if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE;
		FILL_SIMPLE_POINTER( PRUint16, PyInt_AsLong(val_use) );
		break;
	  case nsXPTType::T_U32:
		if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE;
		FILL_SIMPLE_POINTER( PRUint32, PyInt_AsLong(val_use) );
		break;
	  case nsXPTType::T_U64:
		if ((val_use=PyNumber_Long(val))==NULL) BREAK_FALSE;
		FILL_SIMPLE_POINTER( PRUint64, PyLong_AsUnsignedLongLong(val_use) );
		break;
	  case nsXPTType::T_FLOAT:
		if ((val_use=PyNumber_Float(val))==NULL) BREAK_FALSE
		FILL_SIMPLE_POINTER( float, PyFloat_AsDouble(val_use) );
		break;
	  case nsXPTType::T_DOUBLE:
		if ((val_use=PyNumber_Float(val))==NULL) BREAK_FALSE
		FILL_SIMPLE_POINTER( double, PyFloat_AsDouble(val_use) );
		break;
	  case nsXPTType::T_BOOL:
		if ((val_use=PyNumber_Int(val))==NULL) BREAK_FALSE
		FILL_SIMPLE_POINTER( PRBool, PyInt_AsLong(val_use) );
		break;
	  case nsXPTType::T_CHAR:
		if (!PyString_Check(val) && !PyUnicode_Check(val)) {
			PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
			BREAK_FALSE;
		}
		if ((val_use = PyObject_Str(val))==NULL)
			BREAK_FALSE;
		// Sanity check should PyObject_Str() ever loosen its semantics wrt Unicode!
		NS_ABORT_IF_FALSE(PyString_Check(val_use), "PyObject_Str didnt return a string object!");
		FILL_SIMPLE_POINTER( char, *PyString_AS_STRING(val_use) );
		break;

	  case nsXPTType::T_WCHAR:
		if (!PyString_Check(val) && !PyUnicode_Check(val)) {
			PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
			BREAK_FALSE;
		}
		if ((val_use = PyUnicodeUCS2_FromObject(val))==NULL)
			BREAK_FALSE;
		NS_ABORT_IF_FALSE(PyUnicode_Check(val_use), "PyUnicodeUCS2_FromObject didnt return a Unicode object!");
		FILL_SIMPLE_POINTER( PRUnichar, *PyUnicode_AS_UNICODE(val_use) );
		break;

//	  case nsXPTType::T_VOID:
	  case nsXPTType::T_IID: {
		nsIID iid;
		if (!Py_nsIID::IIDFromPyObject(val, &iid))
			BREAK_FALSE;
		nsIID **pp = (nsIID **)ns_v.val.p;
		// If there is an existing [in] IID, free it.
		if (*pp && pi->IsIn())
			nsMemory::Free(*pp);
		*pp = (nsIID *)nsMemory::Alloc(sizeof(nsIID));
		if (*pp==NULL) {
			PyErr_NoMemory();
			BREAK_FALSE;
		}
		memcpy(*pp, &iid, sizeof(iid));
		break;
		}

	  case nsXPTType::T_ASTRING:
	  case nsXPTType::T_DOMSTRING: {
		nsAString *ws = (nsAString *)ns_v.val.p;
		NS_ABORT_IF_FALSE(ws->Length() == 0, "Why does this writable string already have chars??");
		if (val == Py_None) {
			(*ws) = (PRUnichar *)nsnull;
		} else {
			if (!PyString_Check(val) && !PyUnicode_Check(val)) {
				PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
				BREAK_FALSE;
			}
			val_use = PyUnicodeUCS2_FromObject(val);
			NS_ABORT_IF_FALSE(PyUnicode_Check(val_use), "PyUnicodeUCS2_FromObject didnt return a Unicode object!");
			const PRUnichar *sz = PyUnicode_AS_UNICODE(val_use);
			ws->Assign(sz, PyUnicode_GET_SIZE(val_use));
		}
		break;
		}
	  case nsXPTType::T_CSTRING: {
		nsCString *ws = (nsCString *)ns_v.val.p;
		NS_ABORT_IF_FALSE(ws->Length() == 0, "Why does this writable string already have chars??");
		if (val == Py_None) {
			NS_ABORT_IF_FALSE(0, "dont handle None here yet");
		} else {
			if (!PyString_Check(val) && !PyUnicode_Check(val)) {
				PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
				BREAK_FALSE;
			}
			val_use = PyObject_Str(val);
			NS_ABORT_IF_FALSE(PyString_Check(val_use), "PyObject_Str didnt return a string object!");
			const char *sz = PyString_AS_STRING(val_use);
			ws->Assign(sz, PyString_Size(val_use));
		}
		break;
		}
	  case nsXPTType::T_UTF8STRING: {
		nsCString *ws = (nsCString *)ns_v.val.p;
		NS_ABORT_IF_FALSE(ws->Length() == 0, "Why does this writable string already have chars??");
		if (val == Py_None) {
			NS_ABORT_IF_FALSE(0, "dont handle None here yet");
		} else {
			if (PyString_Check(val)) {
				val_use = val;
				Py_INCREF(val);
			} else if (PyUnicode_Check(val)) {
				val_use = PyUnicode_AsUTF8String(val);
			} else {
				PyErr_SetString(PyExc_TypeError, "UTF8 parameters must be string or Unicode objects");
				BREAK_FALSE;
			}
			NS_ABORT_IF_FALSE(PyString_Check(val_use), "must have a string object!");
			const char *sz = PyString_AS_STRING(val_use);
			ws->Assign(sz, PyString_Size(val_use));
		}
		break;
		}

	  case nsXPTType::T_CHAR_STR: {
		// If it is an existing string, free it.
		char **pp = (char **)ns_v.val.p;
		if (*pp && pi->IsIn())
			nsMemory::Free(*pp);
		*pp = nsnull;

		if (val == Py_None)
			break; // Remains NULL.
		if (!PyString_Check(val) && !PyUnicode_Check(val)) {
			PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
			BREAK_FALSE;
		}
		if ((val_use = PyObject_Str(val))==NULL)
			BREAK_FALSE;
		// Sanity check should PyObject_Str() ever loosen its semantics wrt Unicode!
		NS_ABORT_IF_FALSE(PyString_Check(val_use), "PyObject_Str didnt return a string object!");

		const char *sz = PyString_AS_STRING(val_use);
		int nch = PyString_GET_SIZE(val_use);

		*pp = (char *)nsMemory::Alloc(nch+1);
		if (*pp==NULL) {
			PyErr_NoMemory();
			BREAK_FALSE;
		}
		strncpy(*pp, sz, nch+1);
		break;
		}
	  case nsXPTType::T_WCHAR_STR: {
		// If it is an existing string, free it.
		PRUnichar **pp = (PRUnichar **)ns_v.val.p;
		if (*pp && pi->IsIn())
			nsMemory::Free(*pp);
		*pp = nsnull;
		if (val == Py_None)
			break; // Remains NULL.
		if (!PyString_Check(val) && !PyUnicode_Check(val)) {
			PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
			BREAK_FALSE;
		}
		val_use = PyUnicodeUCS2_FromObject(val);
		NS_ABORT_IF_FALSE(PyUnicode_Check(val_use), "PyUnicodeUCS2_FromObject didnt return a Unicode object!");
		const PRUnichar *sz = PyUnicode_AS_UNICODE(val_use);
		int nch = PyUnicode_GET_SIZE(val_use);

		*pp = (PRUnichar *)nsMemory::Alloc(sizeof(PRUnichar) * (nch+1));
		if (*pp==NULL) {
			PyErr_NoMemory();
			BREAK_FALSE;
		}
		memcpy(*pp, sz, sizeof(PRUnichar) * (nch + 1));
		break;
		}
	  case nsXPTType::T_INTERFACE:  {
		nsISupports *pnew = nsnull;
		// Find out what IID we are declared to use.
		nsIID *iid;
		nsIInterfaceInfo *ii = GetInterfaceInfo();
		if (ii)
			ii->GetIIDForParam(m_method_index, pi, &iid);

		// Get it the "standard" way.
		// We do allow NULL here, even tho doing so will no-doubt crash some objects.
		// (but there will certainly be objects out there that will allow NULL :-(
		nsIID iid_use = iid ? *iid : NS_GET_IID(nsISupports);
		if (!Py_nsISupports::InterfaceFromPyObject(val, iid_use, &pnew, PR_TRUE))
			BREAK_FALSE;
		nsISupports **pp = (nsISupports **)ns_v.val.p;
		if (*pp && pi->IsIn()) {
			Py_BEGIN_ALLOW_THREADS; // MUST release thread-lock, incase a Python COM object that re-acquires.
			(*pp)->Release();
			Py_END_ALLOW_THREADS;
		}

		*pp = pnew; // ref-count added by InterfaceFromPyObject
		break;
		}
	  case nsXPTType::T_INTERFACE_IS: {
		// We do allow NULL here, even tho doing so will no-doubt crash some objects.
		// (but there will certainly be objects out there that will allow NULL :-(
		const nsIID *piid;
		if (!GetIIDForINTERFACE_ID(pi->type.argnum, &piid))
			BREAK_FALSE;

		nsISupports *pnew = nsnull;
		// Get it the "standard" way.
		// We do allow NULL here, even tho doing so will no-doubt crash some objects.
		// (but there will certainly be objects out there that will allow NULL :-(
		if (!Py_nsISupports::InterfaceFromPyObject(val, *piid, &pnew, PR_TRUE))
			BREAK_FALSE;
		nsISupports **pp = (nsISupports **)ns_v.val.p;
		if (*pp && pi->IsIn()) {
			Py_BEGIN_ALLOW_THREADS; // MUST release thread-lock, incase a Python COM object that re-acquires.
			(*pp)->Release();
			Py_END_ALLOW_THREADS;
		}

		*pp = pnew; // ref-count added by InterfaceFromPyObject
		break;
		}

	  case nsXPTType::T_PSTRING_SIZE_IS: {
		const char *sz = nsnull;
		PRUint32 nch = 0;
		if (val != Py_None) {
			if (!PyString_Check(val) && !PyUnicode_Check(val)) {
				PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
				BREAK_FALSE;
			}
			if ((val_use = PyObject_Str(val))==NULL)
				BREAK_FALSE;
			// Sanity check should PyObject_Str() ever loosen its semantics wrt Unicode!
			NS_ABORT_IF_FALSE(PyString_Check(val_use), "PyObject_Str didnt return a string object!");

			sz = PyString_AS_STRING(val_use);
			nch = PyString_GET_SIZE(val_use);
		}
		PRBool bBackFill = PR_FALSE;
		PRBool bCanSetSizeIs = CanSetSizeIs(index, PR_TRUE);
		// If we can not change the size, check our sequence is correct.
		if (!bCanSetSizeIs) {
			PRUint32 existing_size = GetSizeIs(index, PR_TRUE);
			if (nch != existing_size) {
				PyErr_Format(PyExc_ValueError, "This function is expecting a string of exactly length %d - %d characters were passed", existing_size, nch);
				BREAK_FALSE;
			}
			// It we have an "inout" param, but an "in" count, then
			// it is probably a buffer the caller expects us to 
			// fill in-place!
			bBackFill = pi->IsIn();
		}
		if (bBackFill) {
			memcpy(*(char **)ns_v.val.p, sz, nch);
		} else {
			// If we have an existing string, free it!
			char **pp = (char **)ns_v.val.p;
			if (*pp && pi->IsIn())
				nsMemory::Free(*pp);
			*pp = nsnull;
			if (sz==nsnull) // None specified.
				break; // Remains NULL.
			*pp = (char *)nsMemory::Alloc(nch);
			if (*pp==NULL) {
				PyErr_NoMemory();
				BREAK_FALSE;
			}
			memcpy(*pp, sz, nch);
			if (bCanSetSizeIs)
				rc = SetSizeIs(index, PR_TRUE, nch);
			else {
				NS_ABORT_IF_FALSE(GetSizeIs(index, PR_TRUE) == nch, "Can't set sizeis, but string isnt correct size");
			}
		}
		break;
		}

	  case nsXPTType::T_PWSTRING_SIZE_IS: {
		const PRUnichar *sz = nsnull;
		PRUint32 nch = 0;
		PRUint32 nbytes = 0;

		if (val != Py_None) {
			if (!PyString_Check(val) && !PyUnicode_Check(val)) {
				PyErr_SetString(PyExc_TypeError, "This parameter must be a string or Unicode object");
				BREAK_FALSE;
			}
			val_use = PyUnicodeUCS2_FromObject(val);
			NS_ABORT_IF_FALSE(PyUnicode_Check(val_use), "PyUnicodeUCS2_FromObject didnt return a Unicode object!");
			sz = PyUnicode_AS_UNICODE(val_use);
			nch = PyUnicode_GET_SIZE(val_use);
			nbytes = sizeof(PRUnichar) * nch;
		}
		PRBool bBackFill = PR_FALSE;
		PRBool bCanSetSizeIs = CanSetSizeIs(index, PR_TRUE);
		// If we can not change the size, check our sequence is correct.
		if (!bCanSetSizeIs) {
			// It is a buffer the caller prolly wants us to fill in-place!
			PRUint32 existing_size = GetSizeIs(index, PR_TRUE);
			if (nch != existing_size) {
				PyErr_Format(PyExc_ValueError, "This function is expecting a string of exactly length %d - %d characters were passed", existing_size, nch);
				BREAK_FALSE;
			}
			// It we have an "inout" param, but an "in" count, then
			// it is probably a buffer the caller expects us to 
			// fill in-place!
			bBackFill = pi->IsIn();
		}
		if (bBackFill) {
			memcpy(*(PRUnichar **)ns_v.val.p, sz, nbytes);
		} else {
			// If it is an existing string, free it.
			PRUnichar **pp = (PRUnichar **)ns_v.val.p;
			if (*pp && pi->IsIn())
				nsMemory::Free(*pp);
			*pp = nsnull;

			if (val == Py_None)
				break; // Remains NULL.
			*pp = (PRUnichar *)nsMemory::Alloc(nbytes);
			if (*pp==NULL) {
				PyErr_NoMemory();
				BREAK_FALSE;
			}
			memcpy(*pp, sz, nbytes);
			if (bCanSetSizeIs)
				rc = SetSizeIs(index, PR_TRUE, nch);
			else {
				NS_ABORT_IF_FALSE(GetSizeIs(index, PR_TRUE) == nch, "Can't set sizeis, but string isnt correct size");
			}
		}
		break;
		}
	  case nsXPTType::T_ARRAY: {
		// If it is an existing array of the correct size, keep it.
		PRUint32 sequence_size = 0;
		PRUint8 array_type;
		nsresult ns = GetArrayType(index, &array_type);
		if (NS_FAILED(ns))
			return ns;
		PRUint32 element_size = GetArrayElementSize(array_type);
		if (val != Py_None) {
			if (!PySequence_Check(val)) {
				PyErr_Format(PyExc_TypeError, "Object for xpcom array must be a sequence, not type '%s'", val->ob_type->tp_name);
				BREAK_FALSE;
			}
			sequence_size = PySequence_Length(val);
		}
		PRUint32 existing_size = GetSizeIs(index, PR_FALSE);
		PRBool bBackFill = PR_FALSE;
		PRBool bCanSetSizeIs = CanSetSizeIs(index, PR_FALSE);
		// If we can not change the size, check our sequence is correct.
		if (!bCanSetSizeIs) {
			// It is a buffer the caller prolly wants us to fill in-place!
			if (sequence_size != existing_size) {
				PyErr_Format(PyExc_ValueError, "This function is expecting a sequence of exactly length %d - %d items were passed", existing_size, sequence_size);
				BREAK_FALSE;
			}
			// It we have an "inout" param, but an "in" count, then
			// it is probably a buffer the caller expects us to 
			// fill in-place!
			bBackFill = pi->IsIn();
		}
		if (bBackFill)
			rc = FillSingleArray(*(void **)ns_v.val.p, val, sequence_size, element_size, array_type&XPT_TDP_TAGMASK);
		else {
			// If it is an existing array, free it.
			void **pp = (void **)ns_v.val.p;
			if (*pp && pi->IsIn()) {
				FreeSingleArray(*pp, existing_size, array_type);
				nsMemory::Free(*pp);
			}
			*pp = nsnull;
			if (val == Py_None)
				break; // Remains NULL.
			size_t nbytes = sequence_size * element_size;
			if (nbytes==0) nbytes = 1; // avoid assertion about 0 bytes
			*pp = (void *)nsMemory::Alloc(nbytes);
			memset(*pp, 0, nbytes);
			rc = FillSingleArray(*pp, val, sequence_size, element_size, array_type&XPT_TDP_TAGMASK);
			if (!rc) break;
			if (bCanSetSizeIs)
				rc = SetSizeIs(index, PR_FALSE, sequence_size);
			else {
				NS_ABORT_IF_FALSE(GetSizeIs(index, PR_FALSE) == sequence_size, "Can't set sizeis, but string isnt correct size");
			}
		}
		break;
		}
	  default:
		// try and limp along in this case.
		// leave rc TRUE
		PyXPCOM_LogWarning("Converting Python object for an [out] param - The object type (0x%x) is unknown - leaving param alone!\n", XPT_TDP_TAG(typ));
		break;
	}
	Py_XDECREF(val_use);
	if (!rc)
		return NS_ERROR_FAILURE;
	return NS_OK;
}

nsresult PyXPCOM_GatewayVariantHelper::ProcessPythonResult(PyObject *ret_ob)
{
	// NOTE - although we return an nresult, if we leave a Python
	// exception set, then our caller may take additional action
	// (ie, translating our nsresult to a more appropriate nsresult
	// for the Python exception.)
	NS_PRECONDITION(!PyErr_Occurred(), "Expecting no Python exception to be pending when processing the return result");

	nsresult rc = NS_OK;
	// If we dont get a tuple back, then the result is only
	// an int nresult for the underlying function.
	// (ie, the policy is expected to return (NS_OK, user_retval),
	// but can also return (say), NS_ERROR_FAILURE
	if (PyInt_Check(ret_ob))
		return PyInt_AsLong(ret_ob);
	// Now it must be the tuple.
	if (!PyTuple_Check(ret_ob) ||
	    PyTuple_Size(ret_ob)!=2 ||
	    !PyInt_Check(PyTuple_GET_ITEM(ret_ob, 0))) {
		PyErr_SetString(PyExc_TypeError, "The Python result must be a single integer or a tuple of length==2 and first item an int.");
		return NS_ERROR_FAILURE;
	}
	PyObject *user_result = PyTuple_GET_ITEM(ret_ob, 1);
	// Count up how many results our function needs.
	int i;
	int num_results = 0;
	int last_result = -1; // optimization if we only have one - this is it!
	int index_retval = -1;
	for (i=0;i<m_num_type_descs;i++) {
		nsXPTParamInfo *pi = (nsXPTParamInfo *)m_info->params+i;
		if (!m_python_type_desc_array[i].is_auto_out) {
			if (pi->IsOut() || pi->IsDipper()) {
				num_results++;
				last_result = i;
			}
			if (pi->IsRetval())
				index_retval = i;
		}
	}

	if (num_results==0) {
		; // do nothing
	} else if (num_results==1) {
		// May or may not be the nominated retval - who cares!
		NS_ABORT_IF_FALSE(last_result >=0 && last_result < m_num_type_descs, "Have one result, but dont know its index!");
		rc = BackFillVariant( user_result, last_result );
	} else {
		// Loop over each one, filling as we go.
		// We allow arbitary sequences here, but _not_ strings
		// or Unicode!
		// NOTE - We ALWAYS do the nominated retval first.
		// The Python pattern is always:
		// return retval [, byref1 [, byref2 ...] ]
		// But the retval is often the last param described in the info.
		if (!PySequence_Check(user_result) ||
		     PyString_Check(user_result) ||
		     PyUnicode_Check(user_result)) {
			PyErr_SetString(PyExc_TypeError, "This function has multiple results, but a sequence was not given to fill them");
			return NS_ERROR_FAILURE;
		}
		int num_user_results = PySequence_Length(user_result);
		// If they havent given enough, we dont really care.
		// although a warning is probably appropriate.
		if (num_user_results != num_results) {
			const char *method_name = m_info->GetName();
			PyXPCOM_LogWarning("The method '%s' has %d out params, but %d were supplied by the Python code\n",
				method_name,
				num_results,
				num_user_results);
		}
		int this_py_index = 0;
		if (index_retval != -1) {
			// We always return the nominated result first!
			PyObject *sub = PySequence_GetItem(user_result, 0);
			if (sub==NULL)
				return NS_ERROR_FAILURE;
			rc = BackFillVariant(sub, index_retval);
			Py_DECREF(sub);
			this_py_index = 1;
		}
		for (i=0;NS_SUCCEEDED(rc) && i<m_info->GetParamCount();i++) {
			// If weve already done it, or dont need to do it!
			if (i==index_retval || m_python_type_desc_array[i].is_auto_out) 
				continue;
			nsXPTParamInfo *pi = (nsXPTParamInfo *)m_info->params+i;
			if (pi->IsOut()) {
				PyObject *sub = PySequence_GetItem(user_result, this_py_index);
				if (sub==NULL)
					return NS_ERROR_FAILURE;
				rc = BackFillVariant(sub, i);
				Py_DECREF(sub);
				this_py_index++;
			}
		}
	}
	return rc;
}
