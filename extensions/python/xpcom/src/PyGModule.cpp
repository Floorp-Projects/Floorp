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

// Unfortunately, we can not use an XPConnect object for
// the nsiModule and nsiComponentLoader interfaces.
//  As XPCOM shuts down, it shuts down the interface manager before 
// it releases all the modules.  This is a bit of a problem for 
// us, as it means we can't get runtime info on the interface at shutdown time.

#include "PyXPCOM_std.h"
#include <nsIModule.h>
#include <nsIComponentLoader.h>

class PyG_nsIModule : public PyG_Base, public nsIModule
{
public:
	PyG_nsIModule(PyObject *instance) : PyG_Base(instance, NS_GET_IID(nsIModule)) {;}
	PYGATEWAY_BASE_SUPPORT(nsIModule, PyG_Base);

	NS_IMETHOD GetClassObject(nsIComponentManager *aCompMgr, const nsCID & aClass, const nsIID & aIID, void * *result);
	NS_IMETHOD RegisterSelf(nsIComponentManager *aCompMgr, nsIFile *location, const char *registryLocation, const char *componentType);
	NS_IMETHOD UnregisterSelf(nsIComponentManager *aCompMgr, nsIFile *location, const char *registryLocation);
	NS_IMETHOD CanUnload(nsIComponentManager *aCompMgr, PRBool *_retval);
};

PyG_Base *MakePyG_nsIModule(PyObject *instance)
{
	return new PyG_nsIModule(instance);
}


// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
PyG_nsIModule::GetClassObject(nsIComponentManager *aCompMgr,
                                const nsCID& aClass,
                                const nsIID& aIID,
                                void** r_classObj)
{
	NS_PRECONDITION(r_classObj, "null pointer");
	*r_classObj = nsnull;
	CEnterLeavePython _celp;
	PyObject *cm = Py_nsISupports::PyObjectFromInterface(aCompMgr, NS_GET_IID(nsIComponentManagerObsolete), PR_TRUE);
	PyObject *iid = Py_nsIID::PyObjectFromIID(aIID);
	PyObject *clsid = Py_nsIID::PyObjectFromIID(aClass);
	const char *methodName = "getClassObject";
	PyObject *ret = NULL;
	nsresult nr = InvokeNativeViaPolicy(methodName, &ret, "OOO", cm, clsid, iid);
	Py_XDECREF(cm);
	Py_XDECREF(iid);
	Py_XDECREF(clsid);
	if (NS_SUCCEEDED(nr)) {
		nr = Py_nsISupports::InterfaceFromPyObject(ret, aIID, (nsISupports **)r_classObj, PR_FALSE);
		if (PyErr_Occurred())
			nr = HandleNativeGatewayError(methodName);
	}
	if (NS_FAILED(nr)) {
		NS_ABORT_IF_FALSE(*r_classObj==NULL, "returning error result with an interface - probable leak!");
	}
	Py_XDECREF(ret);
	return nr;
}

NS_IMETHODIMP
PyG_nsIModule::RegisterSelf(nsIComponentManager *aCompMgr,
                              nsIFile* aPath,
                              const char* registryLocation,
                              const char* componentType)
{
	NS_PRECONDITION(aCompMgr, "null pointer");
	NS_PRECONDITION(aPath, "null pointer");
	CEnterLeavePython _celp;
	PyObject *cm = Py_nsISupports::PyObjectFromInterface(aCompMgr, NS_GET_IID(nsIComponentManagerObsolete), PR_TRUE);
	PyObject *path = Py_nsISupports::PyObjectFromInterface(aPath, NS_GET_IID(nsIFile), PR_TRUE);
	const char *methodName = "registerSelf";
	nsresult nr = InvokeNativeViaPolicy(methodName, NULL, "OOzz", cm, path, registryLocation, componentType);
	Py_XDECREF(cm);
	Py_XDECREF(path);
	return nr;
}

