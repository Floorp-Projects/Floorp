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
#include <iostream.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "nscore.h" 
#include "nsIFactory.h" 
#include "nsIComponentManager.h" 
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xptinfo.h"
#include "xptcall.h"
#include "xpt_struct.h"
#include "nsIAllocator.h"
#include "nsSpecialSystemDirectory.h" 

#include "xpjava.h"


#ifdef XP_PC
#define XPCOM_DLL  "xpcom32.dll"
#define SAMPLE_DLL "sampl32.dll"
#else
#ifdef XP_MAC
#define XPCOM_DLL  "XPCOM_DLL"
#define SAMPLE_DLL "SAMPL_DLL"
#else
#define XPCOM_DLL  "libxpcom.so"
#define SAMPLE_DLL "libsample.so"
#endif
#endif


/* Global references to frequently used classes and objects */
static jclass classString;
static jclass classByte;
static jclass classShort;
static jclass classInteger;
static jclass classLong;
static jclass classFloat;
static jclass classDouble;
static jclass classBoolean;
static jclass classCharacter;

/* Cached field and method IDs */
static jfieldID Byte_value_ID;
static jfieldID Short_value_ID;
static jfieldID Integer_value_ID;
static jfieldID Long_value_ID;
static jfieldID Float_value_ID;
static jfieldID Double_value_ID;
static jfieldID Boolean_value_ID;
static jfieldID Character_value_ID;

static jmethodID Byte_init_ID;
static jmethodID Short_init_ID;
static jmethodID Integer_init_ID;
static jmethodID Long_init_ID;
static jmethodID Float_init_ID;
static jmethodID Double_init_ID;
static jmethodID Boolean_init_ID;
static jmethodID Character_init_ID;

/* Global references to frequently used XPCOM objects */
static nsIInterfaceInfoManager *interfaceInfoManager;

/* Flag to indicate caches filled */
static jboolean cache_initialized = JNI_FALSE;


/* ---------- GENERAL API FUNCTIONS ------------ */

nsresult xpj_InitXPCOM() {
    nsresult res;

#ifdef DEBUG_frankm
    cerr << "Initializing XPCOM" << endl;
#endif

    // Autoregistration magic.  Boogeda boogeda.
    res = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, 
					   nsnull);

#ifdef DEBUG_frankm
    cerr << "XPCOM Initialized" << endl;
#endif

    return res;
}

nsresult xpj_GetMethodInfoByName(const nsID *iid, 
				 const char *methodname, 
				 PRBool wantSetter,
				 const nsXPTMethodInfo **miptr,
				 int *_retval) {
    nsresult res;

    // Get info
    nsIInterfaceInfo *info = nsnull;

    res = interfaceInfoManager->GetInfoForIID(iid, &info);

    if (NS_FAILED(res)) {
        cerr << "Failed to find info for " << iid->ToString() << endl;
        return res;
    }

    // Find info for command
    uint16 methodcount;

    info->GetMethodCount(&methodcount);

    int i;
    for (i = 0; i < methodcount; i++) {
        const nsXPTMethodInfo *mi;

        info->GetMethodInfo(i, &mi);
        // PENDING(frankm): match against name, get/set, *AND* param types
        if (strcmp(methodname, mi->GetName()) == 0) {
            PRBool setter = mi->IsSetter();
            PRBool getter = mi->IsGetter();
            if ((!getter && !setter) 
                || (setter && wantSetter)
                || (getter && !wantSetter)) {
                *miptr = mi;
                *_retval = i;
                break;
            }
        }
    }
    if (i >= methodcount) {
        cerr << "Failed to find " << methodname << endl;
        *miptr = NULL;
        *_retval = -1;
        return NS_ERROR_FAILURE;
    }
    return res;
}

/* ---------- DISPATCH SUPPORT FUNCTIONS ------------ */

