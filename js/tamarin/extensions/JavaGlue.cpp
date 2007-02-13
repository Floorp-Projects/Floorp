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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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

#include "avmplus.h"
#include "avmshell.h"
#include "JavaGlue.h"

#ifdef AVMPLUS_WITH_JNI

// @todo 
//  local ref and global refs not used at all!
//  support for arrays 
//  128 is a very nice number but its not the best way to pick how much room is needed for signatures etc.
// 

#ifdef WIN32
#include <tchar.h>
#endif

#ifdef AVMPLUS_MAC
#include <dlfcn.h>
#endif 

namespace avmplus
{
	BEGIN_NATIVE_MAP(JObjectClass)
		NATIVE_METHOD(avmplus_JObject_toString,				JObject::_toString)
		NATIVE_METHOD(avmplus_JObject_methodSignature,		JObjectClass::methodSignature)
		NATIVE_METHOD(avmplus_JObject_fieldSignature,		JObjectClass::fieldSignature)
		NATIVE_METHOD(avmplus_JObject_constructorSignature,	JObjectClass::constructorSignature)
		NATIVE_METHOD(avmplus_JObject_create,				JObjectClass::create)
		NATIVE_METHOD(avmplus_JObject_createArray,			JObjectClass::createArray)
		NATIVE_METHOD(avmplus_JObject_toArray,				JObjectClass::toArray)
	END_NATIVE_MAP()

	JObjectClass* JObjectClass::cc = 0; //@todo hack to remove

	JObjectClass::JObjectClass(VTable *cvtable)
	: ClassClosure(cvtable)
	{
		// @hack for now so we know our class closure
		cc = this;		
	}

	ScriptObject* JObjectClass::createInstance(VTable *ivtable, ScriptObject *prototype)
    {
        return new (core()->GetGC(), ivtable->getExtraSize()) JObject(ivtable, prototype);
    }

	String* JObjectClass::constructorSignature(String* name, Atom* argv, int argc)
    {
		AvmCore* core = this->core();
		String* s = core->kundefined;

		// get the class first
		JClass* clazz = forName(name);
		if (clazz)
		{
			// now see what constructor would be fired
			// some space for our descriptors 
			char descriptors[256];
			char *argvDesc = &descriptors[0];
			char *constrDesc = &descriptors[128];

			// create a description of our args so we can best match which constructor to call
			clazz->argsDescriptor(core, argvDesc, argc, argv);

			// get the constructor and call it
			jobject constr = clazz->constructorFor(argvDesc, argc, constrDesc);
			if (constr)
			{
				Java* jvm = clazz->jvm();
				jstring str = (jstring) jvm->jni->CallObjectMethod(constr, jvm->java_lang_Object_toString() ); 
				s = jvm->newString(core, str);
			}
		}
		AvmAssert( jvm()->jni->ExceptionOccurred() == 0);
		return s;
	}

	String* JObjectClass::methodSignature(JObject* jobj, String* name, Atom* argv, int argc)
    {
		AvmCore* core = jobj->core();
		String* s = core->kundefined;

		// now see what method would be fired
		JClass* clazz = jobj->getClass();

		// some space for our descriptors 
		char descriptors[256];
		char *argvDesc = &descriptors[0];
		char *methodDesc = &descriptors[128];

		// create a description of our args so we can best match which method to call
		clazz->argsDescriptor(core, argvDesc, argc, argv);

		// get the method and call it
		jobject method = clazz->methodFor(name, argvDesc, argc, methodDesc);
		if (method)
		{
			Java* jvm = clazz->jvm();
			jstring str = (jstring) jvm->jni->CallObjectMethod(method, jvm->java_lang_Object_toString() ); 
			s = jvm->newString(core, str);
		}
		AvmAssert( clazz->jvm()->jni->ExceptionOccurred() == 0);
		return s;
	}

	String* JObjectClass::fieldSignature(JObject* jobj, String* name)
    {
		AvmCore* core = jobj->core();
		String* s = core->kundefined;

		// now see what field would be accessed
		JClass* clazz = jobj->getClass();
		jobject field = clazz->fieldFor(name);
		if (field)
		{
			Java* jvm = clazz->jvm();
			jstring str = (jstring) jvm->jni->CallObjectMethod(field, jvm->java_lang_Object_toString() ); 
			s = jvm->newString(core, str);
		}
		AvmAssert( clazz->jvm()->jni->ExceptionOccurred() == 0);
		return s;
	}

	JClass* JObjectClass::forName(String* name)
	{
		JClass* c = 0;
		Java* j = jvm();
		JNIEnv* jni = j->jni;
		UTF8String* str = name->toUTF8String();
		char* nm = str->lockBuffer();
		Java::replace(nm, nm, '.');
		jobject it = jni->FindClass(nm);
		str->unlockBuffer();
		jthrowable jthrow = jni->ExceptionOccurred();
		if (jthrow)
			throwException(j, toplevel(), kClassNotFoundError, jthrow);
        else if (it)
			c = (JClass*) new (core()->GetGC()) JClass(j, name, (jclass)it);
		return c;
	}

	void JObjectClass::throwException(Java* j, Toplevel* top, int errorId, jthrowable jthrow, String* arg1, String* arg2)
	{
		JNIEnv* jni = j->jni;
		jni->ExceptionClear();
		jstring jstr = (jstring) jni->CallObjectMethod(jthrow, j->java_lang_Object_toString());
		String* str = j->newString(top->core(), jstr);
		top->errorClass()->throwError(errorId, str, arg1, arg2);
	}

	JClass* JObjectClass::forType(jstring type)
	{
		JClass* c = 0;
		Java* j = jvm();
		JNIEnv* jni = j->jni;

		jobject it = jni->CallStaticObjectMethod(j->java_lang_Class(), j->java_lang_Class_forName(), type);
		jthrowable jthrow = jni->ExceptionOccurred();
		if (jthrow)
		{
			throwException(j, toplevel(), kClassNotFoundError, jthrow);
		}
        else if (it)
		{
			String* stype = j->newString(core(), type);
			c = (JClass*) new (core()->GetGC()) JClass(j, stype, (jclass)it);
		}
		return c;
	}

	JObject* JObjectClass::create(String* name, Atom* argv, int argc)
	{
		// get the class first
		JClass* clazz = forName(name);
		if (!clazz) 
			toplevel()->throwError(kClassNotFoundError, name);

		// wrap it in a JObject
		JObject* obj = createJObjectShell(clazz);

		// now construct an instance of this class within the jvm 
		jobject jobj = clazz->callConstructor(this, argc, argv);
		obj->setObject(jobj);

		if (!jobj && argc)
		{
			// no constructor was called, so if we have no args then this is a purely static object
			toplevel()->throwError(kNotConstructorError, name);
		}
		AvmAssert( jvm()->jni->ExceptionOccurred() == 0);
		return obj;
	}

