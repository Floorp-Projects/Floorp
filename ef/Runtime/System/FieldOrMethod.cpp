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
// FieldOrMethod.cpp
//
// Sriram Kini
//

// Runtime representation of java fields or methods
// Most of the operations performed on fields or methods
// are located here.

#include "ErrorHandling.h"
#include "ClassCentral.h"
#include "BytecodeTranslator.h"
#include "PrimitiveOptimizer.h"
#include "FieldOrMethod.h"
#include "CUtils.h"
#include "StringUtils.h"
#include "NativeCodeCache.h"
#include "Backend.h"
#include "Debugger.h"

#include "plstr.h"
#include "prprf.h"
#include "FieldOrMethod_md.h"
#include "InterestingEvents.h"
#include "JavaVM.h"

UT_DEFINE_LOG_MODULE(FieldOrMethod);

/* Convert value valueFrom, whose typekind is tkFrom, to a value of type tkTo. 
 * Store the result in valueTo. It is assumed that valueTo is properly
 * aligned. If a widening conversion is not possible, throw an illegalArgument
 * exception.
 */
void Field::widen(void *valueFrom, void *valueTo, 
		  TypeKind tkFrom, TypeKind tkTo)
{
    switch (tkTo) {
    case tkBoolean: 	/* Can't widen to these */
    case tkChar:
    case tkByte:
	goto cantConvert;
	
    case tkShort:
	if (tkFrom == tkByte)
	    *(Int16 *) valueTo = (Int16) *(Int8 *) valueFrom;
	else
	    goto cantConvert;
	
    case tkInt:
	switch (tkFrom) {
	case tkByte:
	    *(Int32 *) valueTo = (Int32) *(Int8 *) valueFrom;
	    break;
	    
	case tkChar:
	case tkShort:
	    *(Int32 *) valueTo= (Int32) *(Int16 *) valueFrom;
	    break;

	default:
	    goto cantConvert;

	}
	break;

    case tkLong:
	switch (tkFrom) {
	case tkByte:
	    *(Int64 *) valueTo = (Int64) *(Int8 *) valueFrom;
	    break;

	case tkChar:
	case tkShort:
	    *(Int64 *) valueTo = (Int64) *(Int16 *) valueFrom;
	    break;

	case tkInt:
	    *(Int64 *) valueTo = (Int64) *(Int32 *) valueFrom;
	    break;
	    
	default:
	    goto cantConvert;
	}
	break;

    case tkFloat:
	switch (tkFrom) {
	case tkByte:
	    *(Flt32 *) valueTo = (Flt32) *(Int8 *) valueFrom;
	    break;

	case tkChar:
	case tkShort:
	    *(Flt32 *) valueTo = (Flt32) *(Int16 *) valueFrom;
	    break;

	case tkInt:
	    *(Flt32 *) valueTo = (Flt32) *(Int32 *) valueFrom;
	    break;
	    
	case tkLong:
	    *(Flt32 *) valueTo = (Flt32) *(Int64 *) valueFrom;
	    break;
	    
	default:
	    goto cantConvert;
	}
	break;

    case tkDouble:
	switch (tkFrom) {
	case tkByte:
	    *(Flt64 *) valueTo = (Flt64) *(Int8 *) valueFrom;
	    break;

	case tkChar:
	case tkShort:
	    *(Flt64 *) valueTo = (Flt64) *(Int16 *) valueFrom;
	    break;

	case tkInt:
	    *(Flt64 *) valueTo = (Flt64) *(Int32 *) valueFrom;
	    break;

	case tkLong:
	    *(Flt64 *) valueTo = (Flt64) *(Int64 *) valueFrom;
	    break;
	    
	case tkFloat:
	    *(Flt64 *) valueTo = (Flt64) *(Flt32 *) valueFrom;
	    break;
	    
	default:
	    goto cantConvert;
	}
	break;

    default:
	runtimeError(RuntimeError::internal);
    }

    return;

cantConvert:
    runtimeError(RuntimeError::illegalArgument);
}


/* Return the full name of the field or method from the short name and the
 * class name. A full name is of the form "public static int java.lang.foo.bar";
 * the correspoding package name is java.lang.foo, and the short name is bar.
 */
const char *Field::getFullName()
{
	const char *accessString = ((modifiers & CR_FIELD_STATIC)) ? "static " : "";
	const char *statusString;
	const char *finalString = ((modifiers & CR_FIELD_FINAL)) ? "final " : "";

	if ((modifiers & CR_FIELD_PUBLIC))
		statusString = "public ";
	else if ((modifiers & CR_FIELD_PROTECTED))
		statusString = "protected ";
	else
		statusString = "";

	char *typeString = new char[20];
	Uint32 typeStrLen = 20;

	*typeString = 0; 
	getTypeString(*typeOfField, typeString, typeStrLen);

	char *_fullName;

	/* The fullname is the name of the class appended by a dot, appended
	 * by the field name
	 */
	Uint32 outlen = PL_strlen(accessString) + PL_strlen(finalString)+
		PL_strlen(typeString) + PL_strlen(statusString)+
		PL_strlen(className) + PL_strlen(packageName) + PL_strlen(shortName) + 8;

	_fullName = new char[outlen];

	if (packageName && *packageName) 
		PR_snprintf(_fullName, outlen, "%s%s%s%s %s.%s.%s",
				statusString, accessString, finalString, typeString, 
				packageName, className, shortName);
	else
		PR_snprintf(_fullName, outlen, "%s%s%s%s %s.%s",
				statusString, accessString, finalString, typeString, 
				className, shortName);

	fullName = central.getStringPool().intern(_fullName);

	delete [] _fullName;
	delete [] typeString;

	return fullName;
}