static 
int xpjd_BuildParamsForOffset(nsIInterfaceInfo *info, 
			      jint offset,
			      jint arraysize,
			      nsXPTCVariant **_retval) {
    const nsXPTMethodInfo *mi;
    nsresult res;

    // Find info for command
    uint16 methodcount;

    info->GetMethodCount(&methodcount);

    if (offset < methodcount) {
        info->GetMethodInfo(offset, &mi);
    }
    else {
        cerr << "Offset " << offset << " should be < " << methodcount << endl;
        return -1;
    }

    if (NS_FAILED(res)) {
        return -1;
    }

    // Make sure the array is large enough, and alloc more if 
    // necessary.
    int paramcount = mi->GetParamCount();

    if (paramcount > arraysize || *_retval == NULL) {
	*_retval = (nsXPTCVariant *)
	    nsAllocator::Alloc(sizeof(nsXPTCVariant) * paramcount);
    }

    // Set flags, pointers, and types for array elements
    nsXPTCVariant *current = *_retval;

    for (int i = 0; i < paramcount; i++) {
        nsXPTParamInfo param = mi->GetParam(i);
        nsXPTType type = param.GetType();

        current->type  = type.TagPart();
        current->flags = 0;

        if (!param.IsOut()) {
            current->flags = 0;
            current->ptr = 0;
        }
        else {
            // PTR_IS_DATA: ptr points to 'real' data in val
            current->flags = nsXPTCVariant::PTR_IS_DATA;

            if (type.IsPointer()) {
                current->val.p = NULL;
                current->ptr = &current->val.p;
                // PENDING: does this routine also alloc memory?
                // Or do we treat pointers to basic types like "out" 
                // parameters?
                // What about an out parameter that is a 
            }
            else {
                // PENDING: Will this work on all platforms?
                current->ptr = &current->val;
            }
        }
        current++;
    }
    return paramcount;
}

