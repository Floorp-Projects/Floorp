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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Hammond (original author)
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
 * ***** END LICENSE BLOCK *****
 */

#include "nsPyDOM.h"
#include "nsIArray.h"

class nsPyArgArray : public nsIPyArgArray, public nsIArray {
public:
  nsPyArgArray(PyObject *ob);
  ~nsPyArgArray();
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIArray
  NS_DECL_NSIARRAY

  // nsIPyArgArray
  PyObject *GetArgs() {return mObject;}
protected:
  PyObject *mObject;
};

nsPyArgArray::nsPyArgArray(PyObject *ob) :
    mObject(ob)
{
  NS_ASSERTION(PySequence_Check(ob), "You won't get far without a sequence!");
  Py_INCREF(ob);
}

nsPyArgArray::~nsPyArgArray()
{
  Py_DECREF(mObject);
}

// QueryInterface implementation for nsPyArgArray
NS_INTERFACE_MAP_BEGIN(nsPyArgArray)
  NS_INTERFACE_MAP_ENTRY(nsIArray)
  NS_INTERFACE_MAP_ENTRY(nsIPyArgArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPyArgArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsPyArgArray)
NS_IMPL_RELEASE(nsPyArgArray)

// nsIArray impl
NS_IMETHODIMP nsPyArgArray::GetLength(PRUint32 *aLength)
{
  CEnterLeavePython _celp;
  int size = PySequence_Length(mObject);
  if (size==-1) {
    return PyXPCOM_SetCOMErrorFromPyException();
  }
  *aLength = (PRUint32) size;
  return NS_OK;
}

/* void queryElementAt (in unsigned long index, in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP nsPyArgArray::QueryElementAt(PRUint32 index, const nsIID & uuid, void * *result)
{
  *result = nsnull;
  // for now let Python check index validity.  Probably won't get the correct
  // hresult, but its not even clear what that is :)

  if (uuid.Equals(NS_GET_IID(nsIVariant)) || uuid.Equals(NS_GET_IID(nsISupports))) {
    CEnterLeavePython _celp;
    PyObject *sub = PySequence_GetItem(mObject, index);
    if (sub==NULL) {
        return PyXPCOM_SetCOMErrorFromPyException();
    }
    nsresult rv = PyObject_AsVariant(sub, (nsIVariant **)result);
    Py_DECREF(sub);
    return rv;
  }
  NS_WARNING("nsPyArgArray only handles nsIVariant");
  return NS_ERROR_NO_INTERFACE;
}

/* unsigned long indexOf (in unsigned long startIndex, in nsISupports element); */
NS_IMETHODIMP nsPyArgArray::IndexOf(PRUint32 startIndex, nsISupports *element, PRUint32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator enumerate (); */
NS_IMETHODIMP nsPyArgArray::Enumerate(nsISimpleEnumerator **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// The factory function
nsresult NS_CreatePyArgv(PyObject *ob, nsIArray **aArray)
{
  nsPyArgArray *ret = new nsPyArgArray(ob);
  if (ret == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  return ret->QueryInterface(NS_GET_IID(nsIArray), (void **)aArray);
}
