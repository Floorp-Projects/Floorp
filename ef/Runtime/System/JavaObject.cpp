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
#include "ClassWorld.h"
#include <stdlib.h>
#include "ClassCentral.h"
#include "FieldOrMethod.h"
#include "plstr.h"
#include "prprf.h"

#include "JavaVM.h"


/* Counter used for setting the interfaceNumber member in
   Interface's and in those Array's where the element type
   is an interface. */
static Uint32 sInterfaceCount;

// Size of each TypeKind
const Int8  typeKindSizes[nTypeKinds] =
{
    0,	// tkVoid
	1,	// tkBoolean
	1,	// tkUByte
	1,	// tkByte
	2,	// tkChar
	2,	// tkShort
	4,	// tkInt
	8,	// tkLong
	4,	// tkFloat
	8,	// tkDouble
	sizeof(ptr),	// tkObject 
	4,	            // tkSpecial
	sizeof(ptr),	// tkArray
	sizeof(ptr)     // tkInterface
};

// lg2 of the size of each TypeKind
const Int8 lgTypeKindSizes[nTypeKinds] =
{
	-1,	// tkVoid
	0,	// tkBoolean
	0,	// tkUByte
	0,	// tkByte
	1,	// tkChar
	1,	// tkShort
	2,	// tkInt
	3,	// tkLong
	2,	// tkFloat
	3,	// tkDouble
	2,	// tkObject     FIXME - should be 3 on 64-bit machine
	2,	// tkSpecial
	2,	// tkArray      FIXME - should be 3 on 64-bit machine
	2	// tkInterface  FIXME - should be 3 on 64-bit machine
};

// Number of environment slots taken by each TypeKind
const Int8 typeKindNSlots[nTypeKinds] =
{
	0,	// tkVoid
	1,	// tkBoolean
	1,	// tkUByte
	1,	// tkByte
	1,	// tkChar
	1,	// tkShort
	1,	// tkInt
	2,	// tkLong
	1,	// tkFloat
	2,	// tkDouble
	1,	// tkObject
	1,	// tkSpecial
	1,	// tkArray
	1	// tkInterface
};


// ----------------------------------------------------------------------------
// Type