	JObject* JObjectClass::createArray(JObject* arrayType, int size, ArrayObject* arr)
	{
		Java*	vm = jvm();
		JNIEnv* jni = vm->jni;

		// arrayType should be a class object of the type of array we wish to create
		jobject aobj = arrayType->getObject();
		if (!aobj || jni->IsInstanceOf(aobj, vm->java_lang_Class()) != JNI_TRUE)
			toplevel()->throwError(kClassNotFoundError, core()->string(arrayType->toString()));

		// build a JClass for the entry 
		jstring type = (jstring) jni->CallObjectMethod( aobj, vm->java_lang_Class_getName() );
		JClass* clazz = forType( type );
		if (!clazz)
			toplevel()->throwError(kClassNotFoundError, core()->string(arrayType->toString()));

		// now construct an instance of this array 
		jarray jobj = clazz->createArray(this, size, arr);
		jthrowable jthrow = jni->ExceptionOccurred();
		if (jthrow)
			throwException(vm, toplevel(), kClassNotFoundError, jthrow);

		// wrap it in a JObject
		JObject* obj = createJObjectShell(clazz);
		obj->setObject(jobj);

		AvmAssert( jvm()->jni->ExceptionOccurred() == 0);
		return obj;
	}

	ArrayObject* JObjectClass::toArray(JObject* jobj)
	{
		return 0;
	}

	JObject* JObjectClass::createJObjectShell(JClass* clazz)
	{
		if (!clazz)
			return 0;

		// now let's create an AS object using this class as a template
		Atom args[1] = { nullObjectAtom };
		JObject* obj = (JObject*) AvmCore::atomToScriptObject( construct(0,args) );
		obj->setClass(clazz);
		AvmAssert( jvm()->jni->ExceptionOccurred() == 0);
		return obj;
	}

	Java* JObjectClass::jvm() 
	{
		// make sure a VM is available from core
		AvmCore* core = this->core();
		Java* vm = core->java;
		if (!vm)
		{
			vm = new (core->GetGC()) Java();
			if (vm->startupJVM(core) == JNI_OK)
				core->java = vm;
			else
				toplevel()->throwError( kFileOpenError, core->newString("Java Virtual Machine - Runtime System") );
		}
		return core->java;
	}

	//
	// JObject
	//
	JObject::JObject(VTable *vtable, ScriptObject *proto) 
		: ScriptObject(vtable, proto)
	{
	}
	
	JObject::~JObject() {}

	ScriptObject* JObject::createInstance(VTable *ivtable, ScriptObject *prototype)
    {
        return new (core()->GetGC(), ivtable->getExtraSize()) JObject(ivtable, prototype);
    }

// for property assignment
//	void ScriptObject::setMultinameProperty(Multiname* name, Atom value)

// get property 
	Atom JObject::getAtomProperty(Atom name) const
	{
		Java*	vm = jclass->jvm();
		JNIEnv* jni = vm->jni;

		// is this an array access
		Atom a = nullObjectAtom;
		AvmCore* core = this->core();
		if ( (jni->CallBooleanMethod(jclass->classRef(), vm->java_lang_Class_isArray()) == JNI_TRUE) )
		{
			int index = -1;
			if (indexFrom(core, name, &index))
				a = jclass->getArrayElement( (JObject*)this, index );
			else if (core->isString(name))
			{
				String* s = core->string(name);
				if ( s->Equals(core->klength->c_str(), core->klength->length()) )
					a = core->intToAtom( jni->GetArrayLength((jarray)obj) );
				else
					toplevel()->throwError(kReadSealedError, core->atomToString(name), jclass->name() );
			}
			else
				toplevel()->throwError(kReadSealedError, core->atomToString(name), jclass->name() );
		}
		else
		{
			String* nm = core->atomToString(name);
			a = jclass->getField( (JObject*)this, nm);
		}
		AvmAssert( jclass->jvm()->jni->ExceptionOccurred() == 0);
		return a;
	}

	bool JObject::indexFrom(AvmCore* core, Atom a, int* index) const
	{
		bool ok = false;
		if (core->isInteger(a))
		{
			*index = core->integer(a);
			ok = true;
		}
		else if (core->isString(a))
		{
			double d = core->string(a)->toNumber();
			if (!MathUtils::isNaN(d))
			{
				*index = (int)d;
				ok = true;
			}
		}
		return ok; 
	}

	// for calls
	Atom JObject::callProperty(Multiname* multiname, int argc, Atom* argv) 
	{
		String* nm = multiname->getName();
		Atom a = jclass->callMethod(this, nm, argc, argv);
		jthrowable jthrow = jclass->jvm()->jni->ExceptionOccurred();
		if (jthrow)
		{
			Java*	vm = jclass->jvm();
			JNIEnv* jni = vm->jni;
			jstring str = (jstring) jni->CallObjectMethod(jthrow, jclass->jvm()->java_lang_Object_toString() ); 
			String* s = vm->newString(core(), str);
			toplevel()->errorClass()->throwError(kIllegalOpcodeError, nm, s, jclass->name());
		}
		return a;
	}

	bool JObject::hasMultinameProperty(Multiname* multi) const
	{
		String* nm = multi->getName();
		bool has = jclass->hasField( (JObject*)this, nm);
		AvmAssert( jclass->jvm()->jni->ExceptionOccurred() == 0);
		return has;
	}

	void JObject::setMultinameProperty(Multiname* multi, Atom value)
	{
		Java*	vm = jclass->jvm();
		JNIEnv* jni = vm->jni;
		String* name = multi->getName();

		// is this an array access
		AvmCore* core = this->core();
		int index = -1;
		if ( indexFrom(core, name->atom(), &index) && (jni->CallBooleanMethod(jclass->classRef(), vm->java_lang_Class_isArray()) == JNI_TRUE) )
			jclass->setArrayElement(this, index, value );
		else
			jclass->setField(this, name, value);
		AvmAssert( jclass->jvm()->jni->ExceptionOccurred() == 0);
	}

	String* JObject::_toString() const
	{
		AvmCore* core = this->core();
		String* s = 0;
		if (obj)
		{
			JNIEnv* jni = jclass->jvm()->jni;
			#ifdef WRAP_JTOSTRING
			s = core->newString("[JObject object='");
			#endif
			jstring str = (jstring) jni->CallObjectMethod(obj, jclass->jvm()->java_lang_Object_toString() ); 
			String* sraw = jclass->jvm()->newString(core, str);
			#ifdef WRAP_JTOSTRING
			s = core->concatStrings(s, sraw);
			s = core->concatStrings(s, core->newString("']"));
			#else
			s = sraw;
			#endif
		}
		else
		{
			s = core->newString("[JObject object='");
		}
		AvmAssert( jclass->jvm()->jni->ExceptionOccurred() == 0);
		return s;
	}

	// 
	// JClass
	//
	JClass::JClass(Java* j, String* name, jclass cls)
		: vm(j)
		, nm(name)
		, cref(cls)
	{
	}

	jarray JClass::createArray(JObjectClass* obj, int size, ArrayObject* fillWith)
	{
		JNIEnv* jni = vm->jni;

		// use the type character to tell us what kind of array this is 
		jarray arr = 0;
		char t = nm->c_str()[1];
		switch( t )
		{
			case 'Z':
				arr = jni->NewBooleanArray(size);
				break;

			case 'B':
				arr = jni->NewByteArray(size);
				break;

			case 'C':
				arr = jni->NewCharArray(size);
				break;

			case 'S':
				arr = jni->NewShortArray(size);
				break;

			case 'I':
				arr = jni->NewIntArray(size);
				break;

			case 'J':
				arr = jni->NewLongArray(size);
				break;

			case 'F':
				arr = jni->NewFloatArray(size);
				break;

			case 'D':
				arr = jni->NewDoubleArray(size);
				break;

			case 'L':
			case 'G':
			case '[':
				arr = jni->NewObjectArray(size, cref, 0);
				break;

			case 'V':
			default:
				AvmAssertMsg(0, "Wa! what kind of JNI type is this!");
		}
		return arr;
	}