static jboolean JObjectToVariant(JNIEnv *env, 
				 nsXPTCVariant *current, 
				 const jobject elem,
				 REFNSIID iid) {
    jboolean result = JNI_FALSE;
    /*
     * Match:
     *     T_BOOL : Boolean
     *     T_I8: Byte
     *     T_I16: Short
     *     T_I32: Integer
     *     T_I64: Long
     *         -- we may want to support widening casts
     *     T_U<n>: T_I<n> type
     *     T_FLOAT: Float
     *     T_DOUBLE: Double (or Float?)
     *     T_CHAR: Character if value is an ASCII value
     *     T_WCHAR: Character
     *     T_CHAR_STRING: String as UTF-8 chars
     *     T_WCHAR_STRING: String as Unicode chars
     *     T_BSTR: String as UTF-8 chars
     *     T_INTERFACE(_IS): XPCOM pointer for an ComObject, if nsID matches
     */

    // Integer code assumes that current->val.i# == current->val.u#
    // for assignment purposes
    switch(current->type) {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
        if (env->IsInstanceOf(elem, classByte)) {
            current->val.i8 = 
                env->GetByteField(elem, Byte_value_ID);
            result = JNI_TRUE;
        }
        break;
    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
        if (env->IsInstanceOf(elem, classShort)) {
            current->val.i16 = 
                env->GetShortField(elem, Short_value_ID);
            result = JNI_TRUE;
        }
        break;
    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
        if (env->IsInstanceOf(elem, classInteger)) {
            current->val.i32 = 
                env->GetShortField(elem, Integer_value_ID);
            result = JNI_TRUE;
        }
        break;
    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
        if (env->IsInstanceOf(elem, classLong)) {
            current->val.i64 = 
                env->GetShortField(elem, Long_value_ID);
            result = JNI_TRUE;
        }
        break;
    case nsXPTType::T_FLOAT:
        if (env->IsInstanceOf(elem, classFloat)) {
            current->val.f = 
                env->GetFloatField(elem, Float_value_ID);
            result = JNI_TRUE;
        }
        break;
    case nsXPTType::T_DOUBLE:
        if (env->IsInstanceOf(elem, classDouble)) {
            current->val.d = 
                env->GetDoubleField(elem, Double_value_ID);
            result = JNI_TRUE;
        }
        break;
    case nsXPTType::T_BOOL:
        if (env->IsInstanceOf(elem, classBoolean)) {
            current->val.b = 
                env->GetBooleanField(elem, Boolean_value_ID);
            result = JNI_TRUE;
        }
        break;
    case nsXPTType::T_CHAR:
        if (env->IsInstanceOf(elem, classCharacter)) {
            current->val.c = 
                (char)env->GetCharField(elem, Character_value_ID);
            result = JNI_TRUE;
        }
        break;
    case nsXPTType::T_WCHAR:
        if (env->IsInstanceOf(elem, classCharacter)) {
            current->val.wc = 
                env->GetCharField(elem, Character_value_ID);
            result = JNI_TRUE;
        }
        break;
    case nsXPTType::T_CHAR_STR:
        if (env->IsInstanceOf(elem, classString)) {
            jstring string = (jstring)elem;
            jsize jstrlen = env->GetStringUTFLength(string);
            const char *utf = env->GetStringUTFChars(string, NULL);

            // PENDING: copying every time is wasteful, but
            // we need to release `utf' before invocation loses it.
            char *tmpstr = new char[jstrlen + 1];
            strncpy(tmpstr, utf, jstrlen);
            tmpstr[jstrlen] = '\0';

            current->val.p = tmpstr;
            current->flags |= nsXPTCVariant::VAL_IS_OWNED;

            env->ReleaseStringUTFChars(string, utf);
            result = JNI_TRUE;
        }
        break;
    case nsXPTType::T_WCHAR_STR:
        if (env->IsInstanceOf(elem, classString)) {
            jstring string = (jstring)elem;
            jsize jstrlen = env->GetStringLength(string);
            const jchar *wstr = env->GetStringChars(string, NULL);

            // PENDING: copying every time is wasteful, but
            // we need to release `wstr' before invocation loses it.
            PRUnichar *tmpstr = new PRUnichar[jstrlen + 1];
            memcpy(tmpstr, wstr, jstrlen * sizeof(PRUnichar));
            tmpstr[jstrlen] = '\0';

            current->val.p = tmpstr;

            env->ReleaseStringChars(string, wstr);
            result = JNI_TRUE;
        }
        break;
    case nsXPTType::T_VOID:
        // PENDING: handle void ptrs
        break;
    case nsXPTType::T_IID:
        // PENDING: unwrap the ID
        break;
    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
	if (elem == NULL) {
	    current->val.p = NULL;
	}
	else {
	    xpjp_QueryInterfaceToXPCOM(env, elem, iid, &(current->val.p));
	}
	current->flags |= nsXPTCVariant::VAL_IS_IFACE;
        break;
    case nsXPTType::T_BSTR:
        // Ignore for now
        break;
    }
    // VAL_IS_OWNED: val.p holds alloc'd ptr that must be freed
    // VAL_IS_IFACE: val.p holds interface ptr that must be released
    return result;
}