#ifdef DEBUG_LOG
//
// Print a reference to this Type for debugging purposes.
// Return the number of characters printed.
//
int Type::printRef(LogModuleObject &f) const
{
	const char *typeName;

	switch (typeKind) {
		case tkVoid:
			typeName = "void";
			break;
		case tkBoolean:
			typeName = "boolean";
			break;
		case tkUByte:
			typeName = "unsigned byte";
			break;
		case tkByte:
			typeName = "byte";
			break;
		case tkChar:
			typeName = "char";
			break;
		case tkShort:
			typeName = "short";
			break;
		case tkInt:
			typeName = "int";
			break;
		case tkLong:
			typeName = "long";
			break;
		case tkFloat:
			typeName = "float";
			break;
		case tkDouble:
			typeName = "double";
			break;
		case tkObject:
		case tkInterface:
			return static_cast<const ClassOrInterface *>(this)->printRef(f);
		case tkArray:
			return static_cast<const Array *>(this)->printRef(f);
		case tkSpecial:
			typeName = "special";
			break;
		default:
			typeName = "????";
	}
	return (UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s", typeName)));
}
#endif

// Get the superclass of a Type, as defined by java.lang.Class.getSuperClass()
const Type *Type::getSuperClass() const
{
  if (typeKind == tkArray)
    return static_cast<const Type*>(&Standard::get(cObject));
  else if (typeKind == tkInterface)
    return NULL;
  else
    return superType;
}

// Get the unique serial number that is assigned to interfaces
// and to arrays of interfaces.  This serial number is used to perform
// runtime assignability checks.
Uint32 Type::getInterfaceNumber() const
{
    if (typeKind == tkArray) {
        return asArray(*this).getInterfaceNumber();
    } else {
        assert(typeKind == tkInterface);
        return asInterface(*this).getInterfaceNumber();
    }
}

const char *Type::getName() const
{
    switch (typeKind) {
    case tkArray:
        return asArray(*this).getName();

    case tkInterface:
        return asInterface(*this).getName();

    case tkObject:
        return asClass(*this).getName();

    default:
        return asPrimitiveType(*this).getName();
    }
}

    
// Returns the modifiers (access flags) for this class object.
Uint32 Type::getModifiers() const
{
    switch (typeKind) {
    case tkObject:
    case tkInterface:
        return ((ClassOrInterface*)this)->getModifiers();

    case tkArray:
        // Modifier flags for an array are the same as the modifiers for its element type.
        // XXX - Check this assumption for  compatibility with Sun JDK
        return asArray(*this).getElementType().getModifiers();

    default:
        // Primitive type modifier flags
        // XXX - Check this assumption for  compatibility with Sun JDK
        return CR_ACC_PUBLIC | CR_ACC_FINAL | CR_ACC_ABSTRACT;
    }
}

//
// Return true if sub is a subtype of super.  In other words, return true if
// an instance or variable of type sub can be assigned to a variable of type super.
//
bool implements(const Type &sub, const Type &super)
{
	// Every type is a subtype of itself.
	if (&super == &sub)
		return true;

	TypeKind superKind = super.typeKind;
	TypeKind subKind = sub.typeKind;

	switch (subKind) {
		case tkObject:
			switch (superKind) {
				// A class's supertypes can only be its superclasses or superinterfaces --
				// never primitive objects, arrays, or special objects.
				case tkObject:
					return asClass(sub).implements(asClass(super));
				case tkInterface:
					return asClass(sub).implements(asInterface(super));
				default:;
			}
			break;

		case tkArray:
			switch (superKind) {
				// An array's supertypes can only be:
				//   the class Object,
				//   the interface Cloneable, or
				//   arrays whose element type is a supertype of this array's element type.
				case tkObject:
					return &super == &Standard::get(cObject);
				case tkArray:
					return implements(static_cast<const Array *>(&sub)->componentType,
									  static_cast<const Array *>(&super)->componentType);
				case tkInterface:
					return &super == &Standard::get(iCloneable);
				default:;
			}
			break;

		case tkInterface:
			switch (superKind) {
				// An interface's supertypes can only be:
				//   the class Object, or
				//   the interface's superinterfaces.
				case tkObject:
					return &super == &Standard::get(cObject);
				case tkInterface:
					return asInterface(sub).implements(asInterface(super));
				default:;
			}
			break;

		default:;
	}

	// There are no other nontrivial subtyping relationships.
	return false;
}


// ----------------------------------------------------------------------------
// PrimitiveType

const PrimitiveType *PrimitiveType::primitiveTypes[nPrimitiveTypeKinds];

static const char primitiveTypeNames[nPrimitiveTypeKinds][8] =
{
    "void",	    // tkVoid
	"boolean",	// tkBoolean
	"",	        // tkUByte  (Not a real Java type)
	"byte",	    // tkByte
    "char",	    // tkChar
	"short",	// tkShort
	"int",	    // tkInt
	"long",	    // tkLong
	"float",	// tkFloat
	"double"	// tkDouble
};

// JVM-specified encodings for primitive types in signatures
static const char primitiveTypeEncodings[nPrimitiveTypeKinds] =
{
    'V',	    // tkVoid
	'Z',	    // tkBoolean
	'\0',	        // tkUByte  (Not a real Java type)
	'B',	    // tkByte
    'C',	    // tkChar
	'S',	    // tkShort
	'I',	    // tkInt
	'J',	    // tkLong
	'F',	    // tkFloat
	'D'	        // tkDouble
};

//
// Initialize the table used by the obtain method.
// cPrimitiveType must have already been initialized.
//
void PrimitiveType::staticInit()
{
	for (TypeKind tk = tkVoid; tk < nPrimitiveTypeKinds; tk = (TypeKind)(tk+1))
		primitiveTypes[tk] = new PrimitiveType(Standard::get(cClass), tk);
}

const PrimitiveType *PrimitiveType::getClass(const char *className)
{
    int i;
    for (i = 0; i < nPrimitiveTypeKinds; i++) {
        if (!PL_strcmp(primitiveTypeNames[i], className))
            return &obtain((TypeKind)i);
    }
    return NULL;
}

const char *PrimitiveType::getName() const {
    assert(isPrimitive());
    return primitiveTypeNames[(int)typeKind];
}

char PrimitiveType::getSignatureEncoding() const {
    assert(isPrimitive());
    return primitiveTypeEncodings[(int)typeKind];
}

// ----------------------------------------------------------------------------
// Array


const Array *Array::primitiveArrays[nPrimitiveTypeKinds];


//
// Initialize a type for arrays.
// Do not call this constructor directly; use obtain or Type::getArrayType
// or Array::make instead.
// 
Array::Array(const Type &componentType, const Type &superType, Uint32 nVTableSlots):
	Type(Standard::get(cClass),
         &superType, tkArray, componentType.final, nVTableSlots, 0),
         componentType(componentType)
{
	assert(componentType.typeKind != tkVoid);
    interfaceNumber = 0;
    fullName = NULL;
    
    /* Assign each array of interfaces a unique serial number */
    if (getElementType().typeKind == tkInterface)
        interfaceNumber = ++sInterfaceCount;
}

// An array's name is just the name of it's element type followed by an appropriate number
//   of pairs of braces.
const char *Array::getName() const {
    if (fullName)
        return fullName;

    int numDimensions, i;
    const Type &element = getElementType(numDimensions);
    char *name, *n;

    if (element.isPrimitive()) {
        name = new char[numDimensions + 2];
        n = name;
        for (i = 0; i < numDimensions; i++)
	        *n++ = '[';
        *n++ = asPrimitiveType(element).getSignatureEncoding();
    } else {
        const char *elementName = element.getName();
        // Leave room for an 'L' prefix and ';' suffix if we've got an object element type
        name = new char[PL_strlen(elementName) + numDimensions + 3];
        n = name;

        for (i = 0; i < numDimensions; i++)
	        *n++ = '[';

        *n++ = 'L';
        while ((*n = *elementName) != 0) {
            n++;
            elementName++;
        }
        *n++ = ';';
    }
    *n = 0;

    fullName = name;
    return name;
}

//
// Initialize a new Interface object that describes essential information such as
// inheritance relationships of objects belonging to a single Java interface.
//
// package is the package that defined this interface, name is the unqualified
// name of this interface, and interfaces is an array of nInterfaces elements
// that lists all interfaces from which this interface derives.
//
Interface::Interface(Package &package, const char *name, 
					 Uint32 nInterfaces, Interface **interfaces, 
					 ClassFileSummary *summary):
	ClassOrInterface(Standard::get(cClass), tkInterface, NULL, false, 0, package, name, 
					 nInterfaces, interfaces,
					 summary), interfaceNumber(++sInterfaceCount)
{superType = &Standard::get(cObject);}

/* For every superinterface of this interface, obtain the Array object
   that is the type of an array of such interfaces with dimensionality
   given by numDimensions.  Add that Array to the list of assignable types.
   This is a helper function for buildAssignabilityTableForInterfaces(). */
void Interface::addInterfaceArraysToAssignableTable(InterfaceDispatchTable *interfaceDispatchTable,
                                                    int numDimensions)
{
    int i;
    Type *arrayType;

    /* Obtain the array-of-interfaces type and add it to the list
       of assignable types */
    arrayType = this;
    assert(numDimensions >= 1);
    for (i = 0; i < numDimensions; i++) {
        arrayType = const_cast<Type *>(static_cast<const Type *>(&arrayType->getArrayType()));
    }
    interfaceDispatchTable->add(asArray(*arrayType).getInterfaceNumber());
}

// Setup the lookup tables so that we can perform instanceof and
// checkcast operations on arrays where the test type is
// either an array of interfaces or the Cloneable interface.
void Array::buildAssignabilityTableForInterfaces()  
{
    interfaceDispatchTable = new InterfaceDispatchTable();

    /* If this is an array of objects and those objects implement
       one or more interfaces, obtain the type corresponding to
       an array of those interfaces and add it to the list of
       assignable types */

     /* Only class and array instances can implement interfaces, and
        only class instances can implement interfaces other than Cloneable. */
    int numDimensions;
    const Type *elementType = &getElementType(numDimensions);
    if (elementType->typeKind == tkObject) {
        const Class &elementClass = asClass(*elementType);

        /* In addition to their direct superinterfaces, the elements of this
           array implement all the interfaces implemented by their superclass. */
        if (elementType != &Standard::get(cObject)) {
            const Array &superType = asArray(*getSuperType());
            interfaceDispatchTable->inherit(superType.interfaceDispatchTable);
        }

        /* For every interface implemented by this Class, obtain the Array object
           that is the type of an array of such interfaces.  Add that Array to the
           list of assignable types for an array of this Class. */
        for (Uint32 i = 0; i < elementClass.nInterfaces; i++) {
            Interface *superInterface = elementClass.interfaces[i];
            superInterface->addInterfaceArraysToAssignableTable(interfaceDispatchTable, numDimensions);
        } 
    }

    /* All arrays implement the Cloneable interface. */
    interfaceDispatchTable->add(Standard::get(iCloneable).getInterfaceNumber());
  
    /* Check for interface table overflow */
    if (!build(interfaceDispatchTable))
        verifyError(VerifyError::resourceExhausted);

    /* Initialize the fields within the Type that are used by compiled code
       for assignability checks */
    interfaceVIndexTable = 0; /* Arrays never implement interface methods */
    interfaceAssignableTable = interfaceDispatchTable->getAssignableTableBaseAddress();
    assignableMatchValue = interfaceDispatchTable->getAssignableMatchValue();
}

//
// Initialize a new Array object that describes essential information such as
// the type hierarchy.
//
// componentType is the type of elements in the array.  It can itself be an array type.
//
Array &Array::make(const Type &componentType)
{
    const Type *superType;
    
    /* Obtain the supertype of the array */
    const Type *javaLangObject = static_cast<const Type*>(&Standard::get(cObject));
    if (isPrimitiveKind(componentType.typeKind) || (&componentType == javaLangObject)) {
        /* The supertype of a primitive array or an array of Object is Object. */
        superType = javaLangObject;
    } else {
        /* The supertype of an array of non-primitive elements, where the
           elements do not have type Object, is an array of the same
           dimensionality of the element's supertype. */
        superType = static_cast<const Type *>(&componentType.getSuperType()->getArrayType());
    }
    
    /* Reserve one vtable slot for our class pointer, to implement instanceof 
       operations when the target type is neither an interface nor an array of
       interfaces. */
    Uint32 nVTableSlots = 1;
    const Type *super = superType;
    while (super) {
        nVTableSlots += super->nVTableSlots;
        super = super->getSuperType();
    }
    
    /* The sizeof(Class) in the expression below is not a typo.  VTables for
       both Array and Class objects start at the same offset. */
    Int32 objSize = sizeof(Class) + nVTableSlots*sizeof(VTableEntry);
    void *mem = malloc(objSize);
    memset(mem, 0, objSize);  // Set vtable entries to zero
    Array *array = new(mem) Array(componentType, *superType, nVTableSlots);
    
    /* Construct the array's vtable.  Start by copying our parent's vtable */
    Uint32 i;
    for (i = 0; i < superType->nVTableSlots; i++)
        array->setVTableEntry(i, (superType->getVTableEntry(i)).classOrCodePtr);
    
    /* The last vtable entry is a self-referential pointer, used for instanceof */
    array->setVTableEntry(i, array);

    /* Set up the tables that allow us to perform instanceof and checkcast
       operations on this array when the target type is an array of interfaces */
    array->buildAssignabilityTableForInterfaces();

    return *array;
}

// Return the Type of an array's element (not the Type of
// its component).  Also, return the dimensionality of the
// array  in numDimensions.
const Type &Array::getElementType(int &numDimensions) const
{
    const Type *compType;
    compType = &componentType;
    numDimensions = 1;
    while (compType && (compType->typeKind == tkArray)) {
        compType = &(asArray(*compType).componentType);
        numDimensions++;
    }
    return *compType;
}

const Type &Array::getElementType() const
{
    int dummy;
    return getElementType(dummy);
}

#ifdef DEBUG_LOG
//
// Print a reference to this array for debugging purposes.
// Return the number of characters printed.
//
int Array::printRef(LogModuleObject &f) const
{
	int o = componentType.printRef(f);
	return o + UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("[]"));
}
#endif