	jobject JClass::callConstructor(JObjectClass* jobj, int argc, Atom* argv)
	{
		JNIEnv* jni = vm->jni;
		AvmCore* core = jobj->core();

		// some space for our descriptors 
		char descriptors[256];
		char *argvDesc = &descriptors[0];
		char *constrDesc = &descriptors[128];

		// create a description of our args so we can best match which constructor to call
		argsDescriptor(core, argvDesc, argc, argv);

		// get the constructor and call it
		jobject constr = constructorFor(argvDesc, argc, constrDesc);
		if (!constr)
			return 0;

		// package up the args
		int count = argumentCount(constrDesc);
		jvalue* jargs = (jvalue*) alloca( count*sizeof(jvalue) );
		boxArgs(core, jobj->toplevel(), constrDesc, argc, &argv[0], jargs);
		for(int i=argc; i<count; i++)
			memset(&jargs[i], 0, sizeof(jvalue));

		jmethodID mid = jni->FromReflectedMethod(constr);
		jobject obj = jni->NewObjectA(cref, mid, jargs);
		return obj;
	}

	Atom JClass::callMethod(JObject* jobj, String* name, int argc, Atom* argv)
	{
		JNIEnv* jni = vm->jni;
		AvmCore* core = jobj->core();

		// some space for our descriptors 
		char descriptors[256];
		char *argvDesc = &descriptors[0];
		char *methodDesc = &descriptors[128];

		// create a description of our args so we can best match which constructor to call
		argsDescriptor(core, argvDesc, argc, &argv[1]);

		// get the method and call it
		jobject method = methodFor(name, argvDesc, argc, methodDesc);
		if (!method)
		{
			jobj->toplevel()->throwError(kNotImplementedError, name);
		}

		// package up the args
		int count = argumentCount(methodDesc);
		jvalue* jargs = (jvalue*) alloca( count*sizeof(jvalue) );
		const char* rt = boxArgs(core, jobj->toplevel(), methodDesc, argc, &argv[1], jargs);
		for(int i=argc; i<count; i++)
			memset(&jargs[i], 0, sizeof(jvalue));

		// extract return type from methodDesc
		while(*rt++ != ')')
			;

		// now static or no?
		jint mod = jni->CallIntMethod(method, vm->java_lang_reflect_Method_getModifiers() );
		bool is = isStatic(mod);

		// now make the call
		jvalue r;
		jmethodID mid = jni->FromReflectedMethod(method);
		jobject obj = jobj->getObject();
		if (!obj && !is)  
			jobj->toplevel()->throwError(kNotImplementedError, name);

		switch( *rt )
		{
			case 'V':
				(is) ? jni->CallStaticVoidMethodA(cref, mid, jargs) : jni->CallVoidMethodA(obj, mid, jargs);
				break;

			case 'Z':
				r.z = (is) ? jni->CallStaticBooleanMethodA(cref, mid, jargs) : r.z = jni->CallBooleanMethodA(obj, mid, jargs);
				break;

			case 'B':
				r.b = (is) ? jni->CallStaticByteMethodA(cref, mid, jargs) : r.b = jni->CallByteMethodA(obj, mid, jargs);
				break;

			case 'C':
				r.c = (is) ? jni->CallStaticCharMethodA(cref, mid, jargs) : r.c = jni->CallCharMethodA(obj, mid, jargs);
				break;

			case 'S':
				r.s = (is) ? jni->CallStaticShortMethodA(cref, mid, jargs) : r.s = jni->CallShortMethodA(obj, mid, jargs);
				break;

			case 'I':
				r.i = (is) ? jni->CallStaticIntMethodA(cref, mid, jargs) : r.i = jni->CallIntMethodA(obj, mid, jargs);
				break;

			case 'J':
				r.j = (is) ? jni->CallStaticLongMethodA(cref, mid, jargs) : r.j = jni->CallLongMethodA(obj, mid, jargs);
				break;

			case 'F':
				r.f = (is) ? jni->CallStaticFloatMethodA(cref, mid, jargs) : r.f = jni->CallFloatMethodA(obj, mid, jargs);
				break;

			case 'D':
				r.d = (is) ? jni->CallStaticDoubleMethodA(cref, mid, jargs) : r.d = jni->CallDoubleMethodA(obj, mid, jargs);
				break;

			case 'L':
			case 'G':
			case '[':
				r.l = (is) ? jni->CallStaticObjectMethodA(cref, mid, jargs) : r.l = jni->CallObjectMethodA(obj, mid, jargs);
				break;

			default:
				AvmAssertMsg(0, "Wa! what kind of JNI type is this!");
		}

		// coerce type into an AS representation
		Atom a = undefinedAtom;
		jvalueToAtom(core, jobj->toplevel(), r, rt, a);
		return a;
	}

	bool JClass::hasField(JObject* jobj, String* nm)
	{
		jobject field = fieldFor(nm);
		return (field == 0) ? false : true;
	}

	Atom JClass::getField(JObject* jobj, String* nm)
	{
		JNIEnv* jni = vm->jni;
		AvmCore* core = jobj->core();
		char fieldDesc[128];
		
		jobject field = fieldFor(nm);
		if (!field)
		{
			jobj->toplevel()->throwError(kReadSealedError, nm, name() );
		}

		// get the descriptor for the field
		jclass cls = (jclass) jni->CallObjectMethod(field, vm->java_lang_reflect_Field_getType() );
		descriptorVerbose(fieldDesc, cls);

		// now static or no?
		jint mod = jni->CallIntMethod(field, vm->java_lang_reflect_Field_getModifiers() );
		bool is = isStatic(mod);

		// now make the call if we can
		jvalue r;
		jfieldID fid = jni->FromReflectedField(field);
		jobject obj = jobj->getObject();
		if (!obj && !is)  
			jobj->toplevel()->throwError(kReadSealedError, nm, name() );

		switch( *fieldDesc )
		{
			case 'Z':
				r.z = (is) ?  jni->GetStaticBooleanField(cref, fid) : jni->GetBooleanField(obj, fid);
				break;

			case 'B':
				r.b = (is) ?  jni->GetStaticByteField(cref, fid) : jni->GetByteField(obj, fid);
				break;

			case 'C':
				r.c = (is) ?  jni->GetStaticCharField(cref, fid) : jni->GetCharField(obj, fid);
				break;

			case 'S':
				r.s = (is) ?  jni->GetStaticShortField(cref, fid) : jni->GetShortField(obj, fid);
				break;

			case 'I':
				r.i = (is) ?  jni->GetStaticIntField(cref, fid) : jni->GetIntField(obj, fid);
				break;

			case 'J':
				r.j = (is) ?  jni->GetStaticLongField(cref, fid) : jni->GetLongField(obj, fid);
				break;

			case 'F':
				r.f = (is) ?  jni->GetStaticFloatField(cref, fid) : jni->GetFloatField(obj, fid);
				break;

			case 'D':
				r.d = (is) ?  jni->GetStaticDoubleField(cref, fid) : jni->GetDoubleField(obj, fid);
				break;

			case 'L':
			case 'G':
			case '[':
				r.l = (is) ?  jni->GetStaticObjectField(cref, fid) : jni->GetObjectField(obj, fid);
				break;

			case 'V':
			default:
				AvmAssertMsg(0, "Wa! what kind of JNI type is this!");
		}

		// coerce type into an AS representation
		Atom a = undefinedAtom;

		//@todo rt is only 1 char! make sure this call doesn't do anything with it 
		jvalueToAtom(core, jobj->toplevel(), r, fieldDesc, a);
		return a;
	}

