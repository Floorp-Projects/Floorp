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
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
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

#include "jni.h"
#include "nsJavaXPTCStubWeakRef.h"
#include "nsJavaXPTCStub.h"
#include "nsJavaXPCOMBindingUtils.h"
#include "nsIInterfaceInfoManager.h"


nsJavaXPTCStubWeakRef::nsJavaXPTCStubWeakRef(JNIEnv* env, jobject aJavaObject)
{
  mJavaEnv = env;
  mWeakRef = mJavaEnv->NewWeakGlobalRef(aJavaObject);
}

nsJavaXPTCStubWeakRef::~nsJavaXPTCStubWeakRef()
{
  mJavaEnv->DeleteWeakGlobalRef(mWeakRef);
}

NS_IMPL_ADDREF(nsJavaXPTCStubWeakRef)
NS_IMPL_RELEASE(nsJavaXPTCStubWeakRef)

NS_IMPL_THREADSAFE_QUERY_INTERFACE1(nsJavaXPTCStubWeakRef, nsIWeakReference)

NS_IMETHODIMP
nsJavaXPTCStubWeakRef::QueryReferent(const nsIID& aIID, void** aInstancePtr)
{
  // Is weak ref still valid?
  // We create a strong local ref to make sure Java object isn't garbage
  // collected during this call.
  jobject javaObject = mJavaEnv->NewLocalRef(mWeakRef);
  if (!javaObject)
    return NS_ERROR_NULL_POINTER;

  // Java object has not been garbage collected.  Do we have an
  // associated nsJavaXPTCStub?
  void* inst = GetMatchingXPCOMObject(mJavaEnv, javaObject);
  NS_ASSERTION(IsXPTCStub(inst), "Found xpcom object was not an XPTCStub");

  if (!inst) {
    // No XPTCStub exists, so create one

    // Get interface info for class
    nsCOMPtr<nsIInterfaceInfoManager> iim = XPTI_GetInterfaceInfoManager();
    nsCOMPtr<nsIInterfaceInfo> iinfo;
    iim->GetInfoForIID(&aIID, getter_AddRefs(iinfo));

    // Create XPCOM stub
    nsJavaXPTCStub* xpcomStub = new nsJavaXPTCStub(mJavaEnv, javaObject, iinfo);
    NS_ADDREF(xpcomStub);
    AddJavaXPCOMBinding(mJavaEnv, javaObject, SetAsXPTCStub(xpcomStub));

    // return created stub
    *aInstancePtr = (void*) xpcomStub;
    return NS_OK;
  }

  // We have an exising XPTCStub, so return QI result
  nsJavaXPTCStub* xpcomStub = GetXPTCStubAddr(inst);
  return xpcomStub->QueryInterface(aIID, aInstancePtr);
}