//
// Initialize the table used by the obtain method.
// PrimitiveType::obtain must have already been initialized.
//
void Array::staticInit()
{
	primitiveArrays[tkVoid] = 0;	// Can't have an array of type Void.
	for (TypeKind tk = tkBoolean; tk < nPrimitiveTypeKinds; tk = (TypeKind)(tk+1))
		primitiveArrays[tk] = &PrimitiveType::obtain(tk).getArrayType();
}

#ifdef DEBUG
void Array::printValue(LogModuleObject &f, JavaArray *array, int maxRecursion,
                       Uint32 printFlags, const char *newlineString,
                       Vector<JavaObject *> *printedObjects) const
{
    assert(array->getType().typeKind == tkArray);
    char *indentedNewlineString = NULL;  
    if (newlineString) {
        indentedNewlineString = new char[strlen(newlineString) + 3];
        strcpy(indentedNewlineString, newlineString);
        strcat(indentedNewlineString, "  ");
    }

    Uint8 *componentPtr = (Uint8*)array + arrayEltsOffset(componentType.typeKind);
    
    int componentSize = getTypeKindSize(componentType.typeKind);

    // Arbitrarily limit printed array size
    int numElementsToPrint = array->length;
    if (numElementsToPrint > 10)
        numElementsToPrint = 10;

    bool printArrayIndices = printFlags & PRINT_ARRAY_INDICES;

    if (newlineString)
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s", newlineString));
    UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("{"));
    int elementNum;
    for (elementNum = 0; elementNum < numElementsToPrint; elementNum++) {
        if (newlineString)
            UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s ", newlineString));
        if (printArrayIndices)
            UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("[%2d] = ", elementNum));
        ::printValue(f, componentPtr, componentType, maxRecursion - 1, printFlags,
                     indentedNewlineString, printedObjects);
        if (!newlineString && (elementNum != (numElementsToPrint - 1)))
            UT_OBJECTLOG(f, PR_LOG_ALWAYS, (","));
        componentPtr += componentSize;
    }

    if (numElementsToPrint < (Int32)array->length)
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" ..."));
    if (newlineString)
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s", newlineString));
    UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("}"));
    
    if (indentedNewlineString)
        delete [] indentedNewlineString;
}
#endif

//-----------------------------------------------------
// JavaArray
//------------------------------------------------------
// Returns true if the array is an array of primitive elements
bool JavaArray::isPrimitive()
{
  return isPrimitiveKind(((Array *) type)->componentType.typeKind);
}

void *JavaArray::getRaw(Int32 index)
{
    TypeKind tk = ((Array *) type)->componentType.typeKind;
    Int32 size = getTypeKindSize(tk);
  
    return ((char *) this) + arrayEltsOffset(tk) + (index)*size;
}

JavaObject *JavaArray::get(Int32 index)
{
  void *address = getRaw(index);

  if (isPrimitive())
	return &Class::makePrimitiveObject(static_cast<const Array *>(&getType())->componentType.typeKind, address);
  else  /* Array or object type */
	return (JavaObject *) address;
}

#define ARRAY_GET_PRIMITIVE(typeTo, tkTo)\
    if (!isPrimitive()) \
	runtimeError(RuntimeError::illegalArgument);\
\
    typeTo value;\
    void *address = getRaw(index);\
\
    if (((Array *) type)->componentType.typeKind == tkTo)\
      return *(typeTo *) address;\
    else {\
      Field::widen(address, &value, ((Array *) type)->componentType.typeKind, tkTo);\
      return (typeTo) value;\
   }

Int8 JavaArray::getBoolean(Int32 index)
{
  ARRAY_GET_PRIMITIVE(Int8, tkBoolean);
}

char JavaArray::getByte(Int32 index)
{
  ARRAY_GET_PRIMITIVE(Int8, tkByte);
}

Int16 JavaArray::getChar(Int32 index)
{
  ARRAY_GET_PRIMITIVE(Int16, tkChar);
}

Int16 JavaArray::getShort(Int32 index)
{
  ARRAY_GET_PRIMITIVE(Int16, tkShort);
}

Int32 JavaArray::getInt(Int32 index)
{
  ARRAY_GET_PRIMITIVE(Int32, tkInt);
}

Int64 JavaArray::getLong(Int32 index)
{
  ARRAY_GET_PRIMITIVE(Int64, tkLong);
}

Flt32 JavaArray::getFloat(Int32 index)
{
  ARRAY_GET_PRIMITIVE(Flt32, tkFloat);
}

Flt64 JavaArray::getDouble(Int32 index)
{
  ARRAY_GET_PRIMITIVE(Flt64, tkDouble);
}

	
#define ARRAY_SET_PRIMITIVE(typeFrom, tkFrom) \
  if (!isPrimitive())\
	runtimeError(RuntimeError::illegalArgument);\
