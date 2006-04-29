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

#ifndef __NSPYDOM_H__
#define __NSPYDOM_H__

#include "PyXPCOM.h"

class nsIScriptTimeoutHandler;
class nsIArray;

PyObject *PyObject_FromNSDOMInterface(PyObject *pycontext, nsISupports *pis,
                                      const nsIID &iid = NS_GET_IID(nsISupports),
                                      PRBool bMakeNicePyObject = PR_TRUE);

nsresult CreatePyTimeoutHandler(const nsAString &aExpr,
                                PyObject *aFunObj, PyObject *obArgs,
                                nsIScriptTimeoutHandler **aRet);

void PyInit_DOMnsISupports();

// A little interface that allows us to provide an object that supports
// nsIArray, but also allows Python to QI for this private interface and
// get the underlying PyObjects directly, without incurring the transfer
// to and from an nsIVariant.

#define NS_IPYARGARRAY_IID \
 { /* {C169DFB6-BA7A-4337-AED6-EA791BB9C04E} */ \
 0xc169dfb6, 0xba7a, 0x4337, \
 { 0xae, 0xd6, 0xea, 0x79, 0x1b, 0xb9, 0xc0, 0x4e } }

class nsIPyArgArray: public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPYARGARRAY_IID)
  virtual PyObject *GetArgs() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPyArgArray, NS_IPYARGARRAY_IID)

nsresult NS_CreatePyArgv(PyObject *ob, nsIArray **aArray);

#endif // __NSPYDOM_H__
