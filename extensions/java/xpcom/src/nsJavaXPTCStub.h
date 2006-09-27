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

#ifndef _nsJavaXPTCStub_h_
#define _nsJavaXPTCStub_h_

#include "xptcall.h"
#include "jni.h"
#include "nsVoidArray.h"
#include "nsIInterfaceInfo.h"
#include "nsCOMPtr.h"


class nsJavaXPTCStub : public nsXPTCStubBase
{
public:
  NS_DECL_ISUPPORTS

  nsJavaXPTCStub(JNIEnv* aJavaEnv, jobject aJavaObject, nsIInterfaceInfo *aIInfo);

  virtual ~nsJavaXPTCStub();

  // return a refcounted pointer to the InterfaceInfo for this object
  // NOTE: on some platforms this MUST not fail or we crash!
  NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo **aInfo);

  // call this method and return result
  NS_IMETHOD CallMethod(PRUint16 aMethodIndex,
                        const nsXPTMethodInfo *aInfo,
                        nsXPTCMiniVariant *aParams);

private:
//  NS_HIDDEN ~JavaStub();

  // returns a weak reference to a child supporting the specified interface
//  NS_HIDDEN_(JavaStub *) FindStubSupportingIID(const nsID &aIID);
  nsJavaXPTCStub * FindStubSupportingIID(const nsID &aIID);

  // returns true if this stub supports the specified interface
//  NS_HIDDEN_(PRBool) SupportsIID(const nsID &aIID);
  PRBool SupportsIID(const nsID &aIID);

  nsresult SetupJavaParams(const nsXPTParamInfo &aParamInfo,
                           const nsXPTMethodInfo* aMethodInfo,
                           PRUint16 aMethodIndex,
                           nsXPTCMiniVariant* aDispatchParams,
                           nsXPTCMiniVariant &aVariant,
                           jvalue &aJValue, nsACString &aMethodSig);
  nsresult GetRetvalSig(const nsXPTParamInfo* aParamInfo,
                        nsACString &aRetvalSig);
  nsresult FinalizeJavaParams(const nsXPTParamInfo &aParamInfo,
                              const nsXPTMethodInfo* aMethodInfo,
                              PRUint16 aMethodIndex,
                              nsXPTCMiniVariant* aDispatchParams,
                              nsXPTCMiniVariant &aVariant,
                              jvalue &aJValue);
  nsresult SetXPCOMRetval();

  JNIEnv* mJavaEnv;
  jobject mJavaObject;
  nsCOMPtr<nsIInterfaceInfo> mIInfo;

  nsVoidArray   mChildren; // weak references (cleared by the children)
  nsJavaXPTCStub *mMaster;   // strong reference
};

inline void* SetAsXPTCStub(nsJavaXPTCStub* ptr)
  { NS_PRECONDITION(ptr, "null pointer");
    return (void*) (((unsigned long) ptr) | 0x1); }
 
 inline PRBool IsXPTCStub(void* ptr)
  { NS_PRECONDITION(ptr, "null pointer");
    return ((unsigned long) ptr) & 0x1; }
 
 inline nsJavaXPTCStub* GetXPTCStubAddr(void* ptr)
  { NS_PRECONDITION(ptr, "null pointer");
    return (nsJavaXPTCStub*) (((unsigned long) ptr) & ~0x1); }

#endif // _nsJavaXPTCStub_h_