\
    void *address = getRaw(index);\
\
    if (((Array *) type)->componentType.typeKind == tkFrom)\
	    *(typeFrom *) address = value;\
	else \
        Field::widen(&value, address, tkFrom, ((Array *) type)->componentType.typeKind)

void JavaArray::set(Int32 index, JavaObject *value)
{
    void *address = getRaw(index);
  
    Int32 elementSize = getTypeKindSize(((Array *)type)->componentType.typeKind);
	
	/* If this is a primitive type, then it is encased in a Java Object;
	 * otherwise, it is the object itself
	 */
	if (isPrimitive()) {
		TypeKind tk;

		if (!Standard::isPrimitiveWrapperClass(*static_cast<const Class *>(&value->getType()), &tk))
			runtimeError(RuntimeError::illegalArgument);
		
		memset(address, 0, elementSize);
		
		Int32 argumentSize = getTypeKindSize(tk);
		memcpy(address, (char *)value+sizeof(JavaObject), argumentSize);
	} else {
		assert(elementSize == sizeof(JavaObject *));

		// Ensure that the object is assignable
		if (value && !((Array *)type)->componentType.isAssignableFrom(value->getType()))
			runtimeError(RuntimeError::illegalArgument);

		*((JavaObject **) address) = value;
	}

}

void JavaArray::setBoolean(Int32 index, Int8 value)
{
  ARRAY_SET_PRIMITIVE(Int8, tkBoolean);
}


void JavaArray::setByte(Int32 index, char value)
{
  ARRAY_SET_PRIMITIVE(Int8, tkByte);
}

void JavaArray::setChar(Int32 index, Int16 value)
{
  ARRAY_SET_PRIMITIVE(Int16, tkChar);
}

void JavaArray::setShort(Int32 index, Int16 value)
{
  ARRAY_SET_PRIMITIVE(Int16, tkShort);
}

void JavaArray::setInt(Int32 index, Int32 value)
{
  ARRAY_SET_PRIMITIVE(Int32, tkInt);
}

void JavaArray::setLong(Int32 index, Int64 value)
{
  ARRAY_SET_PRIMITIVE(Int64, tkLong);
}

void JavaArray::setFloat(Int32 index, Flt32 value)
{
  ARRAY_SET_PRIMITIVE(Flt32, tkFloat);
}

void JavaArray::setDouble(Int32 index, Flt64 value)
{
  ARRAY_SET_PRIMITIVE(Flt64, tkDouble);
}


// ----------------------------------------------------------------------------
// ClassOrInterface
ClassCentral *ClassOrInterface::central;

static inline bool isInterface(ClassFileSummary &summ) {
	return ((summ.getAccessFlags() & CR_ACC_INTERFACE) != 0);
}


#ifdef DEBUG_LOG
//
// Print a reference to this class or interface for debugging purposes.
// Return the number of characters printed.
//
int ClassOrInterface::printRef(LogModuleObject &f) const
{
	int o = package.printRef(f);
	if (o)
		o += UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("."));
	return o + UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s", name));
}
#endif


//
// Generate the fully qualified name and the declaration name of the class.
//
void ClassOrInterface::getFullNames() const
{
	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	// The 2 at the end comes from one extra dot, and the NULL character
	Int32 fullNameLen = strlen(package.name) + strlen(name) + 2;
	char *fullNameTemp = new char[fullNameLen+1];

	if (*package.name)
	  PR_snprintf(fullNameTemp, fullNameLen, "%s.%s", package.name, name);
	else
	  PR_snprintf(fullNameTemp, fullNameLen, "%s", name);
	  
	const char *clOrIString;
	if ((summary->getAccessFlags() & CR_ACC_INTERFACE)) 
		clOrIString = "interface ";
	else
		clOrIString = "class ";

	char *declNameTemp = new char[fullNameLen+strlen(clOrIString)+1];

	sprintf(declNameTemp, "%s%s", clOrIString, fullNameTemp);
	
	StringPool &sp = central->getStringPool();
	fullName = sp.intern(fullNameTemp);
	declName = sp.intern(declNameTemp);

	delete [] fullNameTemp;
	delete [] declNameTemp;
}

// Returns fully qualified name of the class/interface in the form 
// "class <classname>" or "interface <interfacename>"
const char *ClassOrInterface::toString() const
{
    // XXX - really should be method on Type, not ClassOrInterface ?
	if (declName)
		return declName;

	getFullNames();
	return declName;
}

// Given a fully qualified className, attempts to load and link the class
// and returns the corresponding Class object. The name could be of
// the form java/lang/Object or java.lang.Object. Makes a copy of the
// _className for further use. Throws a verify error if it could not
// load the class.
Class &Class::forName(const char *_className)
{
	// Make a copy of the string since we will be mutilating it
	char *className = PL_strdup(_className);

	// Replace all dots with slashes 
	for (char *s = className; *s; s++)
		if (*s == '.') *s = '/';

	ClassFileSummary &summ = central->addClass(className);
	return *static_cast<Class *>(summ.getThisClass());
}

Uint32 Class::getPrimitiveValue(JavaObject &arg)
{
  assert(Standard::isPrimitiveWrapperClass(*static_cast<const Class *>(arg.type)));
  
  // This makes assumptions about the layout of the
  // primitive classes -- it assumes that the first
  // instance field is the value of the primitive 
  // type. But this is also the most efficient way
  // to extract the value.
  return *(Uint32 *) ((char *) &arg+sizeof(JavaObject));
}

// Creates an instance of the class if the class is instantiable.
// This method does *not* check that the a java class has access 
// permissions to instantiate this class. That must be done by
// the caller of this method.
JavaObject &Class::newInstance()
{
    if (!summary || ::isInterface(*summary))
	  runtimeError(RuntimeError::illegalAccess);
	
	// Cannot instantiate an abstract class
	if (getModifiers() & CR_ACC_ABSTRACT)
		runtimeError(RuntimeError::notInstantiable);

	// Run static initializers for this class if neccessary
	if (!isInitialized())
		runStaticInitializers();

	int32 memSize = sizeof(JavaObject)+summary->getObjSize();
  
	void *mem = malloc(memSize);
	memset(mem, 0, memSize);
  
	return *new (mem) JavaObject(*summary->getThisClass());
}

// Clones the object given by inObj. The implementation of clone here
// is shallow -- if any instance fields in the object are classes,
// the object references are cloned, but not the objects themselves.
// returns a pointer to the cloned object on success, NULL otherwise
JavaObject *Class::clone(JavaObject &inObj) const
{
    if (!summary || ::isInterface(*summary))
	  runtimeError(RuntimeError::illegalAccess);
	
	/* The types must match in order to be able to clone */
	if (&inObj.getType() != (Type *) this)
	  return 0;

	int32 memSize = sizeof(JavaObject)+summary->getObjSize();
  
	void *mem = malloc(memSize);
	memcpy(mem, &inObj, memSize);
  
	return (JavaObject *) mem;
}

