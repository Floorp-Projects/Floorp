/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef _FIELD_OR_METHOD_H_
#define _FIELD_OR_METHOD_H_

#include "plstr.h"

#include "ClassFileSummary.h"
#include "ClassCentral.h"
#include "NativeDefs.h"
#include "LogModule.h"
#include "Debugger.h"

/* Compiling stage for debugging purposes */
enum CompileStage {
	csRead, 
	csPreprocess, 
	csGenPrimitives, 
	csOptimize, 
	csGenInstructions,
	csExecute      
};

union ReturnValue {
	Uint32 wordValue;
	Uint64 doubleWordValue;
};

struct NS_EXTERN FieldOrMethod: JavaObject {

public:
	/* shortName is the short (unqualified name) of the field/method. 
	 * packageName is the name of the package within which the declaring
	 * class is enclosed. className is the unqualified name of the declaring 
	 * class  of this field/method. summary is the ClassFileSummary of the 
	 * declaring class. 
	 * pool is used to allocate dynamic memory; central is a repository
	 * class that is used to add classes that this class might encounter
	 * when parsing its descriptor.
	 */
	FieldOrMethod(const Type &type, const char *packageName, const char *className, 
				  const char *shortName, const char *signatureString,
				  ClassFileSummary &summ, ClassCentral &c, Pool &pool):
		JavaObject(type), 
		central(c), fullName(0), shortName(shortName), className(className),
		packageName(packageName), p(pool), summary(summ),
		signatureString(signatureString) {}   

	
	/* Returns true if the signature string represents a primitive
	 * type or a string type. 
	 */
	static bool primitiveOrStringSignature(const char *sig);

	/* Returns the Class or Interface that this field or method is
	 * a member of.  In order to get the real Class you can pass this
	 * to asClass, in JavaObject.h -- which will attempt to safely
	 * cast to a Class (if possible). FIX-ME can this return NULL?
	 */
	ClassOrInterface *getDeclaringClass() const {
		return static_cast<ClassOrInterface *>(summary.getThisClass());
	}
  
	/* Returns the short (unqualified) name of the field/method */
	const char *getName() const { return shortName; }
  
	/* Returns the access flags of the field/method as found in 
	 * the class file.
	 */
	int getModifiers() const { return modifiers; }
   
	/* Returns true if obj is an instance of this field. */
	bool equals(JavaObject &obj) const {
		/* Since we will not be creating duplicate copies of Field objects,
		 * a simple pointer comparision will suffice
		 */
		return ((JavaObject *) this == &obj);
	}

	/* return the string corresponging to the signature of the method. */
	const char *getSignatureString() { return signatureString; }
  
	// int hashCode();

protected:
	/* ClassCentral object used to add classes as neccessary when parsing the
	 * field descriptor
	 */
	ClassCentral &central;

	/* Full name of the field or method */
	mutable const char *fullName;

	/* Short name of the field or method */
	const char *shortName;

	/* unqualified name of the class that this field/method belongs to */
	const char *className;

	/* Package name of the field or method */
	const char *packageName;

	/* field/method modifiers and access flags */
	Uint16 modifiers;

	/* pool used to dynamically allocate memory */
	Pool &p;

	/* Summary that holds compile-time information about the class */
	ClassFileSummary &summary;

	/* The UTF string representation of the signature of this method */
	const char *signatureString;
  
	/* Given a type, returns a string describing the type as
	 * specified in the Java Reflection API. Initially, str is of
	 * size (not length) strLen; this method grows str as necessary and sets
	 * strLen to the new size. 
	 */
	static void getTypeString(const Type &type, char *&str, Uint32 &strLen);

};


struct NS_EXTERN Field : FieldOrMethod {

public:
	Field(const char *packageName, const char *className,
		  const char *shortName,
		  ClassFileSummary &summary, ClassCentral &c, 
		  Pool &pool,
		  Uint32 runningOffset,
		  const FieldInfo &info);

	static void widen(void *valueFrom, void *valueTo, TypeKind tkFrom, TypeKind tkTo);

	/* Returns the fully qualified name and type of the field or method, eg,
	 * public static java.lang.thread.priority
	 */
	const char *toString() {
		if (!fullName) getFullName();
		return fullName;
	}
  
	/* In all the get and set methods below, obj can be NULL if the field
	 * is static. otherwise, obj must be an object of the right type.
	 */

	/* Get the value of this field within the object given by obj */
	JavaObject &get(JavaObject *obj);
  
  
	/* Get the value of the field in various forms.  */
	Int8 getBoolean(JavaObject *obj);
	Int8 getByte(JavaObject *obj);
	Int16 getChar(JavaObject *obj);
	Int16 getShort(JavaObject *obj);
	Int32 getInt(JavaObject *obj);
	Int64 getLong(JavaObject *obj);
	Flt32 getFloat(JavaObject *obj);
	Flt64 getDouble(JavaObject *obj);