static nsresult JArrayToVariant(JNIEnv *env, 
				int paramcount,
				nsXPTCVariant *params, 
				const jobjectArray jarray) {
    nsXPTCVariant *current = params;

    for (jsize i = 0; i < paramcount; i++, current++) {
        jobject elem = env->GetObjectArrayElement(jarray, i);
	nsIID iid = NS_GET_IID(nsISupports);
	// PENDING: get the iid of the object

        if (elem == NULL) {
            continue;
        }

        if (JObjectToVariant(env, current, elem, iid) == JNI_FALSE) {
            // PENDING: throw an exception
            cerr << "Argument " << i << " is not of the correct type" << endl;
        }
    }
    return NS_OK;
}

       
static jobject VariantToJObject(JNIEnv *env, const nsXPTCVariant *current) {
    jobject result = NULL;

    // Integer code assumes that current->val.i# == current->val.u#
    // for assignment purposes
    switch(current->type) {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
        result =
            env->NewObject(classByte, 
                           Byte_init_ID, 
                           (jbyte)current->val.i16);
        break;
    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
        result =
            env->NewObject(classShort, 
                           Short_init_ID, 
                           (jshort)current->val.i16);
        break;
    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
        result =
            env->NewObject(classInteger, 
                           Integer_init_ID, 
                           (jint)current->val.i32);
        break;
    case nsXPTType::T_I64:
        result =
            env->NewObject(classLong, 
                           Long_init_ID, 
                           (jlong)current->val.i64);
        break;
    case nsXPTType::T_FLOAT:
        result =
            env->NewObject(classFloat, 
                           Float_init_ID, 
                           (jfloat)current->val.f);
        break;
    case nsXPTType::T_DOUBLE:
        result =
            env->NewObject(classDouble, 
                           Double_init_ID, 
                           (jdouble)current->val.d);
        break;
    case nsXPTType::T_BOOL:
        result =
            env->NewObject(classBoolean, 
                           Boolean_init_ID, 
                           (jboolean)current->val.b);
        break;
    case nsXPTType::T_CHAR:
        result =
            env->NewObject(classCharacter, 
                           Character_init_ID, 
                           (jchar)current->val.c);
        break;
    case nsXPTType::T_WCHAR:
        result =
            env->NewObject(classCharacter, 
                           Character_init_ID, 
                           (jchar)current->val.wc);
        break;
    case nsXPTType::T_CHAR_STR:
        if (current->val.p != NULL) {
            result = env->NewStringUTF((const char *)current->val.p);
        }
        break;
    case nsXPTType::T_WCHAR_STR:
        if (current->val.p != NULL) {
            jsize len = 0;
            const jchar *wstr = (const jchar *)current->val.p;
            // PENDING: is this right?
            while (wstr[len] != 0) len++;
            result = env->NewString(wstr, len);
        }
        break;
    case nsXPTType::T_VOID:
        // Ignore for now
        break;
    case nsXPTType::T_IID:
        // Ignore for now
        if (current->val.p != NULL) {
            result = ID_NewJavaID(env, (const nsID *)current->val.p);
        }
        break;
    case nsXPTType::T_BSTR:
        // Ignore for now
        break;
    case nsXPTType::T_INTERFACE:
        // Ignore for now
        break;
    case nsXPTType::T_INTERFACE_IS:
        // Ignore for now
        break;
    }

    return result;
}


static nsresult VariantToJArray(JNIEnv *env, 
				jobjectArray jarray,
				int paramcount,
				nsXPTCVariant *params) {
    nsXPTCVariant *current = params;

    for (jsize i = 0; i < paramcount; i++, current++) {
        jobject elem = NULL; // env->GetObjectArrayElement(jarray, i);
        jboolean isequal = JNI_FALSE;
        nsXPTCVariant currValue;

        if (!(current->flags & nsXPTCVariant::PTR_IS_DATA)) {
            continue;
        }

        if (elem != NULL) {
	    nsIID iid = NS_GET_IID(nsISupports);
	    // PENDING: get the iid of the object

            memcpy(&currValue, current, sizeof(nsXPTCVariant));
            if (JObjectToVariant(env, &currValue, elem, iid) != JNI_FALSE) {
                isequal = 
                    (memcmp(&currValue, current, sizeof(nsXPTCVariant)) != 0);
            }
        }

        if (isequal) {
            // PENDING: what about casting to more specific interfaces?
            continue;
        }

        elem = VariantToJObject(env, current);

        env->SetObjectArrayElement(jarray, i, elem);

        if (current->flags & nsXPTCVariant::VAL_IS_IFACE) {
            xpjp_SafeRelease((nsISupports*)current->val.p);
        }

        if (current->flags & nsXPTCVariant::VAL_IS_OWNED) {
            delete [] current->val.p;
            current->val.p = 0;  // for cleanliness sake
        }
    }
    return NS_OK;
}

/* ---------- DISPATCH API FUNCTIONS ------------ */