	bool JClass::setField(JObject* jobj, String* nm, Atom val)
	{
		JNIEnv* jni = vm->jni;
		AvmCore* core = jobj->core();
		char fieldDesc[128];
		
		jobject field = fieldFor(nm);
		if (!field)
		{
			jobj->toplevel()->throwError(kReadSealedError, nm, name());
		}

		// get the descriptor for the field
		jclass cls = (jclass) jni->CallObjectMethod(field, vm->java_lang_reflect_Field_getType() );
		descriptorVerbose(fieldDesc, cls);

		// now static or no?
		jint mod = jni->CallIntMethod(field, vm->java_lang_reflect_Field_getModifiers() );
		bool is = isStatic(mod);

		// convert the value 
		jvalue r;
		atomTojvalue(core, jobj->toplevel(), val, fieldDesc, r);

		// now make the call
		jfieldID fid = jni->FromReflectedField(field);
		jobject obj = jobj->getObject();
		if (!obj && !is)  
			jobj->toplevel()->throwError(kReadSealedError, nm, name() );

		bool worked = true;
		switch( *fieldDesc )
		{
			case 'Z':
				(is) ? jni->SetStaticBooleanField(cref, fid, r.z) : jni->SetBooleanField(obj, fid, r.z);
				break;

			case 'B':
				(is) ? jni->SetStaticByteField(cref, fid, r.b) : jni->SetByteField(obj, fid, r.b);
				break;

			case 'C':
				(is) ? jni->SetStaticCharField(cref, fid, r.c) : jni->SetCharField(obj, fid, r.c);
				break;

			case 'S':
				(is) ? jni->SetStaticShortField(cref, fid, r.s) : jni->SetShortField(obj, fid, r.s);
				break;

			case 'I':
				(is) ? jni->SetStaticIntField(cref, fid, r.i) : jni->SetIntField(obj, fid, r.i);
				break;

			case 'J':
				(is) ? jni->SetStaticLongField(cref, fid, r.j) : jni->SetLongField(obj, fid, r.j);
				break;

			case 'F':
				(is) ? jni->SetStaticFloatField(cref, fid, r.f) : jni->SetFloatField(obj, fid, r.f);
				break;

			case 'D':
				(is) ? jni->SetStaticDoubleField(cref, fid, r.d) : jni->SetDoubleField(obj, fid, r.d);
				break;

			case 'L':
			case 'G':
			case '[':
				(is) ? jni->SetStaticObjectField(cref, fid, r.l) : jni->SetObjectField(obj, fid, r.l);
				break;

			case 'V':
			default:
				AvmAssertMsg(0, "Wa! what kind of JNI type is this!");
				worked = false;
		}
		return worked;
	}

	bool JClass::isJObject(AvmCore* core, Atom a)
	{
		bool yes = false;
		if ( core->isObject(a) )
		{
			ScriptObject* sc = core->atomToScriptObject(a);
			yes = (sc->getDelegate()->atom() == JObjectClass::cc->get_prototype() ) ? true : false; //@todo fix this 
		}
		return yes;
	}

	bool JClass::isStatic(jint modifiers)
	{
		return (modifiers & vm->modifier_STATIC) == 0 ? false : true;
	}

	Atom JClass::getArrayElement(JObject* jobj, int index)
	{
		JNIEnv* jni = vm->jni;
		AvmCore* core = jobj->core();

		// now make the call if we can
		jobject obj = jobj->getObject();
		if (!obj)  
			jobj->toplevel()->throwError(kReadSealedError, core->string(core->intToAtom(index)), name() );

		jvalue r;
		char t = nm->c_str()[1];
		switch( t )
		{
			case 'Z':
			{
				jboolean* ptr = jni->GetBooleanArrayElements( (jbooleanArray) obj, 0 );
				r.z = ptr[index];
				jni->ReleaseBooleanArrayElements( (jbooleanArray)obj, ptr, JNI_ABORT);
				break;
			}
			case 'B':
			{
				jbyte* ptr = jni->GetByteArrayElements( (jbyteArray) obj, 0 );
				r.b = ptr[index];
				jni->ReleaseByteArrayElements( (jbyteArray)obj, ptr, JNI_ABORT);
				break;
			}
			case 'C':
			{
				jchar* ptr = jni->GetCharArrayElements( (jcharArray) obj, 0 );
				r.c = ptr[index];
				jni->ReleaseCharArrayElements( (jcharArray)obj, ptr, JNI_ABORT);
				break;
			}
			case 'S':
			{
				jshort* ptr = jni->GetShortArrayElements( (jshortArray) obj, 0 );
				r.s = ptr[index];
				jni->ReleaseShortArrayElements( (jshortArray)obj, ptr, JNI_ABORT);
				break;
			}
			case 'I':
			{
				jint* ptr = jni->GetIntArrayElements( (jintArray) obj, 0 );
				r.i = ptr[index];
				jni->ReleaseIntArrayElements( (jintArray)obj, ptr, JNI_ABORT);
				break;
			}
			case 'J':
			{
				jlong* ptr = jni->GetLongArrayElements( (jlongArray) obj, 0 );
				r.j = ptr[index];
				jni->ReleaseLongArrayElements( (jlongArray)obj, ptr, JNI_ABORT);
				break;
			}
			case 'F':
			{
				jfloat* ptr = jni->GetFloatArrayElements( (jfloatArray) obj, 0 );
				r.f = ptr[index];
				jni->ReleaseFloatArrayElements( (jfloatArray)obj, ptr, JNI_ABORT);
				break;
			}
			case 'D':
			{
				jdouble* ptr = jni->GetDoubleArrayElements( (jdoubleArray) obj, 0 );
				r.d = ptr[index];
				jni->ReleaseDoubleArrayElements( (jdoubleArray)obj, ptr, JNI_ABORT);
				break;
			}
			case 'L':
			case 'G':
			case '[':
				r.l = jni->GetObjectArrayElement( (jobjectArray) obj, index );
				break;

			case 'V':
			default:
				AvmAssertMsg(0, "Wa! what kind of JNI type is this!");
		}

		//@todo rt is only 1 char! make sure this call doesn't do anything with it 
		Atom a = undefinedAtom;
		jvalueToAtom(core, jobj->toplevel(), r, &t, a);
		return a;
	}

