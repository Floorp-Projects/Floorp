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

#ifndef JAVAOBJECT_H
#define JAVAOBJECT_H

#include "Fundamentals.h"


// A TypeKind describes the basic kind of a Java value.
// See also Value.h.
enum TypeKind
{						// Size	  Primitive kind?	 Final?
	tkVoid,				//  0			yes			  yes
	tkBoolean,			//  1			yes			  yes
	tkUByte,			//  1			yes			  yes
	tkByte,				//  1			yes			  yes
	tkChar,				//  2			yes			  yes
	tkShort,			//  2			yes			  yes
	tkInt,				//  4			yes			  yes
	tkLong,				//  8			yes			  yes
	tkFloat,			//  4			yes			  yes
	tkDouble,			//  8			yes			  yes
	tkObject,			//  4			no			sometimes		// Java objects
	tkSpecial,			//  4			no			  yes			// Internal, non-java objects; not used at this time
	tkArray,			//  4			no			sometimes		// Java arrays
	tkInterface			//  4			no			  no			// Java interfaces
};
const nPrimitiveTypeKinds = tkDouble + 1;
const nTypeKinds = tkInterface + 1;

inline bool isPrimitiveKind(TypeKind tk) {return tk <= tkDouble;}
inline bool isNonVoidPrimitiveKind(TypeKind tk) {return tk >= tkBoolean && tk <= tkDouble;}
inline bool isDoublewordKind(TypeKind tk) {return tk == tkLong || tk == tkDouble;}
inline int getTypeKindSize(TypeKind tk);
inline int getLgTypeKindSize(TypeKind tk);
inline int nTypeKindSlots(TypeKind tk);
inline bool getTypeKindFromDescriptor(char c, TypeKind &tk);

// ----------------------------------------------------------------------------

struct Type;
struct Class;
struct Interface;
struct Package;

struct Field;
struct Method;
struct Constructor;


// A JavaObject is the header of any Java object.
// It contains just one field -- a pointer to the object's type.
// An actual Java object's fields follow after the end of the JavaEnd structure.
struct JavaObject
{
    const Type &type;							// Pointer to the object's type
  
    explicit JavaObject(const Type &type): type(type) {}
    const Class &getClass() const;
};
const int32 objectTypeOffset = 0;				// Offset from object address to its type pointer word

typedef JavaObject *obj;
typedef const JavaObject *cobj;

inline ptr objToPtr(obj o) {return reinterpret_cast<ptr>(o);}
inline cptr objToPtr(cobj o) {return reinterpret_cast<cptr>(o);}

inline obj ptrToObj(ptr o) {return reinterpret_cast<obj>(o);}
inline cobj ptrToObj(cptr o) {return reinterpret_cast<cobj>(o);}


// ----------------------------------------------------------------------------

// A JavaArray is the header of any Java array.
// It contains only two field -- a pointer to the object's type and a
// number of elements.
// An actual Java array's elements follow after the end of the JavaArray structure.
struct JavaArray: JavaObject
{
	const uint32 length;						// Number of elements in this array

	JavaArray(Type &type, uint32 length): JavaObject(type), length(length) {}
};

const int32 arrayLengthOffset = offsetof(JavaArray, length);	// Offset from array address to its length word
const int32 arrayObjectEltsOffset = sizeof(JavaArray);			// Offset from array-of-objects address to its first element


// ----------------------------------------------------------------------------

struct Array;
// A Type describes the contents of a Java object.
// It's also itself a special kind of Java object.
struct Type: JavaObject
{
    const TypeKind typeKind ENUM_8;				// Kind of objects described here
	const bool final BOOL_8;					// True if this type cannot have any subtypes.  Note that, by this
												//   definition, an array type is final if its element type is final.
	const uint32 nVTableSlots;					// Number of vtable slots for this class; zero if there is no vtable
  private:
	mutable const Array *arrayType;				// Type of arrays whose elements have this Type or null if none declared yet

protected: // This is an abstract class
    Type(const Type &metaType, TypeKind typeKind, bool final, uint32 nVTableSlots):
	JavaObject(metaType), typeKind(typeKind), final(final), nVTableSlots(nVTableSlots), arrayType(0) {}

public:
	const Array &getArrayType() const;
};

inline Type &asType(JavaObject &o);
bool implements(const Type &sub, const Type &super);


// A Type for void, boolean, ubyte, byte, char, short, int, long, float, or double.
struct PrimitiveType: Type
{
  private:
	static const PrimitiveType *primitiveTypes[nPrimitiveTypeKinds];

