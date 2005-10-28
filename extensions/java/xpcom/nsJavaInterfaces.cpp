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

#include "nsJavaWrapper.h"
#include "nsJavaXPCOMBindingUtils.h"
#include "nsJavaXPTCStub.h"
#include "nsIComponentRegistrar.h"
#include "nsString.h"
#include "nsISimpleEnumerator.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIInputStream.h"
#include "nsEnumeratorUtils.h"
#include "nsArray.h"
#include "nsAppFileLocProviderProxy.h"
#include "nsIEventQueueService.h"
#include "nsXULAppAPI.h"
#include "nsILocalFile.h"

#define GRE_NATIVE(func) Java_org_mozilla_xpcom_internal_GREImpl_##func
#define XPCOM_NATIVE(func) Java_org_mozilla_xpcom_internal_XPCOMImpl_##func


nsresult
InitEmbedding_Impl(JNIEnv* env, jobject aLibXULDirectory,
                   jobject aAppDirectory, jobject aAppDirProvider)
{
  nsresult rv;
  if (!InitializeJavaGlobals(env))
    return NS_ERROR_FAILURE;

  // create an nsILocalFile from given java.io.File
  nsCOMPtr<nsILocalFile> libXULDir;
  if (aLibXULDirectory) {
    rv = File_to_nsILocalFile(env, aLibXULDirectory, getter_AddRefs(libXULDir));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  nsCOMPtr<nsILocalFile> appDir;
  if (aAppDirectory) {
    rv = File_to_nsILocalFile(env, aAppDirectory, getter_AddRefs(appDir));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // create nsAppFileLocProviderProxy from given Java object
  nsCOMPtr<nsIDirectoryServiceProvider> provider;
  if (aAppDirProvider) {
    rv = NS_NewAppFileLocProviderProxy(aAppDirProvider,
                                       getter_AddRefs(provider));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // init libXUL
  rv = XRE_InitEmbedding(libXULDir, appDir, provider);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

extern "C" JX_EXPORT void JNICALL
GRE_NATIVE(initEmbeddingNative) (JNIEnv* env, jobject, jobject aLibXULDirectory,
                                 jobject aAppDirectory, jobject aAppDirProvider)
{
  nsresult rv = InitEmbedding_Impl(env, aLibXULDirectory, aAppDirectory,
                                   aAppDirProvider);

  if (NS_FAILED(rv)) {
    ThrowException(env, rv, "Failure in initEmbedding");
    FreeJavaGlobals(env);
  }
}

extern "C" JX_EXPORT void JNICALL
GRE_NATIVE(termEmbedding) (JNIEnv *env, jobject)
{
  // Free globals before calling XRE_TermEmbedding(), since we need some
  // XPCOM services.
  FreeJavaGlobals(env);

  XRE_TermEmbedding();
}

nsresult
InitXPCOM_Impl(JNIEnv* env, jobject aMozBinDirectory,
               jobject aAppFileLocProvider, jobject* aResult)
{
  nsresult rv;
  if (!InitializeJavaGlobals(env))
    return NS_ERROR_FAILURE;

  // create an nsILocalFile from given java.io.File
  nsCOMPtr<nsILocalFile> directory;
  if (aMozBinDirectory) {
    rv = File_to_nsILocalFile(env, aMozBinDirectory, getter_AddRefs(directory));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // create nsAppFileLocProviderProxy from given Java object
  nsAppFileLocProviderProxy* provider = nsnull;
  if (aAppFileLocProvider) {
    provider = new nsAppFileLocProviderProxy(aAppFileLocProvider);
    if (!provider)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  // init XPCOM
  nsIServiceManager* servMan = nsnull;
  rv = NS_InitXPCOM2(&servMan, directory, provider);
  if (provider) {
    delete provider;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // init Event Queue
  nsCOMPtr<nsIEventQueueService>
             eventQService(do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = eventQService->CreateThreadEventQueue();
  NS_ENSURE_SUCCESS(rv, rv);

  // create Java proxy for service manager returned by NS_InitXPCOM2
  rv = CreateJavaProxy(env, servMan, NS_GET_IID(nsIServiceManager),
                       aResult);
  NS_RELEASE(servMan);   // Java proxy has owning ref

  return rv;
}

extern "C" JX_EXPORT jobject JNICALL
XPCOM_NATIVE(initXPCOM) (JNIEnv* env, jobject, jobject aMozBinDirectory,
                         jobject aAppFileLocProvider)
{
  jobject servMan;
  nsresult rv = InitXPCOM_Impl(env, aMozBinDirectory, aAppFileLocProvider,
                               &servMan);
  if (NS_SUCCEEDED(rv))
    return servMan;

  ThrowException(env, rv, "Failure in initXPCOM");
  FreeJavaGlobals(env);
  return nsnull;
}

extern "C" JX_EXPORT void JNICALL
XPCOM_NATIVE(shutdownXPCOM) (JNIEnv *env, jobject, jobject aServMgr)
{
  nsresult rv;
  nsCOMPtr<nsIServiceManager> servMgr;
  if (aServMgr) {
    // Get native XPCOM instance
    void* xpcom_obj;
    rv = GetXPCOMInstFromProxy(env, aServMgr, &xpcom_obj);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to get XPCOM obj for ServiceMgr.");

    // Even if we failed to get the matching xpcom object, we don't abort this
    // function.  Just call NS_ShutdownXPCOM with a null service manager.

    if (NS_SUCCEEDED(rv)) {
      JavaXPCOMInstance* inst = NS_STATIC_CAST(JavaXPCOMInstance*, xpcom_obj);
      servMgr = do_QueryInterface(inst->GetInstance());
    }
  }

  // Free globals before calling NS_ShutdownXPCOM(), since we need some
  // XPCOM services.
  FreeJavaGlobals(env);

  rv = NS_ShutdownXPCOM(servMgr);
  if (NS_FAILED(rv))
    ThrowException(env, rv, "NS_ShutdownXPCOM failed");
}

extern "C" JX_EXPORT jobject JNICALL
XPCOM_NATIVE(newLocalFile) (JNIEnv *env, jobject, jstring aPath,
                            jboolean aFollowLinks)
{
  // Create a Mozilla string from the jstring
  const PRUnichar* buf = nsnull;
  if (aPath) {
    buf = env->GetStringChars(aPath, nsnull);
    if (!buf)
      return nsnull;  // exception already thrown
  }

  nsAutoString path_str(buf);
  env->ReleaseStringChars(aPath, buf);

  // Make call to given function
  nsILocalFile* file = nsnull;
  nsresult rv = NS_NewLocalFile(path_str, aFollowLinks, &file);

  if (NS_SUCCEEDED(rv)) {
    jobject javaProxy;
    rv = CreateJavaProxy(env, file, NS_GET_IID(nsILocalFile), &javaProxy);
    NS_RELEASE(file);   // JavaXPCOMInstance has owning ref
    if (NS_SUCCEEDED(rv))
      return javaProxy;
  }

  ThrowException(env, rv, "Failure in newLocalFile");
  return nsnull;
}

extern "C" JX_EXPORT jobject JNICALL
XPCOM_NATIVE(getComponentManager) (JNIEnv *env, jobject)
{
  // Call XPCOM method
  nsIComponentManager* cm = nsnull;
  nsresult rv = NS_GetComponentManager(&cm);

  if (NS_SUCCEEDED(rv)) {
    jobject javaProxy;
    rv = CreateJavaProxy(env, cm, NS_GET_IID(nsIComponentManager), &javaProxy);
    NS_RELEASE(cm);   // JavaXPCOMInstance has owning ref
    if (NS_SUCCEEDED(rv))
      return javaProxy;
  }

  ThrowException(env, rv, "Failure in getComponentManager");
  return nsnull;
}

extern "C" JX_EXPORT jobject JNICALL
XPCOM_NATIVE(getComponentRegistrar) (JNIEnv *env, jobject)
{
  // Call XPCOM method
  nsIComponentRegistrar* cr = nsnull;
  nsresult rv = NS_GetComponentRegistrar(&cr);

  if (NS_SUCCEEDED(rv)) {
    jobject javaProxy;
    rv = CreateJavaProxy(env, cr, NS_GET_IID(nsIComponentRegistrar), &javaProxy);
    NS_RELEASE(cr);   // JavaXPCOMInstance has owning ref
    if (NS_SUCCEEDED(rv))
      return javaProxy;
  }

  ThrowException(env, rv, "Failure in getComponentRegistrar");
  return nsnull;
}

extern "C" JX_EXPORT jobject JNICALL
XPCOM_NATIVE(getServiceManager) (JNIEnv *env, jobject)
{
  // Call XPCOM method
  nsIServiceManager* sm = nsnull;
  nsresult rv = NS_GetServiceManager(&sm);

  if (NS_SUCCEEDED(rv)) {
    jobject javaProxy;
    rv = CreateJavaProxy(env, sm, NS_GET_IID(nsIServiceManager), &javaProxy);
    NS_RELEASE(sm);   // JavaXPCOMInstance has owning ref
    if (NS_SUCCEEDED(rv))
      return javaProxy;
  }

  ThrowException(env, rv, "Failure in getServiceManager");
  return nsnull;
}