NS_IMETHODIMP
PyG_nsIModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                            nsIFile* aPath,
                            const char* registryLocation)
{
	NS_PRECONDITION(aCompMgr, "null pointer");
	NS_PRECONDITION(aPath, "null pointer");
	CEnterLeavePython _celp;
	PyObject *cm = Py_nsISupports::PyObjectFromInterface(aCompMgr, NS_GET_IID(nsIComponentManagerObsolete), PR_TRUE);
	PyObject *path = Py_nsISupports::PyObjectFromInterface(aPath, NS_GET_IID(nsIFile), PR_TRUE);
	const char *methodName = "unregisterSelf";
	nsresult nr = InvokeNativeViaPolicy(methodName, NULL, "OOz", cm, path, registryLocation);
	Py_XDECREF(cm);
	Py_XDECREF(path);
	return nr;
}

NS_IMETHODIMP
PyG_nsIModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
	NS_PRECONDITION(aCompMgr, "null pointer");
	NS_PRECONDITION(okToUnload, "null pointer");
	CEnterLeavePython _celp;
	// we are shutting down - don't ask for a nice wrapped object.
	PyObject *cm = Py_nsISupports::PyObjectFromInterface(aCompMgr, NS_GET_IID(nsIComponentManagerObsolete), PR_TRUE, PR_FALSE);
	const char *methodName = "canUnload";
	PyObject *ret = NULL;
	nsresult nr = InvokeNativeViaPolicy(methodName, &ret, "O", cm);
	Py_XDECREF(cm);
	if (NS_SUCCEEDED(nr)) {
		*okToUnload = PyInt_AsLong(ret);
		if (PyErr_Occurred())
			nr = HandleNativeGatewayError(methodName);
	}
	Py_XDECREF(ret);
	return nr;
}

///////////////////////////////////////////////////////////////////////////////////

class PyG_nsIComponentLoader : public PyG_Base, public nsIComponentLoader
{
public:
	PyG_nsIComponentLoader(PyObject *instance) : PyG_Base(instance, NS_GET_IID(nsIComponentLoader)) {;}
	PYGATEWAY_BASE_SUPPORT(nsIComponentLoader, PyG_Base);

	NS_DECL_NSICOMPONENTLOADER
};

PyG_Base *MakePyG_nsIComponentLoader(PyObject *instance)
{
	return new PyG_nsIComponentLoader(instance);
}

/* nsIFactory getFactory (in nsIIDRef aCID, in string aLocation, in string aType); */
NS_IMETHODIMP PyG_nsIComponentLoader::GetFactory(const nsIID & aCID, const char *aLocation, const char *aType, nsIFactory **_retval)
{
	CEnterLeavePython _celp;
	const char *methodName = "getFactory";
	PyObject *iid = Py_nsIID::PyObjectFromIID(aCID);
	PyObject *ret = NULL;
	nsresult nr = InvokeNativeViaPolicy(methodName, &ret, "Ozz", 
				iid,
				aLocation,
				aType);
	Py_XDECREF(iid);
	if (NS_SUCCEEDED(nr)) {
		Py_nsISupports::InterfaceFromPyObject(ret, NS_GET_IID(nsIFactory), (nsISupports **)_retval, PR_FALSE);
		if (PyErr_Occurred())
			nr = HandleNativeGatewayError(methodName);
	}
	Py_XDECREF(ret);
	return nr;
}

/* void init (in nsIComponentManager aCompMgr, in nsISupports aRegistry); */
NS_IMETHODIMP PyG_nsIComponentLoader::Init(nsIComponentManager *aCompMgr, nsISupports *aRegistry)
{
	CEnterLeavePython _celp;
	const char *methodName = "init";
	PyObject *c = Py_nsISupports::PyObjectFromInterface(aCompMgr, NS_GET_IID(nsIComponentManagerObsolete), PR_TRUE);
	PyObject *r = Py_nsISupports::PyObjectFromInterface(aRegistry, NS_GET_IID(nsISupports), PR_TRUE);
	nsresult nr = InvokeNativeViaPolicy(methodName, NULL, "OO", c, r);
	Py_XDECREF(c);
	Py_XDECREF(r);
	return nr;
}

