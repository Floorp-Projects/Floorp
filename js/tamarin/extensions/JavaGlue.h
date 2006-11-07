/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 1993-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */


#ifndef _JAVA_GLUE_H_
#define _JAVA_GLUE_H_

#ifdef AVMPLUS_WITH_JNI

#define NO_JNI_STDIO
#include <jni.h>

/**
 * Pointers to the routines that we will be calling.  We bind
 * dynamically since we don't want to get a can't find dll error
 * if we failed to have a vm on this machine.
 */
typedef 
_JNI_IMPORT_OR_EXPORT_ jint (JNICALL 
*_JNI_GetDefaultJavaVMInitArgs) (void *args);

typedef 
_JNI_IMPORT_OR_EXPORT_ jint (JNICALL
*_JNI_CreateJavaVM) (JavaVM **pvm, void **penv, void *args);

#define JNI_NO_LIB		(-8)	/* shared library not found */
#define JNI_BAD_VER		(-9)	/* vm version is less than what is required */
#define JNI_VM_FAIL		(-10)	/* vm creation failure */
#define JNI_INIT_FAIL	(-11)	/* our initialization did not occur correctly */

namespace avmplus
{
	class JObject;
	class JClass;

	class JObjectClass: public ClassClosure
	{
	public:
		static JObjectClass* cc; //@todo hack to remove

		JObjectClass(VTable *cvtable);
		ScriptObject* JObjectClass::createInstance(VTable *ivtable, ScriptObject *prototype);

		/**
		 * Implementations of AS JObject.xxx() methods
		 */
		JObject* create(String* name, Atom* argv, int argc);
		JObject* createArray(JObject* arrayType, int size, ArrayObject* arr);
		ArrayObject* toArray(JObject* jobj);
		String*	 constructorSignature(String* name, Atom* argv, int argc);
		String*	 methodSignature(JObject* jobj, String* name, Atom* argv, int argc);
		String*	 fieldSignature(JObject* jobj, String* name);

		// JObject that has a java class tied to it but no java instance object
		JObject* createJObjectShell(JClass* clazz);

		// helper method for finding the JClass associated with a particular java class(array)
		JClass* forName(String* name);
		JClass* forType(jstring type);

		DECLARE_NATIVE_MAP(JObjectClass)

		static void throwException(Java* j, Toplevel* top, int errorId, jthrowable jthrow, String* arg1=0, String* arg2=0);

	private:
		Java*	jvm();
	};

	class JObject : public ScriptObject
	{
	public:
		JObject(VTable *vtable, ScriptObject *proto);
		~JObject();

		ScriptObject* createInstance(VTable *ivtable, ScriptObject *prototype);

		Atom getProperty(Atom name) const;
		bool hasProperty(Multiname* multi) const;
		Atom callProperty(Multiname* multiname, int argc, Atom* argv);
		void setProperty(Multiname* name, Atom value);

		// internal 
		void setClass(JClass* clazz) { this->jclass = clazz; }
		void setObject(jobject obj)  { this->obj = obj; }
		bool indexFrom(AvmCore* core, Atom a, int* index) const;

		JClass* getClass()			{ return jclass; }
		jobject getObject()			{ return obj; }

		String* _toString() const;

		// AS exposed functions

	private:
		jobject		 obj;		/* underlying java object */
		DWB(JClass*) jclass;	/* class of this object */
	};

	// non-AS exposed wrapper around a jvm Class object
	class JClass : public MMgc::GCObject
	{
	public:
		JClass::JClass(Java* vm, String* name, jclass cref);

		jobject		callConstructor(JObjectClass* jobj, int argc, Atom* argv);
		jarray		createArray(JObjectClass* jobj, int size, ArrayObject* arr);
		Atom		callMethod(JObject* jobj, String* name, int argc, Atom* argv);
		Atom		getField(JObject* jobj, String* name);
		bool		setField(JObject* jobj, String* name, Atom val);
		bool		hasField(JObject* jobj, String* nm);
		Atom		getArrayElement(JObject* jobj, int index);
		void		setArrayElement(JObject* jobj, int index, Atom val);
		const char*	boxArgs(AvmCore* core, Toplevel* toplevel, const char* descriptor, int argc, Atom* argv, jvalue* jargs);