/* Appends the string for the type to str, updating strLen */
void FieldOrMethod::getTypeString(const Type &type, char *&str, Uint32 &strLen) 
{
    switch (type.typeKind) {
    case tkBoolean:
	append(str, strLen, "boolean");
	break;

    case tkUByte:
	append(str, strLen, "unsigned byte");
	break;

    case tkByte:
	append(str, strLen, "byte");
	break;

    case tkChar:
	append(str, strLen, "byte");
	break;

    case tkShort:
	append(str, strLen, "short");
	break;

    case tkInt:
	append(str, strLen, "int");
	break;

    case tkLong:
	append(str, strLen, "long");
	break;

    case tkFloat:
	append(str, strLen, "float");
	break;

    case tkDouble:
	append(str, strLen, "double");
	break;

    case tkObject:
    case tkInterface: {
	const ClassOrInterface *clazz = static_cast<const ClassOrInterface *>(&type);
	append(str, strLen, clazz->getName());
	break;
    }

    case tkArray: {
	const Array *array = static_cast<const Array *>(&type);
	getTypeString(*const_cast<Type *> (&array->componentType), str, strLen);
	append(str, strLen, "[]");
	break;
    }

    default:
	runtimeError(RuntimeError::unknown);
	break;
    }
}

/* Convert a JVM type descriptor into the long form defined by
 * java.lang.reflect, e.g. [[C is emitted as "char[][]" 
 * Advances descriptor to point to the point beyond the argument just
 * parsed. Appends description of parsed argument to out.
 */
static bool descriptorToString(char *out, Uint32 outLen, const char * &descriptor) {
    char *typeString;

    switch (*descriptor) {
    case 'B':
	typeString = "byte";
	break;
    case 'C':
	typeString = "char";
	break;
    case 'D':
	typeString = "double";
	break;
    case 'F':
	typeString = "float";
	break;
    case 'I':
	typeString = "int";
	break;
    case 'J':
	typeString = "long";
	break;
    case 'S':
	typeString = "short";
	break;
    case 'Z':
	typeString = "boolean";
	break;
    case 'V':
	typeString = "void";
	break;
    case '[':
	descriptor++;
	descriptorToString(out, outLen, descriptor);
	append(out, outLen, "[]");
	return true;
    case 'L':
	{
	    descriptor++;
	    char *semicolon = strchr(descriptor, ';');
	    if (!semicolon)
		verifyError(VerifyError::badClassFormat);
	    char *tmpStr = new char[semicolon - descriptor + 1];
	    char *t = tmpStr;
	    while (*descriptor != ';') {
		if (*descriptor == '/')
		    *t++ = '.';
		else
		    *t++ = *descriptor;
		descriptor++;
	    }
	    *t = 0;
	    descriptor++;  // Advance past semi-colon
	    append(out, outLen, tmpStr);
	    delete [] tmpStr;
	    return true;
	}
    case ')':
	return false;
    default:
	verifyError(VerifyError::badClassFormat);
	return false; // NOTREACHED
    }
    append(out, outLen, typeString);
    descriptor++;
    return true;
}

/* Return the full name of the method from the short name and the
 * class name. A full name is of the form 
 * "public static java.lang.foo.bar(int, long, java.lang.Object)".
 * the corresponding package name is java.lang.foo, and the short name is bar.
 */