jboolean xpjd_InitJavaCaches(JNIEnv *env) {

    if (cache_initialized) {
	return JNI_TRUE;
    }

    // For basic types

    classString = env->FindClass("java/lang/String");
    if (classString == NULL) return JNI_FALSE;
    classString = (jclass)env->NewGlobalRef(classString);
    if (classString == NULL) return JNI_FALSE;

    classByte = env->FindClass("java/lang/Byte");
    if (classByte == NULL) return JNI_FALSE;
    classByte = (jclass)env->NewGlobalRef(classByte);
    if (classByte == NULL) return JNI_FALSE;

    classShort = env->FindClass("java/lang/Short");
    if (classShort == NULL) return JNI_FALSE;
    classShort = (jclass)env->NewGlobalRef(classShort);
    if (classShort == NULL) return JNI_FALSE;

    classInteger = env->FindClass("java/lang/Integer");
    if (classInteger == NULL) return JNI_FALSE;
    classInteger = (jclass)env->NewGlobalRef(classInteger);
    if (classInteger == NULL) return JNI_FALSE;

    classLong = env->FindClass("java/lang/Long");
    if (classLong == NULL) return JNI_FALSE;
    classLong = (jclass)env->NewGlobalRef(classLong);
    if (classLong == NULL) return JNI_FALSE;

    classFloat = env->FindClass("java/lang/Float");
    if (classFloat == NULL) return JNI_FALSE;
    classFloat = (jclass)env->NewGlobalRef(classFloat);
    if (classFloat == NULL) return JNI_FALSE;

    classDouble = env->FindClass("java/lang/Double");
    if (classDouble == NULL) return JNI_FALSE;
    classDouble = (jclass)env->NewGlobalRef(classDouble);
    if (classDouble == NULL) return JNI_FALSE;

    classBoolean = env->FindClass("java/lang/Boolean");
    if (classBoolean == NULL) return JNI_FALSE;
    classBoolean = (jclass)env->NewGlobalRef(classBoolean);
    if (classBoolean == NULL) return JNI_FALSE;

    classCharacter = env->FindClass("java/lang/Character");
    if (classCharacter == NULL) return JNI_FALSE;
    classCharacter = (jclass)env->NewGlobalRef(classCharacter);
    if (classCharacter == NULL) return JNI_FALSE;

    Byte_init_ID = env->GetMethodID(classByte, "<init>", "(B)V");
    if (Byte_init_ID == NULL) return JNI_FALSE;

    Byte_value_ID = env->GetFieldID(classByte, "value", "B");
    if (Byte_value_ID == NULL) return JNI_FALSE;

    Short_init_ID = env->GetMethodID(classShort, "<init>", "(S)V");
    if (Short_init_ID == NULL) return JNI_FALSE;

    Short_value_ID = env->GetFieldID(classShort, "value", "S");
    if (Short_value_ID == NULL) return JNI_FALSE;

    Integer_init_ID = env->GetMethodID(classInteger, "<init>", "(I)V");
    if (Integer_init_ID == NULL) return JNI_FALSE;

    Integer_value_ID = env->GetFieldID(classInteger, "value", "I");
    if (Integer_value_ID == NULL) return JNI_FALSE;

    Long_init_ID = env->GetMethodID(classLong, "<init>", "(J)V");
    if (Long_init_ID == NULL) return JNI_FALSE;

    Long_value_ID = env->GetFieldID(classLong, "value", "J");
    if (Long_value_ID == NULL) return JNI_FALSE;

    Float_init_ID = env->GetMethodID(classFloat, "<init>", "(F)V");
    if (Float_init_ID == NULL) return JNI_FALSE;

    Float_value_ID = env->GetFieldID(classFloat, "value", "F");
    if (Float_value_ID == NULL) return JNI_FALSE;

    Double_init_ID = env->GetMethodID(classDouble, "<init>", "(D)V");
    if (Double_init_ID == NULL) return JNI_FALSE;

    Double_value_ID = env->GetFieldID(classDouble, "value", "D");
    if (Double_value_ID == NULL) return JNI_FALSE;

    Boolean_init_ID = env->GetMethodID(classBoolean, "<init>", "(Z)V");
    if (Boolean_init_ID == NULL) return JNI_FALSE;

    Boolean_value_ID = env->GetFieldID(classBoolean, "value", "Z");
    if (Boolean_value_ID == NULL) return JNI_FALSE;

    Character_init_ID = env->GetMethodID(classCharacter, "<init>", "(C)V");
    if (Integer_init_ID == NULL) return JNI_FALSE;

    Character_value_ID = env->GetFieldID(classCharacter, "value", "C");
    if (Character_value_ID == NULL) return JNI_FALSE;

    // Get InterfaceInfoManager

#ifdef DEBUG_frankm
    cerr << "Getting InterfaceInfoManager" << endl;
#endif

    interfaceInfoManager = XPTI_GetInterfaceInfoManager();

    if (!interfaceInfoManager) {
	// XXX: throw exception or something
        cerr << "Failed to find InterfaceInfoManager" << endl;
        return JNI_FALSE;
    }

#ifdef DEBUG_frankm
    cerr << "InterfaceInfoManager found!" << endl;
#endif

    cache_initialized = JNI_TRUE;

    return cache_initialized;
}

