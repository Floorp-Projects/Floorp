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

// PyXPTStub - the stub for implementing interfaces.
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

void *PyXPCOM_XPTStub::ThisAsIID(const nsIID &iid)
{
	if (iid.Equals(NS_GET_IID(nsISupports)))
	    return (nsISupports *)(nsXPTCStubBase *)this;
	else if (iid.Equals(m_iid))
		return (nsISupports *)(nsXPTCStubBase *)this;
	else
		return PyG_Base::ThisAsIID(iid);
}


NS_IMETHODIMP
PyXPCOM_XPTStub::GetInterfaceInfo(nsIInterfaceInfo** info)
{
	NS_PRECONDITION(info, "NULL pointer");
	if (info==nsnull)
		return NS_ERROR_NULL_POINTER;
	// Simply get the XPCOM runtime to provide this
	// (but there must be some reason why they dont get it themselves!?
	// Maybe because they dont know the IID?
	nsCOMPtr<nsIInterfaceInfoManager> iim = XPTI_GetInterfaceInfoManager();
	NS_ABORT_IF_FALSE(iim != nsnull, "Cant get interface from IIM!");
	if (iim==nsnull)
		return NS_ERROR_FAILURE;

	return iim->GetInfoForIID( &m_iid, info);
}

// call this method and return result
NS_IMETHODIMP
PyXPCOM_XPTStub::CallMethod(PRUint16 methodIndex,
                          const nsXPTMethodInfo* info,
                          nsXPTCMiniVariant* params)
{
	nsresult rc = NS_ERROR_FAILURE;
	NS_PRECONDITION(info, "NULL methodinfo pointer");
	NS_PRECONDITION(params, "NULL variant pointer");
	CEnterLeavePython _celp;
	PyObject *obParams = NULL;
	PyObject *result = NULL;
	PyObject *obThisObject = NULL;
	PyObject *obMI = PyObject_FromXPTMethodDescriptor(info);
	PyXPCOM_GatewayVariantHelper arg_helper(this, methodIndex, info, params);
	if (obMI==NULL)
		goto done;
	// base object is passed raw.
	obThisObject = Py_nsISupports::PyObjectFromInterface((nsIInternalPython*)this, NS_GET_IID(nsISupports), PR_TRUE, PR_FALSE);
	obParams = arg_helper.MakePyArgs();
	if (obParams==NULL)
		goto done;
	result = PyObject_CallMethod(m_pPyObject, 
	                                       "_CallMethod_",
					       "OiOO",
					       obThisObject,
					       (int)methodIndex,
					       obMI,
					       obParams);
	if (result!=NULL) {
		rc = arg_helper.ProcessPythonResult(result);
		// Use an xor to check failure && pyerr, or !failure && !pyerr.
		NS_ABORT_IF_FALSE( ((NS_FAILED(rc)!=0)^(PyErr_Occurred()!=0)) == 0, "We must have failure with a Python error, or success without a Python error.");
	}
done:
	if (PyErr_Occurred()) {
		// The error handling - fairly involved, but worth it as
		// good error reporting is critical for users to know WTF 
		// is going on - especially with TypeErrors etc in their
		// return values (ie, after the Python code has successfully
		// exited, but we encountered errors unpacking the
		// result values for the COM caller - there is literally no 
		// way to catch these exceptions from Python code, as their
		// is no Python function on the call-stack)

		// First line of attack in an error is to call-back on the policy.
		// If the callback of the error handler succeeds and returns an
		// integer (for the nsresult), we take no further action.

		// If this callback fails, we log _2_ exceptions - the error handler
		// error, and the original error.

		PRBool bProcessMainError = PR_TRUE; // set to false if our exception handler does its thing!
		PyObject *exc_typ, *exc_val, *exc_tb;
		PyErr_Fetch(&exc_typ, &exc_val, &exc_tb);
		PyErr_NormalizeException( &exc_typ, &exc_val, &exc_tb);

		PyObject *err_result = PyObject_CallMethod(m_pPyObject, 
	                                       "_CallMethodException_",
					       "OiOO(OOO)",
					       obThisObject,
					       (int)methodIndex,
					       obMI,
					       obParams,
		                               exc_typ ? exc_typ : Py_None, // should never be NULL, but defensive programming...
		                               exc_val ? exc_val : Py_None, // may well be NULL.
					       exc_tb ? exc_tb : Py_None); // may well be NULL.
		if (err_result == NULL) {
			PyXPCOM_LogError("The exception handler _CallMethodException_ failed!\n");
		} else if (err_result == Py_None) {
			// The exception handler has chosen not to do anything with
			// this error, so we still need to print it!
			;
		} else if (PyInt_Check(err_result)) {
			// The exception handler has given us the nresult.
			rc = PyInt_AsLong(err_result);
			bProcessMainError = PR_FALSE;
		} else {
			// The exception handler succeeded, but returned other than
			// int or None.
			PyXPCOM_LogError("The _CallMethodException_ handler returned object of type '%s' - None or an integer expected\n", err_result->ob_type->tp_name);
		}
		Py_XDECREF(err_result);
		PyErr_Restore(exc_typ, exc_val, exc_tb);
		if (bProcessMainError) {
			PyXPCOM_LogError("The function '%s' failed\n", info->GetName());
			rc = PyXPCOM_SetCOMErrorFromPyException();
		}
		// else everything is already setup,
		// just clear the Python error state.
		PyErr_Clear();
	}

	Py_XDECREF(obMI);
	Py_XDECREF(obParams);
	Py_XDECREF(obThisObject);
	Py_XDECREF(result);
	return rc;
}