const char *Method::getFullName()
{
    const char *accessString = ((modifiers & CR_METHOD_STATIC)) ? "static " : "";

    const char *statusString;

    if ((modifiers & CR_METHOD_PUBLIC)) 
	statusString = "public ";
    else if ((modifiers & CR_METHOD_PROTECTED))
	statusString = "protected ";
    else if ((modifiers & CR_METHOD_PRIVATE))
	statusString = "private ";
    else
	statusString = "";

    const char *abstractString = ((modifiers & CR_METHOD_ABSTRACT)) ?
	"abstract " : "";

    char *returnString = new char[50];
    Uint32 returnStringLen = 50;
    *returnString  = 0;
    
    char *argsString = new char[256];
    Uint32 argsStringLen = 256;
    *argsString  = 0;

    const char *s = signatureString;
    if (*s++ != '(')
	verifyError(VerifyError::badClassFormat);

    // Check for method which takes no arguments
    if (*s == ')')
	append(argsString, argsStringLen, "()");
    else {
	append(argsString, argsStringLen, "(");

	// Convert each method argument to string
	while (descriptorToString(argsString, argsStringLen, s))
	    append(argsString, argsStringLen, ",");

	if (*s != ')') {
	    delete [] argsString;
	    delete [] returnString;
	    verifyError(VerifyError::badClassFormat);
	}

	/* overwrite the last comma with closing paren and null */
	argsString[PL_strlen(argsString) - 1] = ')';
	
    }
  
    /* Now parse the return argument, only if we're not a constructor*/
    if (shortName != summary.getInitString()) {      
	s++;
	if (!descriptorToString(returnString, returnStringLen, s)) {
	    delete [] argsString;
	    delete [] returnString;
	    verifyError(VerifyError::badClassFormat);
	}

	append(returnString, returnStringLen, " ");
    } 

    char *_fullName;
  
    /* Allocate enough space to hold strings for arguments */
    Uint32 outLen = PL_strlen(accessString)+PL_strlen(statusString)+
	PL_strlen(abstractString)+PL_strlen(returnString)+
	PL_strlen(className)+PL_strlen(shortName)+PL_strlen(packageName)+PL_strlen(argsString)+5;
  
    _fullName = new char[outLen];

    if (shortName == summary.getInitString()) {
		if (packageName && *packageName)
		    PR_snprintf(_fullName, outLen, "%s%s%s%s.%s%s",
				accessString, statusString, returnString, packageName, className, 
				argsString);
		else
		   PR_snprintf(_fullName, outLen, "%s%s%s%s%s",
			       accessString, statusString, returnString, className, 
			       argsString); 
	} else {
		if (packageName && *packageName)
		    PR_snprintf(_fullName, outLen, "%s%s%s%s%s.%s.%s%s",
				accessString, statusString, abstractString, returnString, 
				packageName, className, shortName, argsString);
		else
		    PR_snprintf(_fullName, outLen, "%s%s%s%s%s.%s%s",
				accessString, statusString, abstractString, returnString, 
				className, shortName, argsString);
    }

    fullName = central.getStringPool().intern(_fullName);
    
    delete [] returnString;
    delete [] argsString;
    delete [] _fullName;
    
    return fullName;
}

/* Class Field */

/* Return the size occupied by objects whose type is represented by type */
static Uint32 getTypeSize(const Type &type)
{
    if (isNonVoidPrimitiveKind(type.typeKind)) {
	return isDoublewordKind(type.typeKind) ? 8 : 4; //getTypeKindSize(type.typeKind);
    } else if (type.typeKind == tkObject || type.typeKind == tkInterface) {
	return sizeof(ptr);
    } else if (type.typeKind == tkArray) {
	return sizeof(ptr);  //Arrays and objects are implemented as references
    } else
	return 0; 
}

/* Construct a field given it's packageName, className and shortName.
 * (Also see FieldOrMethod::FieldOrMethod). 
 * summary is the information about the declaring class; central is
 * the repository used to hold ClassFileSummary structures.
 * info contains compile-time information about the field.
 * offset is the offset of this field in the declaring class if
 * the field is an instance (ie., non-static) field.
 */
Field::Field(const char *packageName, const char *className, 
	     const char *shortName, 
	     ClassFileSummary &summary,
	     ClassCentral &c, 
	     Pool &pool,
	     Uint32 offset,
	     const FieldInfo &info) :
    FieldOrMethod(Standard::get(cField), 
		  packageName, className, shortName, info.getDescriptor()->getUtfString(),
		  summary, c, pool)
{
    bool isStatic, isVolatile, isConstant;
    const char *sig;
  
    info.getInfo(sig, isVolatile, isConstant, isStatic);
  
    const char *next;
    typeOfField = &central.parseFieldDescriptor(sig, next);
  
    modifiers = info.getAccessFlags();
  
    if (isStatic) {
	Uint32 size = getTypeKindSize(typeOfField->typeKind);
	pos.address = staticAddress(new (p) char[size]);
	memset(addressFunction(pos.address), 0, size);
    } else 
	pos.offset = offset;
}

// Get the memory address of a static field
addr Field::getAddress() const
{
    assert((modifiers & CR_FIELD_STATIC));
    
    // If static initializers haven't been run, execute them now.
    (const_cast<ClassOrInterface*>(static_cast<const ClassOrInterface *>(getDeclaringClass())))->runStaticInitializers();

    return pos.address;
}

/* Return a JavaObject from the raw bytes located at address. For
 * primitive types, wrap an object wrapper around it
 */
JavaObject &Field::convertObject(void *address)
{
    if (isPrimitiveKind(typeOfField->typeKind))
	return Class::makePrimitiveObject(typeOfField->typeKind, address);
    else {
	/* For object and array types, what's stored is a pointer to the 
	 * JavaObject
	 */
	JavaObject *objp = *((JavaObject **)address);
	return *objp;
    }
}


/* Get the raw address of the object represented by this field */
void *Field::getRaw(JavaObject *obj) 
{
    void *address;

    if ((modifiers & CR_FIELD_STATIC)) {
	address = addressFunction(getAddress());
    } else {
	if (!obj)
	    runtimeError(RuntimeError::nullPointer);

	// Make sure that the type of the object is the
	// same as the declaring class of this field, or
	// a sub-class
	if (! (&obj->getType() == getDeclaringClass() ||
	       (getDeclaringClass()->isAssignableFrom(obj->getType())))
	    )
	    runtimeError(RuntimeError::illegalArgument);
	
	address = (void *) (((char *) obj) + pos.offset);
    }
  
    return address;
}