//
// Make a JavaObject corresponding to a primitive type, whose
// raw bytes are located at *address
//
JavaObject &Class::makePrimitiveObject(TypeKind tk, void *address)
{
    assert(isPrimitiveKind(tk));

    StandardClass sc;
    switch (tk) {
    case tkBoolean:
	  sc = cBoolean;
	  break;
    case tkByte:
	  sc = cByte;
	  break;
    case tkChar:
	  sc = cCharacter;
	  break;
    case tkDouble:
	  sc = cDouble;
	  break;
    case tkFloat:
	  sc = cFloat;
	  break;
    case tkInt:
	  sc = cInteger;
	  break;
    case tkLong:
	  sc = cLong;
	  break;
    case tkShort:
	  sc = cShort;
	  break;
    default:
	  runtimeError(RuntimeError::internal);
    }
	
    JavaObject &inst = const_cast<Class *>(&Standard::get(sc))->newInstance();
    int size = getTypeKindSize(tk);

	// FIX Assumes the offset of the "value" field in the class
    memcpy((char *) &inst + sizeof(JavaObject), address, size);
    return inst;
}

// Returns true if the type represented by the specified Class parameter 
// can be converted to the type represented by this class object.
bool Type::isAssignableFrom(const Type &from) const
{
	return implements(from, *this);
}

// Returns the fully qualified name of the class; unlike toString(),
// does not prepend "class " in front of the name
const char *ClassOrInterface::getName() const
{
	if (fullName)
		return fullName;
	
	getFullNames();
	return fullName;
}


// Returns the modifiers (access flags) for this class object.
Uint32 ClassOrInterface::getModifiers() const
{
	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	return summary->getAccessFlags();
}


// Returns the class loader object for this class. 
// ClassLoader getClassLoader();


// Returns summaries for the interfaces of this class.
// If this class object represents a class, returns an array containing 
// objects representing the interfaces directly implemented by the class. 
// If this Class object represents an interface, returns an array
// containing objects representing the direct superinterfaces of the 
// interface. Returns number of array interfaces actually returned.
// This number could be zero, if the class is a primitive type or
// implements no interfaces.
Int32 ClassOrInterface::getInterfaces(Interface **&summaries) const
{
	// FIX Is this correct? All arrays implement cloneable
	if (isArray())
		return 0;

	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	summaries = const_cast<Interface **>(summary->getInterfaces());
	return summary->getInterfaceCount();
}

// If this class or interface is a member of another class, returns the 
// class object representing the class of which it is a member. If this is
// not the case, returns NULL. Hmmm. Need to think about how to implement 
// this.
// Class *getDeclaringClass();

// Returns an array containing class objects representing all the public 
// classes and interfaces that are members of the class represented by this
// class object. Returns number of class objects returned.
Int32 ClassOrInterface::getClasses(Class **&summaries)
{
	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	// Not currently implemented
	trespass("Not implemented");
	summaries = 0;
	return 0;
}

// Returns an array containing information about the public fields of this 
// object. Returns number of fields in object.
Int32 ClassOrInterface::getFields(const Field **&fields)
{
	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	fields = (const Field **) summary->getPublicFields();
	return summary->getPublicFieldCount();
}

// Returns an array containing information about the public methods of 
// this object. Returns number of fields in object.
Int32 ClassOrInterface::getMethods(const Method **&methods)
{
	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	methods = (const Method **) summary->getPublicMethods();
	return summary->getPublicMethodCount();
}

// Returns an array containing information about the public constructors of
// this object. Returns number of constructors.
Int32 ClassOrInterface::getConstructors(const Constructor **&constructors)
{
  if (!summary)
	runtimeError(RuntimeError::illegalAccess);

  constructors = (const Constructor **) summary->getPublicConstructors();
  return summary->getPublicConstructorCount();
}


// Look up fields/methods/constructors. These methods search through 
// publicly accessible fields/methods/constructors.

// Return the Field object for a static/instance public field with the given 
// name.
Field &ClassOrInterface::getField(const char *name)
{
	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	Field *field = summary->getField(name);

	if (!field)
	  runtimeError(RuntimeError::illegalArgument);

	return *field;
}

// Return the Method object for a static/instance public method with the 
// given name. parameterTypes is an array of Type objects
// that represent the parameters of the desired method.
Method &ClassOrInterface::getMethod(const char *name, 
									const Type *parameterTypes[],
									Int32 numParams)
{
	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	Method *method = summary->getMethod(name, parameterTypes, numParams);

	if (!method)
	  runtimeError(RuntimeError::illegalAccess);

	return *method;
}

// Look up a method based on its name and signature. This routine searches
// only in the current class, and does not search in superclasses.
// If publicOrProtected is true, only public and protected methods are searched;
// otherwise, all declared methods are examined.
Method *ClassOrInterface::getMethod(const char *name, const char *sig, 
									bool publicOrProtected)
{
	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	Method *method = summary->getMethod(name, sig);

	if (!method)
		return 0;

	if (publicOrProtected)
		if (!(method->getModifiers() & CR_METHOD_PUBLIC) &&
			!(method->getModifiers() & CR_METHOD_PROTECTED))
			runtimeError(RuntimeError::illegalAccess);
	
	return method;
}

// Return a public constructor that takes the given parameters. 
// parameterTypes is an array of Type objects
// that represent the parameters of the desired constructor.
Constructor &ClassOrInterface::getConstructor(const Type *parameterTypes[],
											  Int32 numParams)
{
  if (!summary)
	runtimeError(RuntimeError::illegalAccess);
  
  Constructor *constructor = summary->getConstructor(parameterTypes, numParams);
  
  if (!constructor)
	runtimeError(RuntimeError::illegalAccess);
  
  return *constructor;

}


// The following functions deal with *all* declared fields, methods, or
// constructors, be they public, protected, or private.
Int32 ClassOrInterface::getDeclaredClasses(Class **&summaries);

// Return all fields declared by this class, public, protected or private.
Int32 ClassOrInterface::getDeclaredFields(const Field **&fields)
{
	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	fields = (const Field **) summary->getFields();
	return summary->getFieldCount();
}

// Get all methods declared by this class, public, protected or private.
Int32 ClassOrInterface::getDeclaredMethods(const Method **&methods)
{
	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	methods = (const Method **) summary->getDeclaredMethods();
	return summary->getDeclaredMethodCount();
}

// Get all constructors declared by this class.
Int32 ClassOrInterface::getDeclaredConstructors(const Constructor **&constructors)
{
  if (!summary)
	runtimeError(RuntimeError::illegalAccess);

  constructors = (const Constructor **) summary->getConstructors();
  return summary->getConstructorCount();
}


// Get the Field object corresponding to a declared field with the given name.
Field &ClassOrInterface::getDeclaredField(const char *name)
{
	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	Field *field = summary->getDeclaredField(name);

	if (!field)
	  runtimeError(RuntimeError::illegalAccess);
	
	return *field;
}

// Get the Method object corresponding to a declared method with the
// given name and signature. parameterTypes is an array of Type objects
// that represent the parameters of the desired method.
Method &ClassOrInterface::getDeclaredMethod(const char *name, 
											const Type *parameterTypes[], 
											Int32 numParams)
{
	if (!summary)
		runtimeError(RuntimeError::illegalAccess);

	Method *method = summary->getDeclaredMethod(name, parameterTypes, numParams);

	if (!method)
	  runtimeError(RuntimeError::illegalAccess);
	
	return *method;
}