	/* Set the value of the field from the given valueKind and value.
	 * Throws a VerifyError::badClassFormat if the types do not match.
	 */
	void setFromValue(ValueKind vk, Value value);

	/* Set the value of this field within the object obj to value */
	void set(JavaObject *obj, JavaObject &value);

	/* Set the value of the field in various forms. Appropriate exceptions
	 * are thrown if types don't match
	 */
	void setBoolean(JavaObject *obj, Int8 value);
	void setByte(JavaObject *obj, Int8 value);
	void setChar(JavaObject *obj, Int16 value);
	void setShort(JavaObject *obj, Int16 value);
	void setInt(JavaObject *obj, Int32 value);
	void setLong(JavaObject *obj, Int64 value);
	void setFloat(JavaObject *obj, Flt32 value);
	void setDouble(JavaObject *obj, Flt64 value);

	/* return the address of the field if static */
	addr getAddress() const; 

	/* return the offset of the field if dynamic */
	Uint32 getOffset() const 
	{ assert (!(modifiers & CR_FIELD_STATIC)); return pos.offset; }
  
	/* Get the type of the field  */
	const Type &getType() const { return *typeOfField; }

protected:
	const char *getFullName();

private:
	/* The Type class for this field */
	const Type *typeOfField;

	/* Address of this field in memory/Offset inside declaring class */
	union NS_EXTERN {
		Uint32 offset;
		addr address;
	} pos;

	/* Return a new instance of an object of the given class.
	 */
	JavaObject &makeObject(const Class &c) {
		return *new(p) JavaObject(c);
	}

	JavaObject &convertObject(void *address);
	JavaObject &makeObjectWrapper(void *address);
	void *getRaw(JavaObject *obj);

	bool isPrimitive();
};


struct NS_EXTERN Method : public FieldOrMethod {

public:
	Method(const char *packageName, const char *className,
		   const char *shortName,
		   ClassFileSummary &summary, ClassCentral &c, Pool &p,
		   const MethodInfo &info, const Type *vtableType = 0);

	/* Returns the fully qualified name and type of the field or method, eg,
	 * public static java.lang.thread.priority
	 */
	const char *toString() {
		if (!fullName) getFullName();
		return fullName;
	}

	/* Get the vtable index of this method. */
	bool getVIndex(Uint32 &index) const 
	{ index = vIndex; return vIndex != -1; }

	/* Set the vIndex of the method (for dynamically resolved methods only)
	   Note: a single method can be both statically and dynamically resolved
	   depending on whether or not it is called through an interface.
	   */  
	void setVIndex(Uint32 index) {
		if (getModifiers() & CR_METHOD_STATIC)
			return;
		if (vIndex == -1)
			vIndex = index; 
	}
  
	/* getWordArg() is used to generate a 32-bit value that
	 * can be passed as an argument to a function. If arg is
	 * a JavaObject representing a non-primitive type,
	 * a pointer to arg is returned as a 32-bit value. If
	 * arg is a primitive class (java/lang/Integer...etc)
	 * then the corresponding primitive value is returned
	 * as a 32-bit unsigned (raw) quantity.
	 */
	static Uint32 getWordArg(const Type &type, JavaObject &arg);

	/* getDoubleWordArg() is used to generate a 64-bit value that
	 * can be passed as an argument to a function.
	 * arg is an object of class java/lang/Long or java/lang/Boolean;
	 * getDoubleWordArg() extracts and returns the 64-bit value
	 * as two 32-bit quantities.
	 */
	static void getDoubleWordArg(const Type &type, JavaObject &arg, Uint32 &hi, Uint32 &lo);

	/* returns the Type class corresponding to the return parameter of
	 * this method.
	 */
	const Type &getReturnType()  { resolveSignature(); return *signature.resultType; }

	/* Sets params to point to the Type objects for the parameters of
	 * this method. Returns number of params
	 */
	Int32 getParameterTypes(const Type **&params) 
	{ resolveSignature(); params = signature.argumentTypes; return signature.nArguments; }

	/* Sets params to point to the exception types for this method.
	 * Returns number of exception types
	 */
	Int32 getExceptionTypes(const Type *&params);
  
	/* Invoke the specified method on the given object. args is an 
	 * array of parameters to be passed to the method. If a parameter is
	 * of a primitive type, then it must be wrapped inside a primitive wrapper
	 * class object. obj is the object to invoke the method on; it can be NULL 
	 * if the method being invoked is static. Otherwise, it must be an object of 
	 * the correct type.
	 * Returns the object returned by the method just invoked, wrapped
	 * inside a primitive class if neccessary.
	 */
	JavaObject *invoke(JavaObject *obj, JavaObject *args[], Int32 nArgs);