	void JClass::setArrayElement(JObject* jobj, int index, Atom val)
	{
		JNIEnv* jni = vm->jni;
		AvmCore* core = jobj->core();

		// now make the call if we can
		jobject obj = jobj->getObject();
		if (!obj)  
			jobj->toplevel()->throwError(kReadSealedError, core->string(core->intToAtom(index)), name() );

		// convert the value 
		char t = nm->c_str()[1];
		jvalue r;
		atomTojvalue(core, jobj->toplevel(), val, &t, r);
		switch( t )
		{
			case 'Z':
			{
				jboolean* ptr = jni->GetBooleanArrayElements( (jbooleanArray) obj, 0 );
				ptr[index] = r.z;
				jni->ReleaseBooleanArrayElements( (jbooleanArray)obj, ptr, 0);
				break;
			}
			case 'B':
			{
				jbyte* ptr = jni->GetByteArrayElements( (jbyteArray) obj, 0 );
				ptr[index] = r.b;
				jni->ReleaseByteArrayElements( (jbyteArray)obj, ptr, 0);
				break;
			}
			case 'C':
			{
				jchar* ptr = jni->GetCharArrayElements( (jcharArray) obj, 0 );
				ptr[index] = r.c;
				jni->ReleaseCharArrayElements( (jcharArray)obj, ptr, 0);
				break;
			}
			case 'S':
			{
				jshort* ptr = jni->GetShortArrayElements( (jshortArray) obj, 0 );
				ptr[index] = r.s;
				jni->ReleaseShortArrayElements( (jshortArray)obj, ptr, 0);
				break;
			}
			case 'I':
			{
				jint* ptr = jni->GetIntArrayElements( (jintArray) obj, 0 );
				ptr[index] = r.i;
				jni->ReleaseIntArrayElements( (jintArray)obj, ptr, 0);
				break;
			}
			case 'J':
			{
				jlong* ptr = jni->GetLongArrayElements( (jlongArray) obj, 0 );
				ptr[index] = r.j;
				jni->ReleaseLongArrayElements( (jlongArray)obj, ptr, 0);
				break;
			}
			case 'F':
			{
				jfloat* ptr = jni->GetFloatArrayElements( (jfloatArray) obj, 0 );
				ptr[index] = r.f;
				jni->ReleaseFloatArrayElements( (jfloatArray)obj, ptr, 0);
				break;
			}
			case 'D':
			{
				jdouble* ptr = jni->GetDoubleArrayElements( (jdoubleArray) obj, 0 );
				ptr[index] = r.d;
				jni->ReleaseDoubleArrayElements( (jdoubleArray)obj, ptr, 0);
				break;
			}
			case 'L':
			case 'G':
			case '[':
				jni->SetObjectArrayElement( (jobjectArray) obj, index, r.l);
				break;

			case 'V':
			default:
				AvmAssertMsg(0, "Wa! what kind of JNI type is this!");
		}
	}

	/**
	 * Given a list of parameters locate a constructor on the object 
	 * which satisfies the signature criteria
	 * @return a java.lang.reflect.Constructor
	 * object is returned or 0 if no matching method was found
	 */
	jobject JClass::constructorFor(const char* argvDesc, int argc, char* constrDesc)
	{
		JNIEnv* jni = vm->jni;
		AvmAssert( jni->IsInstanceOf(cref, vm->java_lang_Class()) == JNI_TRUE );
		jobjectArray arr = (jobjectArray) jni->CallObjectMethod(cref, vm->java_lang_Class_getConstructors());

		// now we need to iterate over the constructor array looking for a good match
		int len = jni->GetArrayLength(arr);
		jobject match = 0;
		char desc[128];
		for(int i=0; i<len; i++)
		{
			jobject m = jni->GetObjectArrayElement(arr, i);
			if (constructorDescriptor(desc, m) != argc)
				continue;

			if (match)
			{
				// return the closer argument match between the two constructors 
				const char* which = compareDescriptors(argvDesc, constrDesc, desc);
				if (which == constrDesc)
					continue;
			}
			strcpy(constrDesc, desc);
			match = m;
		}
		return match;
	}

	/**
	 * Given a name and list of parameters locate a method on the object 
	 * which satisfies the signature criteria
	 * @return a java.lang.reflect.Method 
	 * object is returned or 0 if no matching method was found
	 */
	jobject JClass::methodFor(String* name, const char* argvDesc, int argc, char* methodDesc)
	{
		JNIEnv* jni = vm->jni;
		AvmAssert( jni->IsInstanceOf(cref, vm->java_lang_Class()) == JNI_TRUE );
		jobjectArray arr = (jobjectArray) jni->CallObjectMethod(cref, vm->java_lang_Class_getMethods());

		// now we need to iterate over the method array looking for a good match
		int len = jni->GetArrayLength(arr);
		jobject match = 0;
		char desc[128];
		for(int i=0; i<len; i++)
		{
			jobject m = jni->GetObjectArrayElement(arr, i);
			jstring nm = (jstring) jni->CallObjectMethod(m, vm->java_lang_reflect_Method_getName());
			if ( !stringsEqual(nm, name) )
				continue;

			if (methodDescriptor(desc, m) != argc)
				continue;

			if (match)
			{
				// return the closer argument match between the two constructors 
				const char* which = compareDescriptors(argvDesc, methodDesc, desc);
				if (which == methodDesc)
					continue;
			}
			strcpy(methodDesc, desc);
			match = m;
		}
		return match;
	}

	/**
	 * Given a name find the associated field
	 * @return a java.lang.reflect.Field
	 * object is returned or 0 if no matching method was found
	 */
	jobject JClass::fieldFor(String* name)
	{
		JNIEnv* jni = vm->jni;
		AvmAssert( jni->IsInstanceOf(cref, vm->java_lang_Class()) == JNI_TRUE );

		jstring jstr = jni->NewString( name->c_str(), name->length() );
		jobject match = jni->CallObjectMethod(cref, vm->java_lang_Class_getField(), jstr);
		return match;
	}

	bool JClass::stringsEqual(jstring s1, String* s2)
	{
		JNIEnv* jni = vm->jni;
		bool eq = false;
		int len = jni->GetStringLength(s1);
		if (len == s2->length())
		{
			const jchar* ch = jni->GetStringChars(s1, 0);
			eq = s2->Equals(ch, len); 
			jni->ReleaseStringChars(s1, ch);
		}
		return eq;
	}

	int JClass::argumentCount(const char* d)
	{
		int i = 0;
		AvmAssert(*d == '(');
		d++; // past the paren
		for(; *d != ')' && *d != 'V'; d++, i++)
			;
		return i;
	}

	const char* JClass::boxArgs(AvmCore* core, Toplevel* toplevel, const char* descriptor, int argc, Atom* argv, jvalue* jargs)
	{
		AvmAssert(*descriptor == '(');
		descriptor++; // past the paren
		for(int i=0; i<argc; i++)
		{
			descriptor = atomTojvalue(core, toplevel, argv[i], descriptor, jargs[i]);
			if (*descriptor == ')')
				break;
		}
		return descriptor;
	}

	// build a jni-like descriptor for the signature of the method
	int JClass::methodDescriptor(char* desc, jobject method)
	{
		// first the parameters
		JNIEnv* jni = vm->jni;
		int argc = 0;
		*desc++ = '(';
		jobjectArray arr = (jobjectArray) jni->CallObjectMethod(method, vm->java_lang_reflect_Method_getParameterTypes() );
		int max = jni->GetArrayLength(arr);
		for(int i=0; i<max; i++)
		{
			jclass type = (jclass)jni->GetObjectArrayElement(arr, i);
			char ch = descriptorChar(type);
			if (ch != 'V') argc++;
			*desc++ = ch;
		}

		// now the return type
		*desc++ = ')';
		jclass rt = (jclass)jni->CallObjectMethod( method, vm->java_lang_reflect_Method_getReturnType() );
		desc = descriptorVerbose(desc, rt);
		*desc = '\0';
		return argc;
	}