	// Only initPrimitiveTypes can construct PrimitiveType instances.
    PrimitiveType(const Type &metaType, TypeKind typeKind): Type(metaType, typeKind, true, 0) { }

  public:
	static const PrimitiveType &obtain(TypeKind tk);

	static void staticInit();
};


// A Type for any array.
struct Array: Type
{
	const Type &elementType;					// Type of elements of this array

  private:
	static const Array *primitiveArrays[nPrimitiveTypeKinds];
  public:

	explicit Array(const Type &elementType);
	static const Array &obtain(TypeKind elementKind);

	static void staticInit();
};


// ----------------------------------------------------------------------------


// An abstract class that contains information common to Class and
// Interface objects.
struct ClassOrInterface: Type
{
    friend class ClassFileSummary;

	Package &package;							// The package that defines this class or interface
    const char *const name;						// Unqualified name of the class or interface (stored in the ClassWorld's pool) 
	const uint32 nInterfaces;     					// Number of interfaces implemented directly by this class or interface
	Interface **interfaces;      	  			// Array of nInterfaces implemented directly by this class or interface
  //const char *const fullName;					// Fully qualified name of the class or interface
    const uint32 nFields;
    Field **fields;
    const uint32 nMethods;
    Method **methods;
    
  protected:
    ClassOrInterface(const Type &metaType, TypeKind typeKind, bool final, 
					 uint32 nVTableSlots, Package &package,
					 const char *name, uint32 nInterfaces, 
					 Interface **interfaces,
					 uint32 nFields, Field **fields,
					 uint32 nMethods, Method **methods);
  
  public:
  #ifdef DEBUG
	int printRef(FILE *f) const;
  #endif

	// Returns fully qualified name of the class in the form 
	// "class <classname>"
	const char *toString(); 

	// Given a fully qualified className, attempts to load and link the class
	// and returns the corresponding Class object.
	static Class &forName(const char *className);

	// Creates an instance of the class if the class is instantiable.
	JavaObject &newInstance();

	// Returns true if given Object is an instance of the class represented
	// by this class object.
	bool isInstance(JavaObject &obj) const;

	// Returns true if the type represented by the specified Class parameter 
	// can be converted to the type represented by this class object.
	bool isAssignableFrom(Class &from) const;

	bool isInterface() const;
	bool isArray() const;
	bool isPrimitive() const;  // Returns true if this class is a primitive type

	// Returns the fully qualified name of the class; unlike toString(),
	// does not prepend "class " in front of the name
	const char *getName() const;

	// Returns the modifiers (access flags) for this class object.
	int getModifiers() const;

	// Returns the class loader object for this class. Need to think about
	// this a little bit.
	// ClassLoader getClassLoader();

	// Returns the class object for the super class of this class. Returns
	// NULL if this is a primitive type or java/lang/Object
	Class *getSuperClass() const;

	// Returns summaries for the interfaces of this class.
	// If this class object represents a class, returns an array containing 
	// objects representing the interfaces directly implemented by the class. 
	// If this Class object represents an interface, returns an array
	// containing objects representing the direct superinterfaces of the 
	// interface. Returns number of array interfaces actually returned.
	// This number could be zero, if the class is a primitive type or
	// implements no interfaces.
	int getInterfaces(Class *&summaries);

	// If this Class object represents an array type, returns the class 
	// object representing the component type of the array; 
	// otherwise returns NULL.
	Class *getComponentType();

	// If this class or interface is a member of another class, returns the 
	// class object representing the class of which it is a member. If this is
	// not the case, returns NULL. Hmmm. Need to think about how to implement 
	// this.
	// Class *getDeclaringClass();

	// Returns an array containing class objects representing all the public 
	// classes and interfaces that are members of the class represented by this
	// class object. Returns number of class objects returned.
	int getClasses(Class *&summaries);

	// Returns an array containing information about the public fields of this 
	// object. Returns number of fields in object.
	int getFields(const Field *&fields);

	// Returns an array containing information about the public methods of 
	// this object. Returns number of fields in object.
	int getMethods(const Method *&methods);

	// Returns an array containing information about the public constructors of
	// this object. Returns number of constructors.
	int getConstructors(const Constructor *&constructors);

	// Look up fields/methods/constructors. These methods search through 
	// publicly accessible fields/methods/constructors.
	Field &getField(const char *name);
	Method &getMethod(const char *name, Class *parameterTypes[],
					  int numParams);
	Constructor &getConstructor(Class *parameterTypes[],
								int numParams);