// Get the Constructor object for a constructor that has the signature given
// by parameterTypes. parameterTypes is an array of Type objects
// that represent the parameters of the desired method.
Constructor &ClassOrInterface::getDeclaredConstructor(const Type *parameterTypes[],
													  Int32 numParams)
{
  if (!summary)
	runtimeError(RuntimeError::illegalAccess);

  Constructor *constructor = summary->getDeclaredConstructor(parameterTypes, numParams);

  if (!constructor)
	runtimeError(RuntimeError::illegalAccess);
  
  return *constructor;
}


// ----------------------------------------------------------------------------
// Class


//
// Initialize the fields of a Class object.  This constructor may only be called
// from within Class::make because the Class object must be specially allocated to
// allow room for its vtable.
//
inline Class::Class(Package &package, const char *name, const Class *parent, 
					Uint32 nInterfaces, Interface **interfaces, ClassFileSummary *summary,
					bool final, Uint32 instanceSize, Uint32 nVTableSlots,
                    VTableOffset *interfaceVOffsetTable, Uint16 *interfaceAssignableTable):
	ClassOrInterface(Standard::get(cClass), tkObject, parent, final, nVTableSlots, package, name, 
					 nInterfaces, interfaces, summary, interfaceVOffsetTable,
                     interfaceAssignableTable),
	instanceSize(instanceSize) { }


#ifdef _WIN32
 #pragma warning(disable: 4355)
#endif
//
// Initialize the fields of a special Class object.  This constructor may only be called
// from within Class::makeSpecial because the Class object must be specially allocated to
// allow room for its vtable.
//
// Each special class object is located in the pkgInternal package, has no parent or interfaces,
// and is final.  The first class object created should be the cClass special class, and for
// that class only isOwnClass should be true.  Every other class object created relies on
// cClass already being set up.
//
// The Standard class should fix up the parents of the special classes it creates after it
// loads the Object class.
//
inline Class::Class(bool isOwnClass, const char *name, Uint32 instanceSize, Uint32 nVTableSlots):
	ClassOrInterface(isOwnClass ? *this : Standard::get(cClass),
					 tkObject, NULL, true, nVTableSlots, pkgInternal, name, 0, 0, 0),
	instanceSize(instanceSize)
{}
#ifdef _WIN32
 #pragma warning(default: 4355)
#endif


//
// Initialize a new Class object that describes essential information such as layout
// and inheritance relationships of objects of a single Java class.
//
// package is the package that defined this class.
// name is the unqualified name of this class.
// parent is the parent class or nil if this is the Class for the Object class.
// interfaces is an array of nInterfaces elements that lists all interfaces from which
//   this class derives except that the interfaces array does not include interfaces
//   from which the parent class derives.
// extraSize is the number of additional bytes taken by fields of this class and should
//   be a multiple of 4.
// nExtraVTableSlots is the number of additional virtual methods that this class declares;
//   it includes the one vtable slot used for class membership testing.
//
Class &Class::make(Package &package, const char *name, const Class *parent, Uint32 nInterfaces, Interface **interfaces,
				   ClassFileSummary *summary,
				   bool final, Uint32 extraSize, Uint32 nVTableSlots,
                   VTableOffset *interfaceVOffsetTable,
                   Uint16 *interfaceAssignableTable)
{
	if (parent) {
		assert(!parent->final);
		//extraSize += parent->instanceSize;
	}
	Int32 objSize = sizeof(Class) + nVTableSlots*sizeof(VTableEntry);
	void *mem = malloc(objSize);
	memset(mem, 0, objSize);  // Set vtable entries to zero
	return *new(mem) Class(package, name, parent, nInterfaces, 
								  interfaces, 
								  summary,
								  final, extraSize, nVTableSlots,
								  interfaceVOffsetTable, interfaceAssignableTable);
	 
}


//
// Initialize a new special Class object.  This routine is only be called
// from within Standard to bootstrap a few special classes.
//
// Each special class object is located in the pkgInternal package, has no parent or interfaces,
// and is final.  The first class object created should be the cClass special class, and for
// that class only isOwnClass should be true.  Every other class object created relies on
// cClass already being set up.
//
// The Standard class should fix up the parents of the special classes it creates after it
// loads the Object class.
//
// name is the unqualified name of this class.
// instanceSize is the TOTAL number of bytes taken by instances of this class and
//   should be a multiple of 4.
// nVTableSlots is the TOTAL number of virtual methods that this class declares, including
//   superclasses and vtable slots used for class membership testing.
//
Class &Class::makeSpecial(bool isOwnClass, const char *name, Uint32 instanceSize, Uint32 nVTableSlots)
{
	void *mem = malloc(sizeof(Class) + nVTableSlots*sizeof(VTableEntry));
	return *new(mem) Class(isOwnClass, name, instanceSize, nVTableSlots);
}


//
// Fix up the parent for a class created with makeSpecial.  This routine is only be called
// from within Standard to bootstrap a few special classes.
//
inline void Class::setParent(const Class &p)
{
	assert(!superType && p.nVTableSlots < nVTableSlots && p.instanceSize <= instanceSize);
	superType = &p;
}


//
// Return true if this class implements class c.
// (ie "is subclass of")
//
bool Class::implements(const Class &c) const
{
	const Class *a = this;
	do {
		if (a == &c)
			return true;
		a = a->getParent();
	} while (a);
	return false;
}


//
// Return true if this class implements interface i.
//
bool Class::implements(const Interface &i) const
{
	const Class *a = this;
	do {
		Uint32 n = a->nInterfaces;
		Interface *const*intfs = a->interfaces;
		while (n--) {
			if ((*intfs)->implements(i))
				return true;
			intfs++;
		}
		a = a->getParent();
	} while (a);
	return false;
}



static const char *forbiddenClasses[] = {
  //"System",                // references System.securityManager which has longs
  "CharToByteConverter",   // references System.getProperty, has finally
  //"Math",                  // Has float and double
  //"Float",
  //"Double"
};

static const numForbiddenClasses = sizeof(forbiddenClasses)/sizeof(forbiddenClasses[0]);

/* Return true if we should not execute the static initializer for this class.
 * This is a temporary kludge to shut out classes whose static initializers 
 * reference long's and float's. Eventualy, once codegen catches up, this
 * should go away.
 */
static bool forbidden(const char *className) 
{
  for (int i = 0; i < numForbiddenClasses; i++)
	if (!PL_strcmp(forbiddenClasses[i], className))
	  return true;

	return false;
}