	// build a jni-like descriptor for the signature of the constructor
	int JClass::constructorDescriptor(char* desc, jobject constr)
	{
		// first the parameters
		JNIEnv* jni = vm->jni;
		int argc = 0;
		*desc++ = '(';
		jobjectArray arr = (jobjectArray) jni->CallObjectMethod(constr, vm->java_lang_reflect_Constructor_getParameterTypes() );
		int max = jni->GetArrayLength(arr);
		for(int i=0; i<max; i++)
		{
			jclass type = (jclass)jni->GetObjectArrayElement(arr, i);
			char ch = descriptorChar(type);
			if (ch != 'V') argc++;
			*desc++ = ch;
		}

		// return type is void
		*desc++ = ')';
		*desc++ = 'V';
		*desc = '\0';
		return argc;
	}

	/**
	 *  Choose the better fit of 2 descriptors against a baseline 
	 *  no check is performed on the number of args 
	 */
	const char* JClass::compareDescriptors(const char* base, const char* desc1, const char* desc2)
	{
		int m1=0, m2=0; // conversion factors for d1 and d2
		const char* d1 = (*desc1 == '(') ? desc1+1 : desc1;
		const char* d2 = (*desc2 == '(') ? desc2+1 : desc2;
		while(*base && *base != ')')
		{
			m1 += conversionCost(base, d1++);
			m2 += conversionCost(base, d2++);
			base++;
		}
		return (m2<m1) ? desc2 : desc1;
	}

	int JClass::conversionCost(const char* from, const char* to)
	{
		int cost = 100;
		switch( *from )
		{
			case 'Z':
				if (*to == 'Z')	cost = 0;
				break;

			case 'B':
				if (*to == 'B' || *to == 'S' || *to == 'I') cost = 0;
				else if (*to == 'J') cost = 1;
				break;

			case 'S':
				if (*to == 'S' || *to == 'I') cost = 0;
				else if (*to == 'J') cost = 1;
				break;

			case 'I':
				if (*to == 'I') cost = 0;
				else if (*to == 'J') cost = 1;
				break;

			case 'J':
			case 'C':
			case 'F':
			case '[':
				AvmAssert(0); // Atoms can't be of this type
				break;

			case 'D':
				if (*to == 'D') cost = 0;
				else if (*to == 'F') cost = 1;
				break;

			case 'G':
				if (*to == 'G')		 cost = 0;
				else if (*to == 'C') cost = 5;
				break;

			case 'L':
				if (*to == 'L') cost = 0;
				break;
		}
		//@todo '[' objects
		return cost;
	}

	int JClass::argsDescriptor(AvmCore* core, char* desc, int argc, Atom* argv)
	{
		for(int i=0; i<argc; i++)
		{
			Atom a = argv[i];
			if (core->isBoolean(a))
				*desc++ = 'Z';
			else if (core->isInteger(a))
			{
				// see if it can be reduced
				int val = core->integer(a);
				if (val >= -128 && val < 127)
					*desc++ = 'B';
				else if (val >= -32768 && val < 32767)
					*desc++ = 'S';
				else
					*desc++ = 'I';
			}
			else if (core->isDouble(a))
				*desc++ = 'D';
			else if (core->isNullOrUndefined(a))
				*desc++ = 'X';
			else if (core->istype(a, ARRAY_TYPE))
				*desc++ = '[';
			else if (core->isString(a))
				*desc++ = 'G';
			else
				*desc++ = 'L';  // class
		}
		*desc = '\0';
		return argc;
	}

	char* JClass::descriptorVerbose(char* desc, jclass cls)
	{
		JNIEnv* jni = vm->jni;
		*desc++ = descriptorChar(cls);
		if (desc[-1] == 'L' || desc[-1] == '[')
		{
			// append the class/array name in unicode
			const jchar *str, *src;
			jstring jstr = (jstring)jni->CallObjectMethod(cls, vm->java_lang_Class_getName() );
			src = str = jni->GetStringChars(jstr, 0);
			while(src && *src)
				*desc++ = (char)*src++;
			jni->ReleaseStringChars(jstr, str);
		}
		*desc = '\0';
		return desc;
	}

	char JClass::descriptorChar(jclass cls)
	{
		JNIEnv* jni = vm->jni;
		char ch = '\0';
		if (  jni->CallBooleanMethod(cls, vm->java_lang_Class_isPrimitive()) == JNI_TRUE )
		{
			if ( jni->IsSameObject(cls, vm->java_lang_Boolean_class()) )
				ch =  'Z';
			else if ( jni->IsSameObject(cls, vm->java_lang_Byte_class()) )
				ch =  'B';
			else if ( jni->IsSameObject(cls, vm->java_lang_Character_class()) )
				ch =  'C';
			else if ( jni->IsSameObject(cls, vm->java_lang_Short_class()) )
				ch =  'S';
			else if ( jni->IsSameObject(cls, vm->java_lang_Integer_class()) )
				ch =  'I';
			else if ( jni->IsSameObject(cls, vm->java_lang_Long_class()) )
				ch =  'J';
			else if ( jni->IsSameObject(cls, vm->java_lang_Float_class()) )
				ch =  'F';
			else if ( jni->IsSameObject(cls, vm->java_lang_Double_class()) )
				ch =  'D';
			else if ( jni->IsSameObject(cls, vm->java_lang_Void_class()) )
				ch =  'V';
			else
				AvmAssert(0); // unknown type?!?
		}
		else if ( jni->IsSameObject(cls, vm->java_lang_String()) )
			ch = 'G';
		else if ( jni->CallBooleanMethod(cls, vm->java_lang_Class_isArray()) == JNI_TRUE )
			ch = '[';
		else
			ch = 'L';
		return ch;
	}

	/**
	 * Do a conversion to the given type 
	 */
	const char* JClass::atomTojvalue(AvmCore* core, Toplevel* toplevel, Atom a, const char* type, jvalue& val)
	{
		switch( *type++ )
		{
			case 'Z':
				val.z = (jboolean) ( core->booleanAtom(a) == trueAtom) ?  JNI_TRUE : JNI_FALSE; 
				break;

			case 'B':
				val.b = (jbyte) ( core->integer(a) );
				break;

			case 'C':
				val.c = *( core->string(a) )[0];
				break;

			case 'S':
				val.s = (jshort) ( core->integer(a) );
				break;

			case 'I':
				val.i = core->integer(a);
				break;

			case 'J':
				atomTojlong(core, a, val.j);  
				break;

			case 'F':
				val.f = core->number(a);
				break;

			case 'D':
				val.d = core->number(a);
				break;

			case '[':
			case 'L':
			{
				if ( !isJObject(core, a) )
				{
					// trying to pass a non-JObject into a jni method, throw exception
					toplevel->throwError(kInvalidArgumentError, core->atomToErrorString(a));
				}
				JObject* obj = (JObject*)core->atomToScriptObject(a);
				val.l = obj->getObject();
				break;
			}
			case 'G':
			{
				JNIEnv* jni = vm->jni;
				String* str = core->string(a);
				jstring jstr = jni->NewString(*str, str->length());
				//@todo need way to dispose of jstr after it last use
				val.l = jstr;
				break;
			}
			default:
				AvmAssertMsg(0, "Wa! what kind of JNI type is this!");
				val.j = 0;
				break;
		}
		return type;
	}