/* Get the value of the field, wrapped in a JavaObject 
 * if neccessary
 */
JavaObject &Field::get(JavaObject *obj)
{
    void *address = getRaw(obj);
  
    return convertObject(address);
}

// Return true if this field represents a primitive type
inline bool Field::isPrimitive()
{
    return (typeOfField->isPrimitive());
}


#define FIELD_GET_PRIMITIVE(typeTo, tkTo)					\
	PR_BEGIN_MACRO											\
	if (!isPrimitive())										\
		runtimeError(RuntimeError::illegalArgument);		\
															\
	typeTo value;											\
	void *address = getRaw(obj);							\
															\
	if (typeOfField->typeKind == tkTo)						\
		return *(typeTo *) address;							\
	else {													\
		widen(address, &value, typeOfField->typeKind, tkTo);\
		return (typeTo) value;								\
	}														\
	PR_END_MACRO


/* Get the value of this field within the object obj,
 * converted to boolean. 
 */
Int8 Field::getBoolean(JavaObject *obj)
{
    FIELD_GET_PRIMITIVE(Int8, tkBoolean);
}

  
Int8 Field::getByte(JavaObject *obj)
{
    FIELD_GET_PRIMITIVE(Int8, tkByte);
}

Int16 Field::getChar(JavaObject *obj)
{
    FIELD_GET_PRIMITIVE(Int16, tkChar);
}


Int16 Field::getShort(JavaObject *obj)
{
    FIELD_GET_PRIMITIVE(Int16, tkShort);
}

Int32 Field::getInt(JavaObject *obj)
{
    FIELD_GET_PRIMITIVE(Int32, tkInt);
}

Int64 Field::getLong(JavaObject *obj)
{
    FIELD_GET_PRIMITIVE(Int64, tkLong);
}

Flt32 Field::getFloat(JavaObject *obj)
{
    FIELD_GET_PRIMITIVE(Flt32, tkFloat);
}

Flt64 Field::getDouble(JavaObject *obj)
{
    FIELD_GET_PRIMITIVE(Flt32, tkDouble);
}


void Field::setFromValue(ValueKind vk, Value value)
{
    assert(getModifiers() & CR_FIELD_STATIC);
    
    void *address = getRaw(0);
    const char *sig = getSignatureString();

    switch (vk) {
    case vkInt:
	if (*sig != 'I' && *sig != 'B' && *sig != 'C' && *sig != 'S' &&
	    *sig != 'Z') 
	    verifyError(VerifyError::badClassFormat);
	*(Int32 *) address = value.getValueContents((Int32 *) 0);
	break;

    case vkLong:
	if (*sig != 'J')
	    verifyError(VerifyError::badClassFormat);
	*(Int64 *) address = value.getValueContents((Int64 *) 0);
	break;

    case vkFloat:
	if (*sig != 'F')
	    verifyError(VerifyError::badClassFormat);
	*(Flt32 *) address = value.getValueContents((Flt32 *) 0);
	break;

    case vkDouble:
	if (*sig != 'D')
	    verifyError(VerifyError::badClassFormat);
	*(Flt64 *) address = value.getValueContents((Flt64 *) 0);
	break;

    case vkAddr:
	if (PL_strcmp(sig, "Ljava/lang/String;"))
	    verifyError(VerifyError::badClassFormat);

	*(JavaObject **) address = (JavaObject *) addressFunction(value.getValueContents((addr *) 0));	
	break;

    default:
	verifyError(VerifyError::badClassFormat);
    }
}

void Field::set(JavaObject *obj, JavaObject &value)
{
    void *address = getRaw(obj);
    
    int size = getTypeKindSize(typeOfField->typeKind);

    /* If this is a primitive type, then it is encased in a Java Object;
     * otherwise, it is the object itself
     */
    if (isPrimitiveKind(typeOfField->typeKind)) {
	if (!Standard::isPrimitiveWrapperClass(*static_cast<const Class *>(&value.getType())))
	    runtimeError(RuntimeError::illegalArgument);
	
		memcpy(address, (char *)&value+sizeof(JavaObject), size);
    } else {
		assert(size == sizeof(char *));
		*((JavaObject **) address) = &value;
    }

}

// Set the value of the field in various ways 
// This code is dependent on our current layout, and
// must be reworked when our layout changes
#define FIELD_SET_PRIMITIVE(typeFrom, tkFrom)					\
	PR_BEGIN_MACRO												\
	if (!isPrimitive())											\
		runtimeError(RuntimeError::illegalArgument);			\
																\
	void *address = getRaw(obj);								\
																\
	if (typeOfField->typeKind == tkFrom)						\
		*(typeFrom *) address = value;							\
	else														\
		widen(&value, address, tkFrom, typeOfField->typeKind);	\
	PR_END_MACRO


void Field::setBoolean(JavaObject *obj, Int8 value)
{
    FIELD_SET_PRIMITIVE(Int8, tkBoolean);
}

void Field::setByte(JavaObject *obj, Int8 value)
{
    FIELD_SET_PRIMITIVE(Int8, tkByte);
}