void ClassOrInterface::runStaticInitializers()
{
  if (initialized)
	return;

  /* Eventually, we also need to have an initializing, but this will do
   * for now
   */
  initialized = true;

  /* Don't compile initializers unless we're executing */
  if (VM::theVM.getCompileStage() != csGenInstructions ||
	  VM::theVM.getNoInvoke())
	return;

  /* KLUDGE ALERT Because codegen currently does not do longs, floats and
   * doubles, we disable static initializers for those classes that
   * use longs, floats and doubles. This should go away once codegen 
   * catches up.
   */
  if (forbidden(name))
	return;

  /* Let's initially do this the easy way. Linearly search through the
   * method array for static initializers
   */
  Int16 methodCount = summary->getMethodCount();
  const MethodInfo **methodInfos = summary->getMethodInfo();

  extern StringPool sp;

  const char *clinitp = sp.get("<clinit>");
  const char *sigp = sp.get("()V");

  for (Int16 i = 0; i < methodCount; i++) {
	if (methodInfos[i]->getName()->getUtfString() == clinitp &&
		methodInfos[i]->getDescriptor()->getUtfString() == sigp) {
	  Method &m = summary->getMethod(i);

	  m.compile();
	  m.invoke(0, 0, 0);
	}
  }

}

#ifdef DEBUG_LOG
void Class::printValue(LogModuleObject &f, JavaObject *obj, int maxRecursion, Uint32 printFlags,
                       const char *newlineString,
                       Vector<JavaObject *> *printedObjects)
{
    const Field **fields;
    
    char *indentedNewlineString = NULL;  
    if (newlineString) {
        indentedNewlineString = new char[strlen(newlineString) + 3];
        strcpy(indentedNewlineString, newlineString);
        strcat(indentedNewlineString, "  ");
    }
    
    if (newlineString)
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s", newlineString));
    
    int numFields = getDeclaredFields(fields);
    UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("{"));
    
    int fieldNum;
    bool fieldPrinted = false;
    for (fieldNum = 0; fieldNum < numFields; fieldNum++) {
        Uint8 *fieldPtr;
        const Field *field = fields[fieldNum];
        int modifiers = field->getModifiers();
        
        if (modifiers & CR_FIELD_STATIC) {
            if (!(printFlags & PRINT_STATIC_FIELDS))
                continue;
            fieldPtr = (Uint8*)addressFunction(field->getAddress());
        } else {
            fieldPtr = (Uint8*)obj + field->getOffset();
        }

        if (newlineString)
            UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s", newlineString));
        else if (fieldPrinted)
            UT_OBJECTLOG(f, PR_LOG_ALWAYS, (","));
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" %s:", field->getName()));
        

        ::printValue(f, fieldPtr, field->getType(), maxRecursion - 1, printFlags,
                     indentedNewlineString, printedObjects);
        fieldPrinted = true;
    }

    if (newlineString)
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s", newlineString));
    UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("}"));
    
    if (indentedNewlineString)
        delete [] indentedNewlineString;
}

#endif

// ----------------------------------------------------------------------------
// Interface

//
// Return true if this interface implements interface i.
//
bool Interface::implements(const Interface &i) const
{
	Uint32 n = nInterfaces;
	Interface *const*intfs = interfaces;
    if (this == &i)
        return true;
	while (n--) {
		if ((*intfs)->implements(i))
			return true;
		intfs++;
	}
	return false;
}


// ----------------------------------------------------------------------------

const Class *Standard::standardClasses[nStandardClasses];
const Interface *Standard::standardInterfaces[nStandardInterfaces];
const JavaObject *Standard::standardObjects[nStandardObjects];

static const char *const standardClassNames[nStandardClasses + 1 - firstOrdinaryStandardClass] = {
	"java/lang/Throwable",						// cThrowable
	"java/lang/Exception",						// cException
	"java/lang/RuntimeException",				// cRuntimeException
	"java/lang/ArithmeticException",			// cArithmeticException
	"java/lang/ArrayStoreException",			// cArrayStoreException
	"java/lang/ClassCastException",				// cClassCastException
	"java/lang/IllegalMonitorStateException",	// cIllegalMonitorStateException
	"java/lang/ArrayIndexOutOfBoundsException",	// cArrayIndexOutOfBoundsException
	"java/lang/NegativeArraySizeException",		// cNegativeArraySizeException
	"java/lang/NullPointerException",			// cNullPointerException
	"java/lang/Error",							// cError
	"java/lang/ThreadDeath",					// cThreadDeath
	"java/lang/Boolean",						// cBoolean
	"java/lang/Byte",							// cByte
	"java/lang/Character",						// cCharacter
	"java/lang/Short",							// cShort
	"java/lang/Integer",						// cInteger
	"java/lang/Long",							// cLong
	"java/lang/Float",							// cFloat
	"java/lang/Double",							// cDouble
    "java/lang/String",                         // cString
	"java/lang/InterruptedException"            // cInterruptedException
};

static const char *const standardInterfaceNames[nStandardInterfaces] = {
	"java/lang/Cloneable"						// iCloneable
};

static const StandardClass standardObjectClasses[nStandardObjects] = {
	cArithmeticException,			// oArithmeticException
	cArrayStoreException,			// oArrayStoreException
	cClassCastException,			// oClassCastException
	cIllegalMonitorStateException,	// oIllegalMonitorStateException
	cArrayIndexOutOfBoundsException,// oArrayIndexOutOfBoundsException
	cNegativeArraySizeException,	// oNegativeArraySizeException
	cNullPointerException			// oNullPointerException
};

// Total number of vtable entries of any object whose class is Class.
const nClassClassVTableEntries = 20;


//
// Initialize the standard classes and objects.
// This routine must be called before any of these are referenced.
//
void Standard::initStandardObjects(ClassCentral &c)
{
	ClassOrInterface::setCentral(&c);

    // Array vtables will get clobbered if this assertion fails.
    assert(sizeof(Class) >= sizeof(Array));

	// Initially, set standardClasses[cClass] to a special class so that we
	// can safely load java/lang/Class. 
	standardClasses[cClass] = &Class::makeSpecial(true, "Class", sizeof(Class), nClassClassVTableEntries);

	// Set standardClasses[cField], standardClasses[cMethod], and
	// standardClasses[cConstructor]
	standardClasses[cField] = &Class::makeSpecial(false, "Field", 
												  sizeof(Class),
												  nClassClassVTableEntries);

	standardClasses[cMethod] = &Class::makeSpecial(false, "Method", 
												   sizeof(Class),
												   nClassClassVTableEntries);

	standardClasses[cConstructor] = &Class::makeSpecial(false, "Constructor", 
												   sizeof(Class),
												   nClassClassVTableEntries);

    // XXX - Disabled until Waldemar tells us what these meta-classes are for
	standardClasses[cPrimitiveType] = &Class::makeSpecial(false, "PrimitiveType", sizeof(PrimitiveType), nClassClassVTableEntries);
	standardClasses[cPrimitiveArray] = &Class::makeSpecial(false, "PrimitiveArray", sizeof(Array), nClassClassVTableEntries);
	standardClasses[cObjectArray] = &Class::makeSpecial(false, "ObjectArray", sizeof(Array), nClassClassVTableEntries);
	standardClasses[cInterface] = &Class::makeSpecial(false, "Interface", sizeof(Interface), nClassClassVTableEntries);

	const Class &objectClass = asClass(*c.addClass("java/lang/Object").getThisClass());
	standardClasses[cObject] = &objectClass;
	
	uint i;
	for (i = cObject + 1; i != firstOrdinaryStandardClass; i++)
	  const_cast<Class *>(standardClasses[i])->setParent(objectClass);
	

	// Now load java/lang/Class and patch standardClasses[cClass] to be the newly
	// loaded class. This way all classes subsequently created will get
	// java/lang/Class as the metaclass.
	const Class &classClass = asClass(*c.addClass("java/lang/Class").getThisClass());
	standardClasses[cClass] = &classClass;

	// Load classes Field, Method and constructor. 
	// FIX Need to fix the metaclasses of fields and methods of these classes
	// to point to Field, Method, or Constructor.
	const Class &classField = asClass(*c.addClass("java/lang/reflect/Field").getThisClass());
	standardClasses[cField] = &classField;

	const Class &classMethod = asClass(*c.addClass("java/lang/reflect/Method").getThisClass());
	standardClasses[cMethod] = &classMethod;

	const Class &classConstructor = asClass(*c.addClass("java/lang/reflect/Constructor").getThisClass());
	standardClasses[cConstructor] = &classConstructor;

    // Fix up the metaclass of classClass and objectClass
    const_cast<Class *>(&objectClass)->type = &asType(classClass);
    const_cast<Class *>(&classClass)->type = &asType(classClass);

	PrimitiveType::staticInit();

    const char *const* pName = standardClassNames;
	for (; i != nStandardClasses; i++)
	    standardClasses[i] = &asClass(*c.addClass(*pName++).getThisClass());

	pName = standardInterfaceNames;
	for (i = 0; i != nStandardInterfaces; i++)
		standardInterfaces[i] = &asInterface(*c.addClass(*pName++).getThisClass());

	const StandardClass *pSC = standardObjectClasses;
	for (i = 0; i != nStandardObjects; i++)
		standardObjects[i] = new JavaObject(get(*pSC++));
  
    Array::staticInit();


}