	void JClass::atomTojlong(AvmCore* core, Atom a, jlong& val)
	{
		if (core->isString(a))
		{
			String* str = core->string(a);
			if (core->isDigit(*str[0]) || *str[0] == '-')
			{
				//@todo need to support -9223372036854775808 to 9223372036854775807, inclusive
			}
		}

		// so a string with digits should be converable 
		val  = (jlong) ( core->integer(a) );
	}

	/**
	 * Do the conversion from the given type to the most appropriate AS type
	 */
	const char* JClass::jvalueToAtom(AvmCore* core, Toplevel* toplevel, jvalue& val, const char* type, Atom& a)
	{
		switch( *type++ )
		{
			case 'Z':
				a = (val.z == JNI_TRUE) ? trueAtom : falseAtom; 
				break;

			case 'B':
				a = core->intToAtom(val.b);
				break;

			case 'C':
			{
				wchar b[2] = { val.c, 0 };
				String* s = core->newString(b);
				a = s->atom();
				break;
			}
			case 'S':
				a = core->intToAtom(val.s);
				break;

			case 'I':
				a = core->intToAtom(val.i);
				break;

			case 'J':
				a = core->numberAtom( jlongToString(core, val.j)->atom() );  // rather demented conversion route but good enought for now
				break;

			case 'F':
				a = core->doubleToAtom( (double)val.f );
				break;

			case 'D':
				a = core->doubleToAtom( (double)val.d );
				break;

			case 'G':
			{
				if (val.l == 0)
					a = nullStringAtom;
				else
				{
#ifdef CONVERT_TO_AS_STRING
					JNIEnv* jni = vm->jni;
					const jchar* jstr = jni->GetStringChars( (jstring)val.l, 0);
					a = core->newString(jstr)->atom();
					jni->ReleaseStringChars( (jstring)val.l, jstr );
#else
					JClass* clazz = (JClass*) JObjectClass::cc->forName( core->newString("java.lang.String") );	 //@todo cache this
					JObject* obj = (JObject*) JObjectClass::cc->createJObjectShell(clazz);
					obj->setObject(val.l);
					a = obj->atom();
#endif
				}
				break;
			}
			case 'L':
			case '[':
			{
				if (val.l == 0)
					a = nullObjectAtom;
				else
				{
					JClass* clazz = (JClass*) JObjectClass::cc->forName( core->newString(type) );
					JObject* obj = (JObject*) JObjectClass::cc->createJObjectShell(clazz);
					obj->setObject(val.l);
					a = obj->atom();
				}
				break;
			}
			case 'V':
				a = undefinedAtom;
				break; 

			default:
				AvmAssertMsg(0, "Wa! what kind of JNI type is this!");
		}
		return type;
	}

	String* JClass::jlongToString(AvmCore* core, jlong& val)
	{
		wchar *b,*start;
		String* str = new (core->GetGC()) String(20);  // max 20 digits in 64bit int including sign
		b = start = str->lockBuffer();
		bool neg = (val<0) ? true : false;
		jlong l = (neg) ? -val : val;
		do 
		{
			*b-- = '0' + (char)( l % 10 );
			l /= 10;
		}
		while(l>0);
		if (neg) *b-- = '-';
		while(b>=start) *b-- = ' ';
		str->unlockBuffer(20);
		return str;
	}

	/*static*/ char* Java::startup_options = 0;

	Java::Java() 
		: bound(false)
	{
	}
	
	Java::~Java() 
	{
		if (jvm)
			jvm->DestroyJavaVM();
		jvm = 0;
		jni = 0;
	}

	String* Java::newString(AvmCore* core, jstring jstr)
	{
		int len = jni->GetStringLength(jstr);
		const jchar* raw = jni->GetStringChars(jstr, 0);
		String* s = new (core->GetGC()) String(raw, len);
		jni->ReleaseStringChars(jstr, raw);
		return s;
	}

 	char* Java::replace(char* dst, const char* src, char what, char with)
	{
		int i=0;
		for(; src[i]; i++)
			if (src[i] == what) dst[i] = with; else dst[i] = src[i];
		dst[i] = '\0';
		return dst;
	}

#define JAVA_KEY_1_5 "Software\\JavaSoft\\Java Runtime Environment\\1.5"
#define JAVA_KEY_1_4 "Software\\JavaSoft\\Java Runtime Environment\\1.4"
#define JAVA_KEY_1_3 "Software\\JavaSoft\\Java Runtime Environment\\1.3"
#define JAVA_KEY_1_2 "Software\\JavaSoft\\JRE"
#define JRE_KEY "RuntimeLib"

#define DL_LIB "/System/Library/Frameworks/JavaVM.framework/Libraries/libjvm.dylib"

	bool Java::bindLibrary(AvmCore* core)
	{
		if (!bound)
		{
#ifdef AVMPLUS_WIN32
			HKEY hKey = NULL;
			DWORD err;
			if ( (err = RegOpenKeyEx(HKEY_LOCAL_MACHINE, JAVA_KEY_1_5, 0, KEY_READ, &hKey)) == ERROR_SUCCESS )
				;
			else if ( (err = RegOpenKeyEx(HKEY_LOCAL_MACHINE, JAVA_KEY_1_4, 0, KEY_READ, &hKey)) == ERROR_SUCCESS )
				;
			else if ( (err = RegOpenKeyEx(HKEY_LOCAL_MACHINE, JAVA_KEY_1_3, 0, KEY_READ, &hKey)) == ERROR_SUCCESS )
				;
			else if ( (err = RegOpenKeyEx(HKEY_LOCAL_MACHINE, JAVA_KEY_1_2, 0, KEY_READ, &hKey)) == ERROR_SUCCESS )
				;
			else 
				return false;

			TCHAR val[256];
			DWORD length = sizeof(val)*sizeof(TCHAR);
			err = RegQueryValueEx(hKey, JRE_KEY, NULL, NULL, (LPBYTE)val, &length);
			RegCloseKey(hKey);

			UINT prev = SetErrorMode(SEM_NOOPENFILEERRORBOX);
			HMODULE lib = LoadLibrary(val);
			#define BIND_FUNC(x,f) f##_ = (_##f)GetProcAddress(x, #f)
#else /* AVMPLUS_WIN32 */
			#define SetErrorMode(x) 
			#define BIND_FUNC(x,f) f##_ = (_##f)dlsym(lib, #f "_Impl")
			void* lib = dlopen(DL_LIB, RTLD_LAZY);
#endif			
			if (lib)
			{

				BIND_FUNC(lib, JNI_GetDefaultJavaVMInitArgs);
				BIND_FUNC(lib, JNI_CreateJavaVM);
				#undef BIND_FUNC
				bound = true;
			}
			SetErrorMode(prev);
		}
		return bound;
	}
	