void Field::setChar(JavaObject *obj, Int16 value)
{
    FIELD_SET_PRIMITIVE(Int16, tkChar);
}

void Field::setShort(JavaObject *obj, Int16 value)
{
    FIELD_SET_PRIMITIVE(Int16, tkShort);
}

void Field::setInt(JavaObject *obj, Int32 value)
{
    FIELD_SET_PRIMITIVE(Int32, tkInt);
}

void Field::setLong(JavaObject *obj, Int64 value)
{
    FIELD_SET_PRIMITIVE(Int64, tkLong);
}

void Field::setFloat(JavaObject *obj, Flt32 value)
{
    FIELD_SET_PRIMITIVE(Flt32, tkFloat);
}

void Field::setDouble(JavaObject *obj, Flt64 value)
{
    FIELD_SET_PRIMITIVE(Flt64, tkDouble);
}



/* Class Method */

/* Parses the type descriptor of the method, resolving and
 * loading classes as necessary. Creates the signature structure
 * for the method.
 */
void Method::parseMethodDescriptor(const char *s)
{
    char *paramstr;
    const char *t;
    const Type **params, *ret;
    int paramSize = 0;
    int numParams = 0;
    bool isStatic;

    if (!(isStatic = (getModifiers() & CR_METHOD_STATIC) != 0)) {
	/* Pass in ourselves as the first argument */
	numParams = 1;
	params = new (p) const Type *[(paramSize = 5)];
	params[0] = getDeclaringClass();
    }

    if (*s++ != '(')
	verifyError(VerifyError::badClassFormat);

    /* Get everything between this and ')' */
    if (!(t = PL_strchr(s, ')')))
	verifyError(VerifyError::badClassFormat);

    int len = t-s+1;

    if (len != 1) {

	TemporaryStringCopy copy(s, len-1);
	paramstr = copy;
    
	const char *next = paramstr;

	while (*next) {
      
	    if (numParams+1 > paramSize) {
		paramSize += 5;

		const Type **oldParams = params;		
		params = new (p) const Type *[paramSize];

		for (int i = 0; i < numParams; i++)
		    params[i] = oldParams[i];
	
		/* We don't do this since we're using a pool to allocate, but 
		 * this is a potential source of runaway memory usage
		 */
		/*	delete [] oldParams;*/
	    }
	
	    params[numParams] = &central.parseFieldDescriptor(next, next);      
	    numParams++;      
	}
    
    } 

    s += len;

    /* Parse the return descriptor */
    if (*s == 'V') {
	if (*(s+1))
	    verifyError(VerifyError::badClassFormat);
  		
	ret = &PrimitiveType::obtain(tkVoid);
    } else {
	const char *next;
	ret = &central.parseFieldDescriptor(s, next);

	if (*next) 
	    verifyError(VerifyError::badClassFormat);  
    }

    signature.nArguments = numParams;
    signature.argumentTypes = params;
    signature.resultType = ret;
    signature.isStatic = isStatic;
}

/* Create a Method object for a method with given packageName, className
 * and shortName. (Also see FieldOrMethod::FieldOrMethod). 
 * summary is the information about the declaring class; central is
 * the repository used to hold ClassFileSummary structures.
 * info contains compile-time information about the method.
 * vtableType is the type of the object to be passed on to the 
 * JavaObject constructor.
 */
Method::Method(const char *packageName, const char *className,
	       const char *shortName, 
	       ClassFileSummary &summary, ClassCentral &c, Pool &pool,
	       const MethodInfo &info, const Type *vtableType) :
    FieldOrMethod((vtableType) ? *vtableType : *static_cast<const Type *>(&Standard::get(cMethod)),
		  packageName, className, shortName, info.getDescriptor()->getUtfString(),
		  summary, c, pool),
    minfo(info), argsSize(-1), dynamicallyDispatched(false), vIndex(-1),
    signatureResolved(false)
{
    modifiers = info.getAccessFlags();

    bool isAbstract, isStatic, isFinal, isSynchronized, isNative;
    const char *sig;
    minfo.getInfo(sig, isAbstract, isStatic, isFinal, isSynchronized, isNative);
    signatureString = sig;

	DEBUG_LOG_ONLY(htmlName = NULL);
}


#ifdef DEBUG_LOG
void Method::printMethodAsBytecodes(LogModuleObject &f)
{
	const Signature &signature = getSignature();
    AttributeCode *code = static_cast<AttributeCode *>(minfo.getAttribute("Code"));

	assert(code);

	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Signature: "));
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%d locals, %d stack words, %d code bytes\n", code->getMaxLocals(), 
						  code->getMaxStack(), code->getCodeLength()));

	if (modifiers & CR_METHOD_ABSTRACT)
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("abstract "));
	if (modifiers & CR_METHOD_STATIC)
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("static "));
	if (modifiers & CR_METHOD_FINAL)
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("final "));
	if (modifiers & CR_METHOD_SYNCHRONIZED)
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("synchronized "));
	if (modifiers & CR_METHOD_NATIVE)
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("native "));

	signature.resultType->printRef(UT_LOG_MODULE(FieldOrMethod));
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" %s(", shortName));
	if (signature.nArguments) {
		uint i = 0;	
		while (true) {
			signature.argumentTypes[i]->printRef(f);
			i++;
			if (i == signature.nArguments)
				break;
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", "));
		}
	}
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, (")\n"));

	const bytecode *bytecodesBegin = (const bytecode *)code->getCode();

	disassembleBytecodes(f, bytecodesBegin, bytecodesBegin + code->getCodeLength(), bytecodesBegin, summary.getConstantPool(), 0);
	disassembleExceptions(f, code->getNumExceptions(), code->getExceptions(), summary.getConstantPool(), 0);
}
#endif // DEBUG_LOG


