/*
 * The contents of this file are subject to the Mozilla Public License 
 * Version 1.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for 
 * the specific language governing rights and limitations under the License.
 *
 * Contributors:
 *    Frank Mitchell (frank.mitchell@sun.com)
 */
#include <jni.h>
#include "nsISupports.h"
#include "xptcall.h"

extern nsresult InitXPCOM();

extern jboolean InitJavaCaches(JNIEnv *env);

extern nsISupports *This(JNIEnv *env, jobject self);

extern nsID *This_IID(JNIEnv *env, jobject self);

extern jobject NewComObject(JNIEnv *env,
			    nsISupports *xpcobj, const nsIID *iid);

extern int BuildParamsForOffset(const nsID *iid, jint offset, 
				  nsXPTCVariant **_retval);

extern void BuildParamsForMethodInfo(const nsXPTMethodInfo *mi, 
				     nsXPTCVariant variantArray[]);

extern nsresult GetMethodInfo(const nsID *iid, jint offset, 
			      const nsXPTMethodInfo **_retval);

extern nsresult GetMethodInfoByName(const nsID *iid, 
				    const char *methodname,
				    PRBool wantSetter, 
				    const nsXPTMethodInfo **miptr,
				    int *_retval);

extern void PrintParams(const nsXPTCVariant params[], int paramcount);

extern nsresult JArrayToVariant(JNIEnv *env, 
				int paramcount,
				nsXPTCVariant *params, 
				const jobjectArray jarray);

extern jboolean JObjectToVariant(JNIEnv *env, 
				 nsXPTCVariant *param, 
				 const jobject obj);

extern nsresult VariantToJArray(JNIEnv *env, 
				jobjectArray jarray,
				int paramcount,
				nsXPTCVariant *params);

extern jobject  VariantToJObject(JNIEnv *env, 
				 const nsXPTCVariant *param);


/* Defined in nsID.cpp */
extern jobject  ID_NewJavaID(JNIEnv *env, const nsID* id);
extern nsID*    ID_GetNative(JNIEnv *env, jobject self);
extern void     ID_SetNative(JNIEnv *env, jobject self, nsID* id);
extern jboolean ID_IsEqual(JNIEnv *env, jobject self, jobject other);

