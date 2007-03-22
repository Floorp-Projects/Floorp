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
 * ***** END LICENSE BLOCK ***** */

#include "nsPyDOM.h"
#include "nsIProgrammingLanguage.h"
#include "nsIScriptTimeoutHandler.h"
#include "nsIArray.h"

class nsPyTimeoutHandler: public nsIScriptTimeoutHandler
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  nsPyTimeoutHandler(const nsAString &aExpr, PyObject *aFunObj,
                     PyObject *obArgv) {
    mFunObj = aFunObj;
    Py_XINCREF(mFunObj);
    mExpr = aExpr;
    NS_ASSERTION(PyTuple_Check(obArgv), "Should be a tuple!");
    // We don't build the argv yet, as we want that to wrap a tuple with
    // the 'lateness' arg added, and tuples are immutable, so we can't change
    // it when we get called multiple times and need to change that last arg.
    mObArgv = obArgv;
	Py_INCREF(mObArgv);
    // mArrayArgv remains empty
    mLateness = 0;
  }
  ~nsPyTimeoutHandler() {
    Py_XDECREF(mFunObj);
    Py_XDECREF(mObArgv);
  }

  virtual const PRUnichar *GetHandlerText() {
    return mExpr.get();
  }
  virtual void *GetScriptObject() {
    return mFunObj;
  }
  virtual void GetLocation(const char **aFileName, PRUint32 *aLineNo) {
    *aFileName = mFileName.get();
    *aLineNo = mLineNo;
  }
  virtual nsIArray *GetArgv();

  virtual PRUint32 GetScriptTypeID() {
        return nsIProgrammingLanguage::PYTHON;
  }
  virtual PRUint32 GetScriptVersion() {
        return 0;
  }

  // Called by the timeout mechanism so the secret 'lateness' arg can be
  // added.
  virtual void SetLateness(PRIntervalTime aHowLate) {
    mLateness = aHowLate;
  }


private:

  // filename, line number of the script 
  nsCAutoString mFileName;
  PRUint32 mLineNo;
  PRIntervalTime mLateness;

  nsCOMPtr<nsIArray> mArrayArgv;
  PyObject *mObArgv;
  // The Python expression or script object
  nsAutoString mExpr;
  PyObject *mFunObj;
};


// QueryInterface implementation for nsPyTimeoutHandler
NS_INTERFACE_MAP_BEGIN(nsPyTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsIScriptTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsPyTimeoutHandler)
NS_IMPL_RELEASE(nsPyTimeoutHandler)


nsIArray *nsPyTimeoutHandler::GetArgv() {
  // Create a new tuple object with our original args, plus one for
  // lateness.
  // Using tuples means we can skip alot of error checking.
  CEnterLeavePython _celp;
  if (!PyTuple_Check(mObArgv)) {
    NS_ERROR("Must be a tuple");
    return nsnull;
  }
  int argc_orig = PyTuple_Size(mObArgv);
  PyObject *obNew = PyTuple_New(argc_orig+1);
  for (int i=0;i<argc_orig;i++) {
    PyTuple_SET_ITEM(obNew, i, PyTuple_GET_ITEM(mObArgv, i));
    Py_INCREF(PyTuple_GET_ITEM(obNew, i));
  }
  PyTuple_SET_ITEM(obNew, argc_orig, PyInt_FromLong(mLateness));
  nsresult nr = NS_CreatePyArgv(obNew, getter_AddRefs(mArrayArgv));
  NS_ASSERTION(NS_SUCCEEDED(nr), "Failed to create argv!?");
  Py_DECREF(obNew); // reference held by mArrayArgv
  return mArrayArgv;
}

// And our factory.
nsresult CreatePyTimeoutHandler(const nsAString &aExpr,
                                PyObject *aFunObj, PyObject *obArgs,
                                nsIScriptTimeoutHandler **aRet) {

  *aRet = new nsPyTimeoutHandler(aExpr, aFunObj, obArgs);
  if (!aRet)
    return NS_ERROR_OUT_OF_MEMORY;
  return (*aRet)->QueryInterface(NS_GET_IID(nsIScriptTimeoutHandler),
                                 NS_REINTERPRET_CAST(void **, aRet));
}