// Returns true if clazz represents one of the primitive classes:
// java/lang/Byte, Boolean, Character, Short, Integer, Long, Float
// and Double. If tkIn is non-null, returns the typeKind of the 
// primitive class on success. 
bool Standard::isPrimitiveWrapperClass(const Class &clazz, TypeKind *tkIn)
{
	TypeKind tk;
	bool isWrapper = true;

	if (&clazz == &Standard::get(cInteger))
		tk = tkInt;
	else if (&clazz == &Standard::get(cShort))
		tk = tkShort;
	else if (&clazz == &Standard::get(cBoolean))
		tk = tkBoolean;
	else if (&clazz == &Standard::get(cByte))
		tk = tkByte;
	else if (&clazz == &Standard::get(cCharacter))
		tk = tkChar;
	else if (&clazz == &Standard::get(cLong))
		tk = tkLong;
	else if (&clazz == &Standard::get(cFloat))
		tk = tkFloat;
	else if (&clazz == &Standard::get(cDouble))
		tk = tkDouble;
	else
		return false;

	if (tkIn)
		*tkIn = tk;

	return true;
}

// Returns true if clazz represents one of the Double-word
// primitive classes -- java/lang/Long or Double.
bool Standard::isDoubleWordPrimitiveWrapperClass(const Class &clazz)
{
  return (&clazz == &Standard::get(cLong) ||
		  &clazz == &Standard::get(cDouble));
}


#ifdef DEBUG

void printValue(LogModuleObject &f, void *valPtr, const Type &superType, int maxRecursionLevel,
                Uint32 printFlags, const char *newlineString,
                Vector<JavaObject *> *printedObjects)

{
    char *str = NULL;
    JavaObject *obj = NULL;

    switch(superType.typeKind) {

     case tkByte:
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("0x%02x", *((Int32*)valPtr)));
        break;
     
     case tkChar:
        {
            Uint16 charVal = *((Uint32*)valPtr);
            if (charVal < 0x7f)
                UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("'%c'", charVal));
            else
                UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("0x%04x", charVal));
        }
        break;

    case tkShort:
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("0x%04x", *((Int32*)valPtr)));
        break;

    case tkInt:
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%d", *((Int32*)valPtr)));
        break;
      
    case tkInterface:
    case tkObject:
    case tkArray:
        {
            obj = *((JavaObject**)valPtr);
            if (!obj) {
                UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("null"));
                break;
            }
            const Type &objType = obj->getType();
            assert(implements(objType, superType));
            if (&objType == &asType(Standard::get(cString))) {
                JavaString *javaString = &asJavaString(*obj);
                str = javaString->convertUtf();
                UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\"%s\"", str));
                delete str;
                break;
            }
            
            if (maxRecursionLevel > 0) {
                obj->printValue(f, maxRecursionLevel - 1, printFlags, newlineString, printedObjects);
            } else {
                if (printFlags & PRINT_OBJECT_TYPE) {
                    objType.printRef(f);
                    UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" "));
                }
                UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("0x%010x", *((Uint32**)valPtr)));
            }
        }
        break;
        
    case tkDouble:
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%f", *((Flt64*)valPtr)));
        break;
        
    case tkFloat:
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%f", (Flt64)(*((Flt32*)valPtr))));
        break;
        
    case tkLong:
        str = PR_smprintf("%lld", *((Uint64*)valPtr));
        if (!str)
            return;
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s", str));
#if 0
        PR_smprintf_free(str);  // XXX - Should be PR_smprintf_free() when that function is implemented
#endif
        break;
        
    case tkBoolean:
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s", *((Uint32*)valPtr) ? "true" : "false"));
        break;
        
    default:
        assert(0);
        UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("don't know yet"));
        break;
    }
}

void JavaObject::printValue(LogModuleObject &f, int maxRecursion, Uint32 printFlags,
                            const char *newlineString,
                            Vector<JavaObject *> *printedObjects)
{ 
    bool deletePrintedObjects = false;

    // Prevent infinite loops due to circular object references     
    if (printedObjects) {
        int i;
        int numObjectsPrinted = printedObjects->size();
        for (i = 0; i < numObjectsPrinted; i++) {
            if ((*printedObjects)[i] == this) {
                UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("$%d", i));
                return;
            }
        }
    } else if (maxRecursion > 0) {
        // Vector<JavaObject *> *printedObjects = new Vector<JavaObject *>;
        deletePrintedObjects = true;
    }

    // An object reference is either an Array or an Object.  (An interface is a type, but
    //  you can never have an instance of an interface.)
    switch(getType().typeKind) {
    case tkObject:
        {
            Class *clazz = const_cast<Class *>(&asClass(getType()));
            clazz->printValue(f, this, maxRecursion, printFlags, newlineString, printedObjects);
        }
        break;

    case tkArray:
        asArray(getType()).printValue(f, (JavaArray*)this, maxRecursion, printFlags, newlineString, printedObjects);
        break;

    default:
        assert(0);
    }
    
    if (deletePrintedObjects)
        delete printedObjects;
}

UT_DEFINE_LOG_MODULE(JavaObjectDump);

void dump(JavaObject *obj, int maxRecursion, Uint32 printFlags, const char *newlineString)
{
    Vector<JavaObject *> printedObjects;
    obj->printValue(UT_LOG_MODULE(JavaObjectDump), maxRecursion, printFlags, newlineString, &printedObjects);
}

#endif