	// The following functions deal with *all* declared fields, methods, or
	// constructors, be they public, protected, or private.
	int getDeclaredClasses(Class *&summaries);
	int getDeclaredFields(const Field *&fields) const;
	int getDeclaredMethods(const Method *&methods) const;
	int getDeclaredConstructors(const Constructor *&constructors) const;

	Field &getDeclaredField(const char *name);
	Method &getDeclaredMethod(const char *name, Class parameterTypes[], int numParams);
	Constructor &getDeclaredConstructor(Class parameterTypes[], int numParams);
};


// A vtable contains two kinds of entries:
//   Method pointers that point to code (either actual code or stubs that
//     load the actual code);
//   Class pointers that point to superclasses.
//
// The method pointers are used for dispatching methods inherited from
// superclasses.  The Class pointers are used for quickly determining
// whether an object belongs to a particular class.
union VTableEntry
{
	void *codePtr;								// Pointer to method
	Class *classPtr;							// Pointer to superclass
};


// Runtime reflection information about a class, such as its inheritance
// hierarchy, fields and field layout, etc.
// A Class remains in memory as long as a class is alive.  It does
// not keep track of a class's constant pool, etc.
struct Class: ClassOrInterface
{
    friend class ClassFileSummary;

	const Class *parent;   						// The superclass of this class (nil if this is the class Object)
	uint32 instanceSize;					   // Size of instance objects in bytes
  
  private:
	Class(Package &package, const char *name, const Class *parent, uint32 nInterfaces, Interface **interfaces,
		  uint32 nFields, Field **fields,
		  uint32 nMethods, Method **methods,
		  bool final, uint32 instanceSize, uint32 nVTableSlots);

	void setSuper(const Class *p) {parent = p;}
    void setSize(uint32 size) {instanceSize = size;}

  public:
	static Class &make(Package &package, const char *name, 
					   const Class *parent, uint32 nInterfaces, 
					   Interface **interfaces,
					   uint32 nFields, Field **fields,
					   uint32 nMethods, Method **methods,
					   bool final, uint32 extraSize, uint32 nExtraVTableSlots);

	bool implements(const Class &c) const;
	bool implements(const Interface &i) const;

	VTableEntry &getVTableEntry(uint32 vIndex);
};

const uint32 vTableOffset = sizeof(Class);		// Offset from a Type to beginning of its vtable
inline int32 vTableIndexToOffset(uint32 vIndex) {return (int32)(vTableOffset + vIndex*sizeof(VTableEntry));}


struct Interface: ClassOrInterface
{
	Interface(Package &package, const char *name, uint32 nInterfaces, 
			  Interface **interfaces, uint32 nFields, Field **fields, 
			  uint32 nMethods, Method **methods);

	bool implements(const Interface &i) const;

	// Other methods to inquire about this interface
};


// ----------------------------------------------------------------------------
// Distinguished objects

// Distinguished classes
extern const Class *cObject;
extern const Class *cThrowable;
extern const Class *cException;
extern const Class *cRuntimeException;
extern const Class *cArithmeticException;
extern const Class *cArrayStoreException;
extern const Class *cClassCastException;
extern const Class *cIndexOutOfBoundsException;
extern const Class *cArrayIndexOutOfBoundsException;
extern const Class *cNegativeArraySizeException;
extern const Class *cNullPointerException;

// Internal classes
extern const Class *cPrimitiveType;
extern const Class *cPrimitiveArray;
extern const Class *cObjectArray;
extern const Class *cClass;
extern const Class *cInterface;

// Distinguished interfaces
extern const Interface *iCloneable;

// Distinguished objects
extern const JavaObject *oArithmeticException;
extern const JavaObject *oArrayStoreException;
extern const JavaObject *oClassCastException;
extern const JavaObject *oArrayIndexOutOfBoundsException;
extern const JavaObject *oNegativeArraySizeException;
extern const JavaObject *oNullPointerException;

class ClassCentral;
void initDistinguishedObjects(ClassCentral &c);
void initDistinguishedObjectsOld();  // Soon to be obsolete

// --- INLINES ----------------------------------------------------------------


extern const int8 typeKindSizes[nTypeKinds];
//
// Return the number of bytes taken by a value that has the given TypeKind.
//
inline int getTypeKindSize(TypeKind tk)
{
	assert((uint)tk < nTypeKinds);
	return typeKindSizes[tk];
}


extern const int8 lgTypeKindSizes[nTypeKinds];
//
// Return the base-2 logarithm of the number of bytes taken by a value
// that has the given TypeKind.
//
inline int getLgTypeKindSize(TypeKind tk)
{
	assert((uint)tk < nTypeKinds);
	return lgTypeKindSizes[tk];
}