	int Java::startupJVM(AvmCore* core)
	{
		if (!bindLibrary(core))
			return JNI_NO_LIB;

		union {
			JDK1_1InitArgs args_1_1; /* 1.1 args */
			JavaVMInitArgs args;
		} vm;

		JavaVMOption options[2];
		vm.args.version = JNI_VERSION_1_2;
		vm.args.options = options;
		vm.args.ignoreUnrecognized = JNI_FALSE;
		options[0].optionString = "-Djava.class.path=."; // default current dir
		options[1].optionString = startup_options;
		vm.args.nOptions = (startup_options) ? 2 : 1;

		#ifdef AVMPLUS_VERBOSE
		if (core->verbose)
			core->console << "Creating JavaVM with options " << startup_options;
		#endif /* AVMPLUS_VERBOSE */
#ifdef SUPPORT_JNI_1_1
		// @todo see if we even support this and if so make sure it works
		// try to get the args for the vm 
		if (JNI_GetDefaultJavaVMInitArgs_(&vm) < 0)
		{
			// how about JDK 1.1
			vm.args.version = JNI_VERSION_1_2;
			if (JNI_GetDefaultJavaVMInitArgs_(&vm) < 0)
			{
				// final attempt!
				vm.args_1_1.version = JNI_VERSION_1_1;
				vm.args_1_1.classpath = "";
				vm.args_1_1.verbose = true;
				if (JNI_GetDefaultJavaVMInitArgs_(&vm) < 0)
					return JNI_BAD_VER;
			}
		}
#endif
		/* load and initialize a Java VM, return a JNI interface 
		* pointer in jni env */
		if (JNI_CreateJavaVM_(&jvm, (void**)&jni, &vm) < 0)
			return JNI_VM_FAIL;

		// rev up our engine
		char dst[128];  // should be big enough for any class name and descriptor

		INIT_JCLASS(java_lang_Byte);
		INIT_JCLASS(java_lang_Boolean);
		INIT_JCLASS(java_lang_Class);
		INIT_JCLASS(java_lang_Character);
		INIT_JCLASS(java_lang_Double);
		INIT_JCLASS(java_lang_Float);
		INIT_JCLASS(java_lang_Integer);
		INIT_JCLASS(java_lang_Long);
		INIT_JCLASS(java_lang_Object);
		INIT_JCLASS(java_lang_Short);
		INIT_JCLASS(java_lang_String);
		INIT_JCLASS(java_lang_Void);
		INIT_JCLASS(java_lang_reflect_Constructor);
		INIT_JCLASS(java_lang_reflect_Field);
		INIT_JCLASS(java_lang_reflect_Method);
		INIT_JCLASS(java_lang_reflect_Modifier);

		INIT_JMETHODID(java_lang_Object, equals, "(Ljava/lang/Object;)Z"); 
		INIT_JMETHODID(java_lang_Object, getClass, "()Ljava/lang/Class;"); 
		INIT_JMETHODID(java_lang_Object, toString, "()Ljava/lang/String;"); 
		INIT_JMETHODID(java_lang_Class, getName, "()Ljava/lang/String;");
		INIT_JMETHODID(java_lang_Class, getCanonicalName, "()Ljava/lang/String;");
		INIT_JMETHODID(java_lang_Class, getMethods, "()[Ljava/lang/reflect/Method;");
		INIT_JMETHODID(java_lang_Class, getFields, "()[Ljava/lang/reflect/Field;");
		INIT_JMETHODID(java_lang_Class, getField, "(Ljava/lang/String;)Ljava/lang/reflect/Field;");
		INIT_JMETHODID(java_lang_Class, getConstructors, "()[Ljava/lang/reflect/Constructor;");
		INIT_JMETHODID(java_lang_Class, isPrimitive, "()Z"); 
		INIT_JMETHODID(java_lang_Class, isArray, "()Z"); 
		INIT_SJMETHODID(java_lang_Class, forName, "(Ljava/lang/String;)Ljava/lang/Class;");
		INIT_JMETHODID(java_lang_reflect_Method, getName, "()Ljava/lang/String;");
		INIT_JMETHODID(java_lang_reflect_Method, getReturnType, "()Ljava/lang/Class;"); 
		INIT_JMETHODID(java_lang_reflect_Method, getParameterTypes, "()[Ljava/lang/Class;");
		INIT_JMETHODID(java_lang_reflect_Method, getModifiers, "()I");
		INIT_JMETHODID(java_lang_reflect_Field, getName, "()Ljava/lang/String;");
		INIT_JMETHODID(java_lang_reflect_Field, getType, "()Ljava/lang/Class;");
		INIT_JMETHODID(java_lang_reflect_Field, getModifiers, "()I");
		INIT_JMETHODID(java_lang_reflect_Constructor, getParameterTypes, "()[Ljava/lang/Class;");

		INIT_PRIMITIVE_TYPE(java_lang_Byte);
		INIT_PRIMITIVE_TYPE(java_lang_Boolean);
		INIT_PRIMITIVE_TYPE(java_lang_Character);
		INIT_PRIMITIVE_TYPE(java_lang_Double);
		INIT_PRIMITIVE_TYPE(java_lang_Float);
		INIT_PRIMITIVE_TYPE(java_lang_Integer);
		INIT_PRIMITIVE_TYPE(java_lang_Long);
		INIT_PRIMITIVE_TYPE(java_lang_Short);
		INIT_PRIMITIVE_TYPE(java_lang_Void);

		modifier_STATIC = jni->GetStaticIntField(java_lang_reflect_Modifier(), jni->GetStaticFieldID(java_lang_reflect_Modifier(), "STATIC", "I"));
		modifier_FINAL = jni->GetStaticIntField(java_lang_reflect_Modifier(), jni->GetStaticFieldID(java_lang_reflect_Modifier(), "FINAL", "I"));

		jthrowable exc = jni->ExceptionOccurred();
		if (exc)
		{
			/**
			 * If you land in here it means that one of the above class or method ids 
			 * was not found.  set a breakpoint on the dst array and single step over
			 * each.  If you see 0 in EAX then you know which one failed.
			 */
			// @todo something to do here!
			AvmAssertMsg(0, "The VM did not start up correctly; we are dead");
			jni->ExceptionClear();
			return JNI_INIT_FAIL;
		}
		return JNI_OK;
	}
}	
#else /* !AVMPLUS_WITH_JNI */
namespace avmplus {
	BEGIN_NATIVE_MAP(JObjectClass)
		NATIVE_METHOD(avmplus_JObject_toString,				JObjectClass::NYI)
		NATIVE_METHOD(avmplus_JObject_methodSignature,		JObjectClass::NYI)
		NATIVE_METHOD(avmplus_JObject_fieldSignature,		JObjectClass::NYI)
		NATIVE_METHOD(avmplus_JObject_constructorSignature,	JObjectClass::NYI)
		NATIVE_METHOD(avmplus_JObject_create,				JObjectClass::NYI)
		NATIVE_METHOD(avmplus_JObject_createArray,			JObjectClass::NYI)
		NATIVE_METHOD(avmplus_JObject_toArray,				JObjectClass::NYI)
	END_NATIVE_MAP()

	void JObjectClass::NYI() { }
}
#endif /* AVMPLUS_WITH_JNI */