// A little macro that just compares our stage to test stage and
// causes the function to return if we are past our desired stage
#define COMPILE_UP_TO_STAGE(inOurStage, inTestStage)	\
	PR_BEGIN_MACRO										\
	if (inOurStage < inTestStage)						\
		return (0);										\
	PR_END_MACRO

void* Method::compile()
{
    Class			*clazz = (Class *) getDeclaringClass();
    bool			inhibitBackpatching = VM::theVM.getInhibitBackpatching();
    CompileStage	stage = VM::theVM.getCompileStage();

    UT_LOG(FieldOrMethod, PR_LOG_ALWAYS, ("\n\n\n\n*** Compiling method %s::%s\n", clazz->name, getName()));

	// preprocess bytecodes
	gCompileBroadcaster->broadcastEvent(gCompileBroadcaster, kBroacastCompile, this);

    if (modifiers & CR_METHOD_ABSTRACT)				// not allowed to compile abstract methods
		verifyError(VerifyError::abstractMethod);

    AttributeCode *code = static_cast<AttributeCode *>(minfo.getAttribute("Code"));

    if (!code) {
		UT_LOG(FieldOrMethod, PR_LOG_DEBUG, ("No code attribute\n"));
		runtimeError(RuntimeError::illegalAccess);
    }

	COMPILE_UP_TO_STAGE(stage, csPreprocess);

    Uint32 maxLocals = code->getMaxLocals();
    Uint32 maxStack = code->getMaxStack();
    const bytecode *bytecodesBegin = (const bytecode *)code->getCode();
    Uint32 codeLength = code->getCodeLength();
  
    const Signature &signature = getSignature();
    const Type **argumentTypes = signature.argumentTypes;
    const Type *resultType = signature.resultType;
    int nArguments = signature.nArguments;

	DEBUG_LOG_printMethodAsBytecodes(UT_LOG_MODULE(FieldOrMethod)); 

	// create bytecode graph
	UT_LOG(FieldOrMethod, PR_LOG_ALWAYS, ("creating bytecode graph...\n"));
    Pool primitivePool;
    ControlGraph *cg;

    {
	    Pool bytecodeGraphPool;

	    ValueKind *argumentKinds = new(bytecodeGraphPool) ValueKind[nArguments];
	    for (int i = 0; i != nArguments; i++)
			argumentKinds[i] = typeKindToValueKind(argumentTypes[i]->typeKind);

	    bool isStatic = (getModifiers() & CR_METHOD_STATIC) != 0;
	    bool isSynchronized = (getModifiers() & CR_METHOD_SYNCHRONIZED) != 0;
	    BytecodeGraph bg(summary, bytecodeGraphPool, !isStatic, isSynchronized, nArguments, argumentKinds,
						 typeKindToValueKind(resultType->typeKind), maxLocals, maxStack, bytecodesBegin, codeLength,
						 code->getNumExceptions(), code->getExceptions(),
						 *this);

	    {
			Pool tempPool1;

		    UT_LOG(FieldOrMethod, PR_LOG_ALWAYS, ("dividing into blocks...\n\n"));
		    bg.divideIntoBlocks(tempPool1);

			DEBUG_LOG_ONLY(bg.print(UT_LOG_MODULE(FieldOrMethod), summary.getConstantPool()));  
			COMPILE_UP_TO_STAGE(stage, csGenPrimitives);
			// tempPool1 is destroyed here.
		}

		{
			// create primitive graph
			Pool tempPool2;
			cg = BytecodeTranslator::genPrimitiveGraph(bg, primitivePool, tempPool2);
			// tempPool2 is destroyed here.
		}
		// bytecodeGraphPool is destroyed here.
	}

    if (stage < csOptimize) {
		DEBUG_LOG_ONLY(cg->print(UT_LOG_MODULE(FieldOrMethod)));
		return 0;
    }

	// optimize
	UT_LOG(FieldOrMethod, PR_LOG_ALWAYS, ("optimizing primitive graph...\n"));
    simpleTrimDeadPrimitives(*cg);

	DEBUG_LOG_ONLY(cg->print(UT_LOG_MODULE(FieldOrMethod)));

	COMPILE_UP_TO_STAGE(stage, csGenInstructions);

	// generate code
    void *compiledCode = translateControlGraphToNative(*cg, *this);

    UT_LOG(FieldOrMethod, PR_LOG_ALWAYS, ("*** Done Compiling Method %s::%s\n", 
					  clazz->name, shortName));

	// anyone know what this is?
#ifdef DEBUG_LOG
	if (VM::debugger.getEnabled())
		printDebugInfo();
#endif

    if (!inhibitBackpatching)
		updateVTable();

	gCompileBroadcaster->broadcastEvent(gCompileBroadcaster, kBroacastCompile, this);

    return compiledCode;
}