extern const int8 typeKindNSlots[nTypeKinds];
//
// Return the number of environment slots taken by a value that has the
// given TypeKind.
//
inline int nTypeKindSlots(TypeKind tk)
{
	assert((uint)tk < nTypeKinds);
	return typeKindNSlots[tk];
}

//
// If c is the descriptor for a primitive type, sets tk to the
// typeKind of c and returns true. Else, returns false.
//
inline bool getTypeKindFromDescriptor(char c, TypeKind &tk)
{
  bool ret = true;

  switch (c) {
  case 'B':
	tk = tkByte;
	break;

  case 'C':
	tk = tkChar;
	break;

  case 'D':
	tk = tkDouble;
	break;

  case 'F':
	tk = tkFloat;
	break;

  case 'I':
	tk = tkInt;
	break;
	
  case 'J':
	tk = tkLong;
	break;

  case 'S':
	tk = tkShort;
	break;

  case 'Z':
	tk = tkBoolean;
	break;

  default:
	ret = false;	
  }

  return ret;
}


// ----------------------------------------------------------------------------


//
// Return the type of arrays whose elements have this Type.
//
inline const Array &Type::getArrayType() const
{
	if (arrayType)
		return *arrayType;
	arrayType = new Array(*this);
	return *arrayType;
}

//
// Assert that this JavaObject is really a Type and return that Type.
//
inline Type &asType(JavaObject &o)
{
	assert(&o.type &&
		   (&o.type == cPrimitiveType || &o.type == cPrimitiveArray || &o.type == cObjectArray ||
			&o.type == cClass || &o.type == cInterface));
	return *static_cast<Type *>(&o);
}


//
// Return the primitive type corresponding to the given TypeKind.
//
inline const PrimitiveType &PrimitiveType::obtain(TypeKind tk)
{
    assert(isPrimitiveKind(tk) && primitiveTypes[tk]);
    return *primitiveTypes[tk];
}


//
// Return a PrimitiveArray corresponding to the given TypeKind.
//
inline const Array &Array::obtain(TypeKind elementKind)
{
	assert(isNonVoidPrimitiveKind(elementKind) && primitiveArrays[elementKind]);
	return *primitiveArrays[elementKind];
}


// ----------------------------------------------------------------------------


//
// Return the class of a Java object.  The class of an array is Object.
//
inline const Class &JavaObject::getClass() const
{
	return type.typeKind == tkArray ? *cObject : *static_cast<const Class *>(&type);
}


//
// Initialize a new ClassOrInterface object.  metaType is the type of
// the ClassOrInterface object -- either cClass or cInterface.
// size is the size of instances of this class; size is zero if this is Interface
// instead of a Class.
// package is the package that defined this class or interface, name is the unqualified
// name of this class or interface, and interfaces is an array of nInterfaces elements
// that lists all interfaces from which this class or interface derives except that,
// if this is a Class, the interfaces array does not include interfaces from which
// the parent class derives.
//
inline ClassOrInterface::ClassOrInterface(const Type &metaType, 
										  TypeKind typeKind, bool final, 
										  uint32 nVTableSlots,
										  Package &package, const char *name,
										  uint32 nInterfaces, 
										  Interface **interfaces, 
										  uint32 nFields, Field **fields,
										  uint32 nMethods, Method **methods):
	Type(metaType, typeKind, final, nVTableSlots),
	package(package),
	name(name),
	nInterfaces(nInterfaces),
	interfaces(interfaces),
	nFields(nFields),
	fields(fields),
	nMethods(nMethods),
	methods(methods)
{}


//
// Get the vtable entry at the given index.
//
inline VTableEntry &Class::getVTableEntry(uint32 vIndex)
{
	assert(vIndex < nVTableSlots);
	return *(VTableEntry *)((ptr)this + vTableIndexToOffset(vIndex));
}


//
// Initialize a new Interface object that describes essential information such as
// inheritance relationships of objects belonging to a single Java interface.
//
// package is the package that defined this interface, name is the unqualified
// name of this interface, and interfaces is an array of nInterfaces elements
// that lists all interfaces from which this interface derives.
//
inline Interface::Interface(Package &package, const char *name, 
							uint32 nInterfaces, Interface **interfaces, 
							uint32 nFields, Field **fields,
							uint32 nMethods, Method **methods):
	ClassOrInterface(*cInterface, tkInterface, false, 0, package, name, 
					 nInterfaces, interfaces,
					 nFields, fields,
					 nMethods, methods)
{}

#endif