		jobject		methodFor(String* name, const char* argvDesc, int argc, char* methodDesc);
		jobject		constructorFor(const char* argvDesc, int argc, char* constrDesc);
		jobject		fieldFor(String* name);
		int			methodDescriptor(char* desc, jobject method);
		int			constructorDescriptor(char* desc, jobject constr);
		int			argsDescriptor(AvmCore* core, char* desc, int argc, Atom* argv);
		char*		descriptorVerbose(char* desc, jclass cls);
		char		descriptorChar(jclass cls);
		bool		stringsEqual(jstring s1, String* s2);
		bool		isStatic(jint modifiers);

		static const char*	compareDescriptors(const char* base, const char* desc1, const char* desc2);
		static int			argumentCount(const char* d);
		static int			conversionCost(const char* from, const char* to);

		// conversions methods 
		const char*		jvalueToAtom(AvmCore* core, Toplevel* toplevel, jvalue& val, const char* type, Atom& a);
        const char*		atomTojvalue(AvmCore* core, Toplevel* toplevel, Atom a, const char* type, jvalue& val);
		void			atomTojlong(AvmCore* core, Atom a, jlong& val);

        static bool		isJObject(AvmCore* core, Atom a);
		static String*	jlongToString(AvmCore* core, jlong& val);

		Java*	jvm()		{ return vm; }
		String* name()		{ return nm; }
		jclass	classRef()	{ return cref; }

	private:
		DWB(Java*)		vm;
		DRCWB(String*)	nm;			/* name used for class lookup */
		jclass			cref;		/* class */
	};

// auto class determination
#define BEGIN_DEFINE_JCLASS()			static const int _classRef_0_= __LINE__ + 1; 
#define DEFINE_JCLASS(c) 				static const int _classRef_##c = __LINE__ - _classRef_0_; jclass c() const { return classRefTable[_classRef_##c]; }
#define END_DEFINE_JCLASS() 			jclass classRefTable[__LINE__-_classRef_0_];
#define INIT_JCLASS(c)					classRefTable[_classRef_##c] = jni->FindClass( replace(dst, #c) );

// instances of Class objects
#define BEGIN_DEFINE_PRIMITIVE_TYPE()	static const int _primitiveRef_0_= __LINE__ + 1; 
#define DEFINE_PRIMITIVE_TYPE(c)		static const int _primitiveRef_##c = __LINE__ - _primitiveRef_0_; jclass c##_class() const { return primitiveRefTable[_primitiveRef_##c]; }
#define END_DEFINE_PRIMITIVE_TYPE() 	jclass primitiveRefTable[__LINE__-_primitiveRef_0_];
#define INIT_PRIMITIVE_TYPE(c)			primitiveRefTable[_primitiveRef_##c] = (jclass)jni->GetStaticObjectField( c(), jni->GetStaticFieldID( c(), "TYPE", "Ljava/lang/Class;") );

// auto method id determination
#define BEGIN_DEFINE_JMETHODID()		static const int _methodId_0_= __LINE__ + 1; 
#define DEFINE_JMETHODID(c, m, s)		static const int _methodId_##c##m = __LINE__ - _methodId_0_; jmethodID c##_##m() const { return methodIdTable[_methodId_##c##m]; }
#define END_DEFINE_JMETHODID()			jmethodID methodIdTable[__LINE__-_methodId_0_];
#define INIT_JMETHODID(c, m, s)			methodIdTable[_methodId_##c##m] = jni->GetMethodID( c(), #m, s);
#define INIT_SJMETHODID(c, m, s)		methodIdTable[_methodId_##c##m] = jni->GetStaticMethodID( c(), #m, s);
#define DEFINE_SJMETHODID DEFINE_JMETHODID

	/**
	 * The container class for a Java Vm
	 */
	class Java
	{
	public:
		Java();
		~Java();

		String* newString(AvmCore* core, jstring jstr);

		static char* startup_options;  /* set by the shell */

		/* returns jni error code or JNI_OK */
		int startupJVM(AvmCore* core);

		JavaVM* jvm;       /* denotes a Java VM */
		JNIEnv* jni;       /* pointer to native method interface */
		bool	bound; 

		_JNI_GetDefaultJavaVMInitArgs	JNI_GetDefaultJavaVMInitArgs_;
		_JNI_CreateJavaVM				JNI_CreateJavaVM_;

		bool bindLibrary(AvmCore* core);

		// java.lang.reflect.Modifier consts
		int modifier_STATIC;
		int modifier_FINAL;