	/* Invoke the specified method on the given object. args is an 
	 * array of parameters to be passed to the method. All parameters must be
	 * of the "raw" form: they are passed in as an array of 32-bit words. 
	 * Double-word parameters must be passed in as two consecutive words 
	 * in the array. obj is the object to invoke the method on; it can be NULL 
	 * if the method being invoked is static. Otherwise, it must be an object of 
	 * the correct type. nWords is the number of words in the array, not the
	 * number of arguments.
	 * This routine does not check for correctness of arguments; it is the caller's 
	 * responsibility to ensure that the arguments are correct.
	 *
	 * Returns the return value of the method as a word or double-word 
	 * depending on the return-type.
	 */
	ReturnValue invokeRaw(JavaObject *obj, Uint32 args[], Int32 nWords);

	/* Returns the signature of the method */
	const Signature &getSignature() { resolveSignature(); return signature; }

	/* Compile the byte code of the method. Returns a pointer to the compiled 
	 * code. For dynamically dispatched methods, patches the vtable of the 
	 * declaring class/interface with the pointer to the compiled code.
	 */
	void *compile();

	/* Return the total size of the parameters of this method. */
	Int32 getArgsSize();

	/* Return a pointer to the compiled code of the method. This might be a
	 * stub that actually compiles the method when called.
	 */
	addr getCode(bool inhibitBackpatch = false);

	/* A method can have a vtableIndex when it serves as the implementation
	   of an interface method and yet still be statically resolved during
	   "normal" method lookup, hence the requirement for a separate flag that
	   indicates whether normal method dispatch is done using a vtable. */
	bool getDynamicallyDispatched() const { return dynamicallyDispatched; }
	void setDynamicallyDispatched(bool b) {dynamicallyDispatched = b; }

	/* Update the vtable of the declaring class and all subclasses that inherit
	   this method. */
	void updateVTable() {summary.updateVTable(*this);}

	void resolveSignature() {
		if (!signatureResolved)
			parseMethodDescriptor(signatureString);
	}

	bool isSelf(const char *fullyQualifiedClassName, const char *simpleMethodName,
				const char *javaSig);

	const MethodInfo &getMethodInfo() { return minfo; }

#ifdef DEBUG_LOG
	int printRef(LogModuleObject &f) const;
	void printDebugInfo();
#endif
	pcToByteCodeTable& getPC2Bci() { return pc2bci; };

protected:
	const char *getFullName();

private:

	/* Parse the signature of the method and fill up the Signature field. */
	void parseMethodDescriptor(const char *sig);
  
	/* Signature of the method. */
	Signature signature;
    
	/* Static structure that holds the byte code of the method */
	const MethodInfo &minfo;

	/* Total size of the parameters of this method. Set to -1 if it 
	 * hasn't been computed yet.
	 */
	Int32 argsSize;

	/* If false, this method has been statically resolved and the caller must be
	 * back-patched when this method is compiled. If true, then this method
	 * has been dynamically resolved; the caller is not backpatched, but the 
	 * vtable entry is.
	 */
	bool dynamicallyDispatched;

	/* If true, all classes mentioned in method's signature have been loaded */
	bool signatureResolved;

	/* VIndex of the method in the class that defined it if a dynamically
	 * resolved method; if a statically resolved  method, a is the address of 
	 * the code of the method.
	 */
	union  {
		Int32 vIndex;
		addr a;
	};

	pcToByteCodeTable pc2bci;

	// HTML debug support
#ifdef DEBUG_LOG
protected:
	char* htmlName;
	void resolveHTMLName();
public:
	const char* getHTMLName() { resolveHTMLName(); return htmlName; }
	void dumpBytecodeDisassembly(LogModuleObject &f);

	/* on in debug version that prints a method's bytecode representation */
	void printMethodAsBytecodes(LogModuleObject &f);
#define DEBUG_LOG_printMethodAsBytecodes(f) printMethodAsBytecodes(f);
#else
#define DEBUG_LOG_printMethodAsBytecodes(f)
#endif// DEBUG_LOG
};

struct NS_EXTERN Constructor : public Method {
public:
	Constructor(const char *packageName, const char *className,
				const char *shortName,
				ClassFileSummary &summary, ClassCentral &c, Pool &p,
				const MethodInfo &info) :
		Method(packageName, className, shortName, summary, c, p, info,
			   &Standard::get(cConstructor)) { }

	/* Construct a new instance of the object. params is an array of
	 * arguments for the constructor; numParams is the length of the 
	 * array.
	 */
	JavaObject &newInstance(JavaObject *params[], Int32 numParams);
};

inline bool FieldOrMethod::primitiveOrStringSignature(const char *sig) {
	return (!PL_strcmp(sig, "Ljava/lang/String;") ||
			!PL_strcmp(sig, "Z") ||
			!PL_strcmp(sig, "B") ||
			!PL_strcmp(sig, "C") ||
			!PL_strcmp(sig, "D") ||
			!PL_strcmp(sig, "F") ||
			!PL_strcmp(sig, "I") ||
			!PL_strcmp(sig, "S"));
}
	
#endif /* _FIELD_OR_METHOD_H_ */





