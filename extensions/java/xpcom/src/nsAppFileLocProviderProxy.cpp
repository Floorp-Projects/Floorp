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
#include "nsJavaXPCOMBindingUtils.h"
#include "nsILocalFile.h"
#include "nsISimpleEnumerator.h"


nsAppFileLocProviderProxy::nsAppFileLocProviderProxy(JNIEnv* env,
                                                     jobject aJavaObject)
  : mJavaEnv(env)
{
  mJavaLocProvider = env->NewGlobalRef(aJavaObject);
}

nsAppFileLocProviderProxy::~nsAppFileLocProviderProxy()
{
  mJavaEnv->DeleteGlobalRef(mJavaLocProvider);
}

NS_IMPL_ISUPPORTS2(nsAppFileLocProviderProxy,
                   nsIDirectoryServiceProvider,
                   nsIDirectoryServiceProvider2)


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
  mJavaEnv->GetBooleanArrayRegion(persistant, 0, 1, (jboolean*) aIsPersistant);

  // Set nsIFile result value
  nsCOMPtr<nsILocalFile> localFile;
  nsresult rv = File_to_nsILocalFile(mJavaEnv, javaFile,
                                     getter_AddRefs(localFile));
  if (NS_SUCCEEDED(rv)) {
    return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)aResult);
  }

  return rv;
}


// nsIDirectoryServiceProvider2

class DirectoryEnumerator : public nsISimpleEnumerator
{
public:
  NS_DECL_ISUPPORTS

  DirectoryEnumerator(JNIEnv* env, jobjectArray aJavaFileArray)
    : mJavaEnv(env)
    , mIndex(0)
  {
    mJavaFileArray = NS_STATIC_CAST(jobjectArray,
                                    env->NewGlobalRef(aJavaFileArray));
    mArraySize = env->GetArrayLength(aJavaFileArray);
  }

  ~DirectoryEnumerator()
  {
    mJavaEnv->DeleteGlobalRef(mJavaFileArray);
  }

  NS_IMETHOD HasMoreElements(PRBool* aResult)
  {
    if (!mJavaFileArray) {
      *aResult = PR_FALSE;
    } else {
      *aResult = (mIndex < mArraySize);
    }
    return NS_OK;
  }

  NS_IMETHOD GetNext(nsISupports** aResult)
  {
    nsresult rv = NS_ERROR_FAILURE;

    jobject javaFile = mJavaEnv->GetObjectArrayElement(mJavaFileArray, mIndex++);
    if (javaFile) {
      nsCOMPtr<nsILocalFile> localFile;
      rv = File_to_nsILocalFile(mJavaEnv, javaFile, getter_AddRefs(localFile));
      mJavaEnv->DeleteLocalRef(javaFile);

      if (NS_SUCCEEDED(rv)) {
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)aResult);
      }
    }

    mJavaEnv->ExceptionClear();
    return NS_ERROR_FAILURE;
  }

private:
  JNIEnv*       mJavaEnv;
  jobjectArray  mJavaFileArray;
  PRUint32      mArraySize;
  PRUint32      mIndex;
};

NS_IMPL_ISUPPORTS1(DirectoryEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
nsAppFileLocProviderProxy::GetFiles(const char* aProp,
                                    nsISimpleEnumerator** aResult)
{
  nsresult rv = NS_OK;

  // Setup params for calling Java function
  jstring prop = mJavaEnv->NewStringUTF(aProp);
  if (!prop)
    rv = NS_ERROR_OUT_OF_MEMORY;

  // Create method ID
  jmethodID mid = nsnull;
  if (NS_SUCCEEDED(rv)) {
    jclass clazz = mJavaEnv->GetObjectClass(mJavaLocProvider);
    if (clazz) {
      mid = mJavaEnv->GetMethodID(clazz, "getFiles",
                                  "(Ljava/lang/String;)[Ljava/io/File;");
      mJavaEnv->DeleteLocalRef(clazz);
    }
    if (!mid)
      rv = NS_ERROR_FAILURE;
  }

  // Call Java function
  jobject javaFileArray = nsnull;
  if (NS_SUCCEEDED(rv)) {
    javaFileArray = mJavaEnv->CallObjectMethod(mJavaLocProvider, mid, prop);

    // Handle any exception thrown by Java method.
    jthrowable exp = mJavaEnv->ExceptionOccurred();
    if (exp) {
#ifdef DEBUG
      mJavaEnv->ExceptionDescribe();
#endif

      // If the exception is an instance of XPCOMException, then get the
      // nsresult from the exception instance.  Else, default to
      // NS_ERROR_FAILURE.
      if (mJavaEnv->IsInstanceOf(exp, xpcomExceptionClass)) {
        jfieldID fid;
        fid = mJavaEnv->GetFieldID(xpcomExceptionClass, "errorcode", "J");
        if (fid) {
          rv = mJavaEnv->GetLongField(exp, fid);
        } else {
          rv = NS_ERROR_FAILURE;
        }
        NS_ASSERTION(fid, "Couldn't get 'errorcode' field of XPCOMException");
      } else {
        rv = NS_ERROR_FAILURE;
      }
    } else {
      // No exception thrown.  Check the result.
      if (javaFileArray == nsnull) {
        rv = NS_ERROR_FAILURE;
      }
    }
  }

  if (NS_SUCCEEDED(rv)) {
    // Parse return value
    *aResult = new DirectoryEnumerator(mJavaEnv,
                                       NS_STATIC_CAST(jobjectArray,
                                                      javaFileArray));
    NS_ADDREF(*aResult);
    return NS_OK;
  }

  // Handle error conditions
  *aResult = nsnull;
  mJavaEnv->ExceptionClear();
  return rv;
}