		BEGIN_DEFINE_JCLASS();
			DEFINE_JCLASS(java_lang_Byte);
			DEFINE_JCLASS(java_lang_Boolean);
			DEFINE_JCLASS(java_lang_Class);
			DEFINE_JCLASS(java_lang_Character);
			DEFINE_JCLASS(java_lang_Double);
			DEFINE_JCLASS(java_lang_Float);
			DEFINE_JCLASS(java_lang_Integer);
			DEFINE_JCLASS(java_lang_Long);
			DEFINE_JCLASS(java_lang_Object);
			DEFINE_JCLASS(java_lang_Short);
			DEFINE_JCLASS(java_lang_String);
			DEFINE_JCLASS(java_lang_Void);
			DEFINE_JCLASS(java_lang_reflect_Constructor);
			DEFINE_JCLASS(java_lang_reflect_Field);
			DEFINE_JCLASS(java_lang_reflect_Method);
			DEFINE_JCLASS(java_lang_reflect_Modifier);
		END_DEFINE_JCLASS();

		BEGIN_DEFINE_JMETHODID();
			DEFINE_JMETHODID(java_lang_Object, equals, "(Ljava/lang/Object;)Z"); 
			DEFINE_JMETHODID(java_lang_Object, toString, "()Ljava/lang/String;"); 
			DEFINE_JMETHODID(java_lang_Object, getClass, "()Ljava/lang/Class;"); 
			DEFINE_JMETHODID(java_lang_Class, getName, "()Ljava/lang/String;");
			DEFINE_JMETHODID(java_lang_Class, getCanonicalName, "()Ljava/lang/String;");
			DEFINE_JMETHODID(java_lang_Class, getMethods, "()[Ljava/lang/reflect/Method;");
			DEFINE_JMETHODID(java_lang_Class, getFields, "()[Ljava/lang/reflect/Field;");
			DEFINE_JMETHODID(java_lang_Class, getField, "(Ljava/lang/String;)Ljava/lang/reflect/Field;");
			DEFINE_JMETHODID(java_lang_Class, getConstructors, "()[Ljava/lang/reflect/Constructors;");
			DEFINE_JMETHODID(java_lang_Class, isPrimitive, "()Z"); 
			DEFINE_JMETHODID(java_lang_Class, isArray, "()Z"); 
			DEFINE_SJMETHODID(java_lang_Class, forName, "(Ljava/lang/String;)Ljava/lang/Class;");
			DEFINE_JMETHODID(java_lang_reflect_Method, getName, "()Ljava/lang/String;");
			DEFINE_JMETHODID(java_lang_reflect_Method, getReturnType, "()Ljava/lang/Class;"); 
			DEFINE_JMETHODID(java_lang_reflect_Method, getParameterTypes, "()[Ljava/lang/Class;");
			DEFINE_JMETHODID(java_lang_reflect_Method, getModifiers, "()I");
			DEFINE_JMETHODID(java_lang_reflect_Field, getName, "()Ljava/lang/String;");
			DEFINE_JMETHODID(java_lang_reflect_Field, getType, "()Ljava/lang/Class;");
			DEFINE_JMETHODID(java_lang_reflect_Field, getModifiers, "()I");
			DEFINE_JMETHODID(java_lang_reflect_Constructor, getParameterTypes, "()[Ljava/lang/Class;");
		END_DEFINE_JMETHODID();

		BEGIN_DEFINE_PRIMITIVE_TYPE();
			DEFINE_PRIMITIVE_TYPE(java_lang_Byte);
			DEFINE_PRIMITIVE_TYPE(java_lang_Boolean);
			DEFINE_PRIMITIVE_TYPE(java_lang_Class);
			DEFINE_PRIMITIVE_TYPE(java_lang_Character);
			DEFINE_PRIMITIVE_TYPE(java_lang_Double);
			DEFINE_PRIMITIVE_TYPE(java_lang_Float);
			DEFINE_PRIMITIVE_TYPE(java_lang_Integer);
			DEFINE_PRIMITIVE_TYPE(java_lang_Long);
			DEFINE_PRIMITIVE_TYPE(java_lang_Object);
			DEFINE_PRIMITIVE_TYPE(java_lang_Short);
			DEFINE_PRIMITIVE_TYPE(java_lang_Void);
		END_DEFINE_PRIMITIVE_TYPE();

	 	static char* replace(char* dst, const char* src, char what='_', char with='/');
	};
}

#endif /* AVMPLUS_WITH_JNI */

#endif /* _JAVA_GLUE_H_ */