void xpjd_FlushJavaCaches(JNIEnv *env) {

    cache_initialized = JNI_FALSE;

    // For basic types

    env->DeleteGlobalRef(classString);
    classString = NULL;

    env->DeleteGlobalRef(classByte);
    classByte = NULL;

    env->DeleteGlobalRef(classShort);
    classShort = NULL;

    env->DeleteGlobalRef(classInteger);
    classInteger = NULL;

    env->DeleteGlobalRef(classLong);
    classLong = NULL;

    env->DeleteGlobalRef(classFloat);
    classFloat = NULL;

    env->DeleteGlobalRef(classDouble);
    classDouble = NULL;

    env->DeleteGlobalRef(classBoolean);
    classBoolean = NULL;

    env->DeleteGlobalRef(classCharacter);
    classCharacter = NULL;

    Byte_init_ID = NULL;
    Byte_value_ID = NULL;
    Short_init_ID = NULL;
    Short_value_ID = NULL;
    Integer_init_ID = NULL;
    Integer_value_ID = NULL;
    Long_init_ID = NULL;
    Long_value_ID = NULL;
    Float_init_ID = NULL;
    Float_value_ID = NULL;
    Double_init_ID = NULL;
    Double_value_ID = NULL;
    Boolean_init_ID = NULL;
    Boolean_value_ID = NULL;
    Character_init_ID = NULL;
    Character_value_ID = NULL;
}

jboolean xpjd_GetInterfaceInfo(JNIEnv *env, 
			      jobject iid,
			      nsIInterfaceInfo **info) {
    nsID *nativeIID = ID_GetNative(env, iid);

    return xpjd_GetInterfaceInfoNative(*nativeIID, info);
}

jboolean xpjd_GetInterfaceInfoNative(REFNSIID iid,
				     nsIInterfaceInfo **info) {
    nsresult res;

    // Get info
    *info = nsnull;

    res = interfaceInfoManager->GetInfoForIID(&iid, info);

    if (NS_FAILED(res)) {
        cerr << "Failed to find info for " << iid.ToString() << endl;
        return res;
    }
}

void xpjd_InvokeMethod(JNIEnv *env, 
		       nsISupports *target,
		       nsIInterfaceInfo *info,
		       jint methodIndex,
		       jobjectArray jargs) {

    nsresult res = NS_OK;
    nsXPTCVariant paramArray[32];
    nsXPTCVariant *params = paramArray;
    int nparams = 0;
    const int capacity = sizeof(paramArray)/sizeof(paramArray[0]);

    // XXX: check info != NULL, target != null, jargs != null, env != null

    // XXX: check bounds of methodIndex

    nparams = xpjd_BuildParamsForOffset(info, 
					methodIndex, 
					capacity,
					&params);
    if (nparams < 0) {
	// PENDING: throw an exception
	cerr << "Couldn't initialize parameter array" << endl;
	goto finally;
    }

    res = JArrayToVariant(env, nparams, params, jargs);
    if (NS_FAILED(res)) {
	// PENDING: throw an exception
	cerr << "Array and parameter list mismatch" << endl;
	goto finally;
    }

    //xpjp_SafeAddRef(target);

    res = XPTC_InvokeByIndex(target, methodIndex, nparams, params);

    //xpjp_SafeRelease(target);

    if (NS_FAILED(res)) {
	// PENDING: throw an exception
	cerr << "Invocation failed, status: " << res << endl;
	goto finally;
    }

    res = VariantToJArray(env, jargs, nparams, params);
    if (NS_FAILED(res)) {
	// PENDING: throw an exception
	cerr << "Array and parameter list mismatch" << endl;
	goto finally;
    }

finally:
    if (params != paramArray) {
	nsAllocator::Free(params);
    }
}


