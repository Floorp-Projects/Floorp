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

#include "nsAppFileLocProviderProxy.h"
#include "nsILocalFile.h"
#include "nsJavaXPCOMBindingUtils.h"


nsAppFileLocProviderProxy::nsAppFileLocProviderProxy(JNIEnv* env,
                                                     jobject aJavaObject)
  : mJavaEnv(env)
{
  mJavaLocProvider = mJavaEnv->NewGlobalRef(aJavaObject);
}

nsAppFileLocProviderProxy::~nsAppFileLocProviderProxy()
{
  mJavaEnv->DeleteGlobalRef(mJavaLocProvider);
}

NS_IMPL_ISUPPORTS1(nsAppFileLocProviderProxy, nsIDirectoryServiceProvider)

// nsIDirectoryServiceProvider
NS_IMETHODIMP
nsAppFileLocProviderProxy::GetFile(const char* aProp, PRBool* aIsPersistant,
                                   nsIFile** aResult)
{
  // Setup params for calling Java function
  jstring prop = mJavaEnv->NewStringUTF(aProp);
  if (!prop)
    return NS_ERROR_OUT_OF_MEMORY;
  jbooleanArray persistant = mJavaEnv->NewBooleanArray(1);
  if (!persistant)
    return NS_ERROR_OUT_OF_MEMORY;

  // Create method ID
  jmethodID mid = nsnull;
  jclass clazz = mJavaEnv->GetObjectClass(mJavaLocProvider);
  if (clazz) {
    mid = mJavaEnv->GetMethodID(clazz, "getFile",
                                "(Ljava/lang/String;[Z)Ljava/io/File;");
  }
  if (!mid)
    return NS_ERROR_FAILURE;

  // Call Java function
  jobject javaFile = nsnull;
  javaFile = mJavaEnv->CallObjectMethod(mJavaLocProvider, mid, prop,
                                        persistant);
  if (javaFile == nsnull || mJavaEnv->ExceptionCheck())
    return NS_ERROR_FAILURE;

  // Set boolean output value
  jboolean isCopy = PR_FALSE;
  jboolean* array = mJavaEnv->GetBooleanArrayElements(persistant, &isCopy);
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  } else {
    *aIsPersistant = array[0];
    if (isCopy) {
      mJavaEnv->ReleaseBooleanArrayElements(persistant, array, JNI_ABORT);
    }
  }

  // Set nsIFile result value
  nsCOMPtr<nsILocalFile> localFile;
  nsresult rv = File_to_nsILocalFile(mJavaEnv, javaFile,
                                     getter_AddRefs(localFile));
  if (NS_SUCCEEDED(rv)) {
    return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)aResult);
  }

  return rv;
}