// Disassemble the bytecode to the specified logmodule
#ifdef DEBUG_LOG
void Method::dumpBytecodeDisassembly(LogModuleObject &f)
{
    AttributeCode *code = static_cast<AttributeCode *>(minfo.getAttribute("Code"));
    const bytecode *bytecodesBegin = (const bytecode *)code->getCode();
    Uint32 codeLength = code->getCodeLength();
	const ConstantPool *constantPool = summary.getConstantPool();

	disassembleBytecodes(f, bytecodesBegin, bytecodesBegin + codeLength, bytecodesBegin, constantPool, 0);
}
#endif

// Examine the type of the argument arg against the actual type type.
// If arg is a primitive wrapper class, extract the primitive value
// from the wrapper class. Otherwise, pass the arg on unchanged.
Uint32 Method::getWordArg(const Type &actualType, JavaObject &arg)
{
	// Check arguments for correctness
	if (isPrimitiveKind(actualType.typeKind)) {
		if (Standard::isPrimitiveWrapperClass(*static_cast<const Class *>(&arg.getType())))
			return Class::getPrimitiveValue(arg);
		else {
			runtimeError(RuntimeError::illegalArgument);
			return 0;
		}
    } else {
		// Ensure that arg is assignable to our argument
		return (Uint32) &arg;
    }
}

void Method::getDoubleWordArg(const Type &, JavaObject &arg, Uint32 &hi, Uint32 &lo)
{
    struct {
	  Uint32 lo;
	  Uint32 hi;
    } u;

    memcpy(&u, ((char *) &arg+sizeof(JavaObject)), 8);
	
    hi = u.hi;
    lo = u.lo;
}

JavaObject *Method::invoke(JavaObject * PC_ONLY(obj), JavaObject * PC_ONLY(args)[], Int32 PC_ONLY(nArgs))
{
#if defined (XP_PC) || defined(LINUX)
#if 0
    void *code = addressFunction(getCode());
#endif

    Int32 i;
    Int32 start = 0;

    Uint32 *argsToBePassed = 0;

    // Make sure we're passing the right kind of object 
    if (!(getModifiers() & CR_METHOD_STATIC)) 
	if (!obj || !getDeclaringClass()->isAssignableFrom(*obj->type))
	    runtimeError(RuntimeError::illegalArgument);
      
    // Make sure we're passing the right number of arguments
    Int32 nArgsToCompare = ((getModifiers() & CR_METHOD_STATIC)) ? 
	getSignature().nArguments : getSignature().nArguments-1;
  
    if (nArgs != nArgsToCompare)
		runtimeError(RuntimeError::illegalArgument);

    // If we're an instance method, pass ourselves in as the first argument
    if ((getModifiers() & CR_METHOD_STATIC) == 0) {
	  start = 1;
	  argsToBePassed = new Uint32[++nArgs*2];
	  argsToBePassed[0] = (Uint32) obj;
    } else
	  argsToBePassed = new Uint32[nArgs*2];
  
	Int32 nWords = 0;  // Number of words to push on the stack

    for (nWords = start, i = start;  i < nArgs; i++) {
	  // FIX Currently assumes none of the args are double-word
	  if (!Standard::isDoubleWordPrimitiveWrapperClass(*static_cast<const Class *>(signature.argumentTypes[i])))
		argsToBePassed[nWords++] = (args[i-start]) ? getWordArg(*signature.argumentTypes[i], *args[i-start]): 0;
		
	  else {
		  if (args[i-start])
			  getDoubleWordArg(*signature.argumentTypes[i], *args[i - start], 
							   argsToBePassed[nWords], argsToBePassed[nWords+1]);
		  else 
			  argsToBePassed[nWords] = argsToBePassed[nWords+1] = 0;

		  nWords += 2;
	  }
    }

#if 0
    //Uint32 returnValue;
    for (Int32 n = nWords - 1; n >= 0; n--) {
	  void *arg = (void *) argsToBePassed[n];
	  prepareArg(n, arg);    
    }
    
    callFunc(code);
  
    getReturnValue(returnValue);
#else
	ReturnValue returnValue = invokeRaw(obj, argsToBePassed, nWords);
#endif
	
	if (argsToBePassed) 
      delete [] argsToBePassed;

    // Wrap up the return value inside an object if neccessary 
    if (signature.resultType->typeKind != tkVoid) {
		if (isPrimitiveKind(signature.resultType->typeKind)) {
			if (!isDoublewordKind(signature.resultType->typeKind))
				return &Class::makePrimitiveObject(signature.resultType->typeKind, 
												   (void *) &returnValue.wordValue);
			else
				return &Class::makePrimitiveObject(signature.resultType->typeKind, 
												   (void *) &returnValue.doubleWordValue);
		} else
			return (JavaObject *) returnValue.wordValue;
    } else
		return 0;
	
#else
    trespass("Method::invoke() not implemented for this platform\n");
    return 0;
#endif
}


