/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the IDispatch implementation for XPConnect
 *
 * The Initial Developer of the Original Code is
 * David Bradley.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "XPCPrivate.h"

nsDispatchSupport* nsDispatchSupport::mInstance = nsnull;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDispatchSupport, nsIDispatchSupport)

nsDispatchSupport::nsDispatchSupport()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsDispatchSupport::~nsDispatchSupport()
{
  /* destructor code */
}

/* void COMVariant2JSVal (in COMVARIANTPtr comvar, out JSVal val); */
NS_IMETHODIMP nsDispatchSupport::COMVariant2JSVal(VARIANT * comvar, jsval *val)
{
    XPCCallContext ccx(NATIVE_CALLER);
    nsresult retval;
    XPCDispConvert::COMToJS(ccx, *comvar, *val, retval);
    return retval;
}

/* void COMArray2JSArray (in COMVARIANTPtr comvar, out JSObjectPtr obj); */
NS_IMETHODIMP nsDispatchSupport::JSVal2COMVariant(jsval val, VARIANT * comvar)
{
    XPCCallContext ccx(NATIVE_CALLER);
    nsresult retval;
    XPCDispConvert::JSToCOM(ccx, val, *comvar, retval);
    return retval;
}

NS_IMETHODIMP nsDispatchSupport::CreateInstance(const char * className, PRBool testScriptability, IDispatch ** result)
{
    return XPCDispObject::COMCreateInstance(className, testScriptability, result);
}

nsDispatchSupport* nsDispatchSupport::GetSingleton()
{
    if(!mInstance)
    {
        mInstance = new nsDispatchSupport;
        NS_IF_ADDREF(mInstance);
    }
    NS_IF_ADDREF(mInstance);
    return mInstance;
}