/* void onRegister (in nsIIDRef aCID, in string aType, in string aClassName, in string aContractID, in string aLocation, in boolean aReplace, in boolean aPersist); */
NS_IMETHODIMP PyG_nsIComponentLoader::OnRegister(const nsIID & aCID, const char *aType, const char *aClassName, const char *aContractID, const char *aLocation, PRBool aReplace, PRBool aPersist)
{
	CEnterLeavePython _celp;
	const char *methodName = "onRegister";
	PyObject *iid = Py_nsIID::PyObjectFromIID(aCID);
	nsresult nr = InvokeNativeViaPolicy(methodName, NULL, "Ossssii", 
				iid,
				aType,
				aClassName, 
				aContractID, 
				aLocation, 
				aReplace,
				aPersist);
	Py_XDECREF(iid);
	return nr;
}

/* void autoRegisterComponents (in long aWhen, in nsIFile aDirectory); */
NS_IMETHODIMP PyG_nsIComponentLoader::AutoRegisterComponents(PRInt32 aWhen, nsIFile *aDirectory)
{
	CEnterLeavePython _celp;
	const char *methodName = "autoRegisterComponents";
	PyObject *c = Py_nsISupports::PyObjectFromInterface(aDirectory, NS_GET_IID(nsIFile), PR_TRUE);
	nsresult nr = InvokeNativeViaPolicy(methodName, NULL, "iO", aWhen, c);
	Py_XDECREF(c);
	return nr;
}

/* boolean autoRegisterComponent (in long aWhen, in nsIFile aComponent); */
NS_IMETHODIMP PyG_nsIComponentLoader::AutoRegisterComponent(PRInt32 aWhen, nsIFile *aComponent, PRBool *_retval)
{
	CEnterLeavePython _celp;
	const char *methodName = "autoRegisterComponent";
	PyObject *ret = NULL;
	PyObject *c = Py_nsISupports::PyObjectFromInterface(aComponent, NS_GET_IID(nsIFile), PR_TRUE);
	nsresult nr = InvokeNativeViaPolicy(methodName, &ret, "iO", aWhen, c);
	Py_XDECREF(c);
	if (NS_SUCCEEDED(nr)) {
		*_retval = PyInt_AsLong(ret);
		if (PyErr_Occurred())
			nr = HandleNativeGatewayError(methodName);
	}
	Py_XDECREF(ret);
	return nr;
}

/* boolean autoUnregisterComponent (in long aWhen, in nsIFile aComponent); */
NS_IMETHODIMP PyG_nsIComponentLoader::AutoUnregisterComponent(PRInt32 aWhen, nsIFile *aComponent, PRBool *_retval)
{
	CEnterLeavePython _celp;
	const char *methodName = "autoUnregisterComponent";
	PyObject *ret = NULL;
	PyObject *c = Py_nsISupports::PyObjectFromInterface(aComponent, NS_GET_IID(nsIFile), PR_TRUE);
	nsresult nr = InvokeNativeViaPolicy(methodName, &ret, "iO", aWhen, c);
	Py_XDECREF(c);
	if (NS_SUCCEEDED(nr)) {
		*_retval = PyInt_AsLong(ret);
		if (PyErr_Occurred())
			nr = HandleNativeGatewayError(methodName);
	}
	Py_XDECREF(ret);
	return nr;
}

/* boolean registerDeferredComponents (in long aWhen); */
NS_IMETHODIMP PyG_nsIComponentLoader::RegisterDeferredComponents(PRInt32 aWhen, PRBool *_retval)
{
	CEnterLeavePython _celp;
	const char *methodName = "registerDeferredComponents";
	PyObject *ret = NULL;
	nsresult nr = InvokeNativeViaPolicy(methodName, &ret, "i", aWhen);
	if (NS_SUCCEEDED(nr)) {
		*_retval = PyInt_AsLong(ret);
		if (PyErr_Occurred())
			nr = HandleNativeGatewayError(methodName);
	}
	Py_XDECREF(ret);
	return nr;
}

/* void unloadAll (in long aWhen); */
NS_IMETHODIMP PyG_nsIComponentLoader::UnloadAll(PRInt32 aWhen)
{
	CEnterLeavePython _celp;
	const char *methodName = "unloadAll";
	return InvokeNativeViaPolicy(methodName, NULL, "i", aWhen);
}