ReturnValue Method::invokeRaw(JavaObject* /*obj*/, Uint32 argsToBePassed[], Int32 nWords)
{
	Uint32 wordRet;
	ReturnValue returnValue;
    void *code = addressFunction(getCode());

    for (Int32 n = nWords - 1; n >= 0; n--) {
		void *arg = (void *) argsToBePassed[n];
		prepareArg(n, arg);    
    }
    
    callFunc(code);
	    
	getReturnValue(wordRet);

	// XXX Double-word return values are currently broken
	assert(!isDoublewordKind(getSignature().resultType->typeKind));

	returnValue.wordValue = wordRet;
	
	return returnValue;
}

#undef PC_ONLY

Int32 Method::getArgsSize()
{
    if (argsSize >= 0)
	return argsSize;

    Int32 size = 0;

    // Make sure the signature has been created
    getSignature();

    for (uint i = 0; i < signature.nArguments; i++) {
	const Type *t = signature.argumentTypes[i];

	assert(t);
	size += getTypeSize(*t);
    }
  
    return (argsSize = size);
}


#ifdef DEBUG_LOG
//
// Print a reference to this method for debugging purposes.
// Return the number of characters printed.
//
int Method::printRef(LogModuleObject &f) const
{
    int o = getDeclaringClass()->printRef(f);
    return o + UT_OBJECTLOG(f, PR_LOG_ALWAYS, (".%s", shortName));
}
#endif

/* Return a pointer to the compiled code of the method. This might be a
   * stub that actually compiles the method when called. This stub may or
   * may not "backpatch" the call site to point to the newly compiled method
   * depending on whether or not the method is normally statically dispatched.
   * This backpatching is not done when inhibitBackpatch is true.  (This
   * argument is used for implementations of interface methods which are
   * always dispatched using a vtable and which, therefore, shouldn't
   * backpatch their caller.)
   */
addr Method::getCode(bool inhibitBackpatch)
{
    MethodDescriptor descriptor(*this);
    return NativeCodeCache::getCache().lookupByDescriptor(descriptor,
														  inhibitBackpatch); 
														  
}


// Checks to see that the given className, methodName and signature
// represent this method.
bool Method::isSelf(const char *fullyQualifiedClassName, const char *simpleMethodName,
		    const char *javaSig)
{
    Class *clazz = (Class *) getDeclaringClass();
  
    return (((fullyQualifiedClassName == clazz->getName()) || (*fullyQualifiedClassName == '*')) && 
	    ((simpleMethodName == shortName) || (*simpleMethodName == '*')) &&
	    ((javaSig == signatureString) || (*javaSig == '*')));

}

#ifdef DEBUG_LOG
// Dumps the local variable table with the debug information
void
Method::printDebugInfo()
{
	AttributeCode *code = (AttributeCode *) 
		getMethodInfo().getAttribute("Code");
	AttributeLocalVariableTable *localVariableTable = 
		(AttributeLocalVariableTable *)
		code->getAttribute("LocalVariableTable");
	
	if (!localVariableTable) {
		UT_LOG(Debugger, PR_LOG_ALWAYS, ("No debug info\n"));
		return;
	}

	// Walk the local variable table
	Uint32 n = localVariableTable->getNumEntries();
	for(Uint32 i=0; i < n; i++) {
		LocalVariableEntry& entry = localVariableTable->getEntryByNumber(i);
		entry.dump();
		DoublyLinkedList<IntervalMapping>& mappings = entry.mappings;
		for (DoublyLinkedList<IntervalMapping>::iterator j = mappings.begin();
			 !mappings.done(j); 
			 j = mappings.advance(j)) {
			 IntervalMapping& mapping = mappings.get(j);
			 mapping.dump();
		}
	}
}
#endif

/* Class Constructor */
JavaObject &Constructor::newInstance(JavaObject *params[], Int32 numParams)
{
    ClassOrInterface *declaringClass = getDeclaringClass();

    if (declaringClass->isArray())
		runtimeError(RuntimeError::illegalArgument);

    JavaObject &newObject = (static_cast<Class *>(declaringClass))->newInstance();
  
    invoke(&newObject, params, numParams);
    return newObject;
}

#ifdef DEBUG_LOG

void Method::resolveHTMLName()
{
    if (htmlName)
	return;

    // gererate a hash value from the fully qualified name
    const char* p = toString();
    Uint32 hash = 0;
    while(*p)
	hash = (hash << 1) + *p++;

    char hashText[11];
    sprintf(hashText, "[%08x]", hash);

    // name length
    const char* name = getName();
    Uint32 len = strlen(name);
    if(len > 16)
	len = 16;

    htmlName = new char[len + 10 + 5 + 1];

    // copy name and clean out undesirable chars
    htmlName[len] = 0;
    strncpy(htmlName, name, len);
    char* q = htmlName;
    while(*q) {
	switch(*q) {
	case ':': case '<': case '>': case '/': case ' ':
	    *q = '_';
	}
	q++;
    }

    // append hash vale and .html
    strcat(htmlName, hashText);
    strcat(htmlName, ".html");
    Uint32 newLen = strlen(htmlName);
    assert(newLen < 32);
}

#endif // DEBUG_LOG
