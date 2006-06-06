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
 * Portions created by the Initial Developer are Copyright (C) 2005
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


/**
 * How we handle XPCOM weak references to a Java object:
 *
 * If XPCOM requires or asks for a weak reference of a Java object, we first
 * find (or create) an nsJavaXPTCStub for that Java object.  That way, there is
 * always an nsJavaXPTCStub for any nsJavaXPTCStubWeakRef.  However, the
 * XPTCStub may not always be 'valid'; that is, its refcount may be zero if
 * is not currently referenced by any XPCOM class.
 * When an XPCOM method queries the referent from the weak reference, the 
 * weak ref checks first whether the Java object is still valid.  If so, we can
 * immediately return an addref'd nsJavaXPTCStub.  The XPTCStub takes care of
 * finding an XPTCStub for the required IID.
 */

nsJavaXPTCStubWeakRef::nsJavaXPTCStubWeakRef(jobject aJavaObject,
                                             nsJavaXPTCStub* aXPTCStub)
  : mXPTCStub(aXPTCStub)
{
  JNIEnv* env = GetJNIEnv();
  jobject weakref = env->NewObject(weakReferenceClass,
                                   weakReferenceConstructorMID, aJavaObject);
  mWeakRef = env->NewGlobalRef(weakref);
}

nsJavaXPTCStubWeakRef::~nsJavaXPTCStubWeakRef()
{
  JNIEnv* env = GetJNIEnv();
  env->CallVoidMethod(mWeakRef, clearReferentMID);
  env->DeleteGlobalRef(mWeakRef);
  mXPTCStub->ReleaseWeakRef();
}

NS_IMPL_ADDREF(nsJavaXPTCStubWeakRef)
NS_IMPL_RELEASE(nsJavaXPTCStubWeakRef)

NS_IMPL_QUERY_INTERFACE1(nsJavaXPTCStubWeakRef, nsIWeakReference)

NS_IMETHODIMP
nsJavaXPTCStubWeakRef::QueryReferent(const nsIID& aIID, void** aInstancePtr)
{
  LOG(("nsJavaXPTCStubWeakRef::QueryReferent()\n"));

  // Is weak ref still valid?
  // We create a strong local ref to make sure Java object isn't garbage
  // collected during this call.
  JNIEnv* env = GetJNIEnv();
  jobject javaObject = env->CallObjectMethod(mWeakRef, getReferentMID);
  if (env->IsSameObject(javaObject, NULL))
    return NS_ERROR_NULL_POINTER;

  // Java object has not been garbage collected, so return QI from XPTCStub.
  return mXPTCStub->QueryInterface(aIID, aInstancePtr);
}

