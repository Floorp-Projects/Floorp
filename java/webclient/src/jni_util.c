#include <stdlib.h>
#include <string.h>

#include "jni.h"
#include "jni_util.h"


JNIEXPORT jvalue JNICALL
JNU_CallMethodByName(JNIEnv *env, 
					 jboolean *hasException,
					 jobject obj, 
					 const char *name,
					 const char *signature,
					 ...)
{
    jvalue result;
    va_list args;
	
    va_start(args, signature);
    result = JNU_CallMethodByNameV(env, hasException, obj, name, signature, 
								   args); 
    va_end(args);
	
    return result;    
}

JNIEXPORT jvalue JNICALL
JNU_CallMethodByNameV(JNIEnv *env, 
					  jboolean *hasException,
					  jobject obj, 
					  const char *name,
					  const char *signature, 
					  va_list args)
{
    jclass clazz;
    jmethodID mid;
    jvalue result;
    const char *p = signature;
    printf("jni_util: call method by name if 0");
    /* find out the return type */
    while (*p && *p != ')')
        p++;
    p++;
	
    result.i = 0;
	
    if ((*env)->EnsureLocalCapacity(env, 3) < 0)
        goto done2;
    printf("jni_util: call method by name if 1");
    clazz = (*env)->GetObjectClass(env, obj);
    printf("jni_util: call method by name 2");
    mid = (*env)->GetMethodID(env, clazz, name, signature);
    printf("jni_util: call method by name 3");
    if (mid == 0)
        goto done1;
    printf("jni_util: call method by name if 4");
    switch (*p) {
    case 'V':
        (*env)->CallVoidMethodV(env, obj, mid, args);
		break;
    case '[':
    case 'L':
        result.l = (*env)->CallObjectMethodV(env, obj, mid, args);
		break;
    case 'Z':
        result.z = (*env)->CallBooleanMethodV(env, obj, mid, args);
		break;
    case 'B':
        result.b = (*env)->CallByteMethodV(env, obj, mid, args);
		break;
    case 'C':
        result.c = (*env)->CallCharMethodV(env, obj, mid, args);
		break;
    case 'S':
        result.s = (*env)->CallShortMethodV(env, obj, mid, args);
		break;
    case 'I':
        result.i = (*env)->CallIntMethodV(env, obj, mid, args);
		break;
    case 'J':
        result.j = (*env)->CallLongMethodV(env, obj, mid, args);
		break;
    case 'F':
        result.f = (*env)->CallFloatMethodV(env, obj, mid, args);
		break;
    case 'D':
        result.d = (*env)->CallDoubleMethodV(env, obj, mid, args);
		break;
    default:
        (*env)->FatalError(env, "JNU_CallMethodByNameV: illegal signature");
    }
  done1:
    (*env)->DeleteLocalRef(env, clazz);
  done2:
    if (hasException) {
        *hasException = (*env)->ExceptionCheck(env);
    }
    return result;    
}

JNIEXPORT void * JNICALL
JNU_GetEnv(JavaVM *vm, jint version)
{
    void *env;
    (*vm)->GetEnv(vm, &env, version);
    return env;
}
