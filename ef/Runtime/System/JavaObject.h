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
#include "InterfaceDispatchTable.h"
#include "LogModule.h"

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
const uint nPrimitiveTypeKinds = tkDouble + 1;
const uint nTypeKinds = tkInterface + 1;

inline bool NS_EXTERN isPrimitiveKind(TypeKind tk) {return tk <= tkDouble;}
inline bool NS_EXTERN isNonVoidPrimitiveKind(TypeKind tk) {return tk >= tkBoolean && tk <= tkDouble;}
inline bool NS_EXTERN isDoublewordKind(TypeKind tk) {return tk == tkLong || tk == tkDouble;}
inline int NS_EXTERN getTypeKindSize(TypeKind tk);
inline int NS_EXTERN getLgTypeKindSize(TypeKind tk);
inline int NS_EXTERN nTypeKindSlots(TypeKind tk);
inline bool NS_EXTERN getTypeKindFromDescriptor(char c, TypeKind &tk);

#ifdef DEBUG

// Flags for dumping objects and arrays
#define PRINT_ARRAY_INDICES    0x01
#define PRINT_OBJECT_TYPE      0x02
#define PRINT_STATIC_FIELDS    0x04
#define PRINT_DEFAULT_FLAGS    PRINT_OBJECT_TYPE

#endif

// ----------------------------------------------------------------------------

struct Type;
struct Class;
struct Interface;
struct Package;

struct Field;
struct Method;
struct Constructor;

class ClassCentral;
class ClassFileSummary;

// A JavaObject is the header of any Java object.
// It contains just one field -- a pointer to the object's type.
// An actual Java object's fields follow after the end of the JavaEnd structure.
struct NS_EXTERN JavaObject
{
    const Type* type;							// Pointer to the object's type
	Int32 state;					// contains lock and gc state, and possibly hash id
	explicit JavaObject(const Type &type): type(&type), state(0x3) {}
public:
    inline const Type &getType() const { return *type; }
    inline const Class &getClass() const;
#ifdef DEBUG
    void printValue(LogModuleObject &f,
                    int maxRecursion = 0,
                    Uint32 printFlags = PRINT_DEFAULT_FLAGS,
                    const char *newlineString = NULL,
                    Vector<JavaObject *> *printedObjects = NULL);
#endif

};
const Int32 objectTypeOffset = 0;				// Offset from object address to its type pointer word

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
struct NS_EXTERN JavaArray: JavaObject
{
	const Uint32 length;						// Number of elements in this array

	JavaArray(const Type &type, Uint32 length): JavaObject(type), length(length) {}
	
	JavaObject *get(Int32 index);
	Int8 getBoolean(Int32 index);
	char getByte(Int32 index);
	Int16 getChar(Int32 index);
	Int16 getShort(Int32 index);
	Int32 getInt(Int32 index);
	Int64 getLong(Int32 index);
	Flt32 getFloat(Int32 index);
	Flt64 getDouble(Int32 index);
	
	void set(Int32 index, JavaObject *obj);
	void setBoolean(Int32 index, Int8 value);
	void setByte(Int32 index, char value);
	void setChar(Int32 index, Int16 value);
	void setShort(Int32 index, Int16 value);
	void setInt(Int32 index, Int32 value);
	void setLong(Int32 index, Int64 value);
	void setFloat(Int32 index, Flt32 value);
	void setDouble(Int32 index, Flt64 value);
private:
    void *getRaw(Int32 index);
    bool isPrimitive();
};

// Offset from array address to its length word
const Int32 arrayLengthOffset = offsetof(JavaArray, length);

// Offset from array address to its first element, equivalent to arrayEltsOffset(tkObject)
const Int32 arrayObjectEltsOffset = sizeof(JavaArray);


// ----------------------------------------------------------------------------

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
    void *classOrCodePtr;                       // One of the above
};

struct Array;
class InterfaceDispatchTable;

// A Type describes the contents of a Java object.
// It's also itself a special kind of Java object.
struct NS_EXTERN Type: JavaObject
{
    const TypeKind typeKind ENUM_8;				// Kind of objects described here
	const bool final BOOL_8;					// True if this type cannot have any subtypes.  Note that, by this
												//   definition, an array type is final if its element type is final.
	const Uint32 nVTableSlots;					// Number of vtable slots for this class; zero if there is no vtable
    const VTableOffset *interfaceVIndexTable;   // Table for mapping interface to first VIndex in interface's VTable
    const Uint16 *interfaceAssignableTable;     // Table for determining if instance is assignable to an interface type
    Uint16 assignableMatchValue;                // Used to compare with entry in interfaceAssignableTable

  private:
	mutable const Array *arrayType;				// Type of arrays whose components have this Type or null if none declared yet

  protected: // This is an abstract class
    Type(const Type &metaType, const Type *superType, TypeKind typeKind, bool final, Uint32 nVTableSlots,
         VTableOffset *interfaceVIndexTable = 0, Uint16 *interfaceAssignableTable = 0):
	JavaObject(metaType), typeKind(typeKind), final(final), nVTableSlots(nVTableSlots),
      interfaceVIndexTable(interfaceVIndexTable),
      interfaceAssignableTable(interfaceAssignableTable),
      arrayType(0), superType(superType) {}

    const Type *superType;
  public:
	const Array &getArrayType() const;
    const Type *getSuperType() const {return superType;};
    const Type *getSuperClass() const;

    Uint32 getInterfaceNumber() const;
        
	VTableEntry &getVTableEntry(Uint32 vIndex) const;
    void setVTableEntry(Uint32 vIndex, void *classOrCodePtr);
    
    // Returns true if the current class object describes an interface.
    bool isInterface() const {return (typeKind == tkInterface);}
    bool isArray() const {return (typeKind == tkArray);}
    
    // Returns true if this class describes a primitive type
    bool isPrimitive() const {return isPrimitiveKind(typeKind); }

	// Returns true if the type represented by the specified Class parameter 
	// can be converted to the type represented by this class object.
	bool isAssignableFrom(const Type &from) const;
    
	// Returns true if the object represented by obj is an instance of this class
    bool isInstance(JavaObject &obj) const {return (&obj.getType() == this ||
													isAssignableFrom(obj.getType()));}

	// Returns the modifiers (access flags) for this class object.
	Uint32 getModifiers() const;
    
    // Returns the fully qualified name of the class; unlike toString(),
	// does not prepend "class " in front of the name
	const char *getName() const;

  #ifdef DEBUG_LOG
	int printRef(LogModuleObject &f) const;
  #endif
};

inline Type &asType(JavaObject &o);

bool implements(const Type &sub, const Type &super);


// A Type for void, boolean, ubyte, byte, char, short, int, long, float, or double.
struct NS_EXTERN PrimitiveType: Type
{
  private:
	static const PrimitiveType *primitiveTypes[nPrimitiveTypeKinds];
		
	// Only initPrimitiveTypes can construct PrimitiveType instances.
    PrimitiveType(const Type &metaType, TypeKind typeKind): Type(metaType, NULL, typeKind, true, 0, 0) { }

  public:
	static const PrimitiveType &obtain(TypeKind tk);
	static const PrimitiveType *getClass(const char* className);
	static void staticInit();
    const char *getName() const;
    char getSignatureEncoding() const;
};

inline const PrimitiveType &asPrimitiveType(const Type &t)
{
	assert(isPrimitiveKind(t.typeKind));
	return *static_cast<const PrimitiveType *>(&t);
}


// A Type for any array.
struct NS_EXTERN Array: Type
{
	const Type &componentType;					// Type of components of this array

  private:
    mutable const char *fullName;				// Fully qualified name of the array

    // Unique serial number among all interfaces and arrays of interfaces
    Uint32 interfaceNumber;
    
    InterfaceDispatchTable *interfaceDispatchTable;

	static const Array *primitiveArrays[nPrimitiveTypeKinds];
	explicit Array(const Type &componentType, const Type &superType,
                   Uint32 nVTableSlots);
    void buildAssignabilityTableForInterfaces();
    
  public:
    static Array &make(const Type &componentType);
	static const Array &obtain(TypeKind elementKind);
    const Type &getElementType() const;
    const Type &getElementType(int &numDimensions) const;
    const Type &getComponentType() const {return componentType;};

    // Used for arrays of interfaces
    Uint32 getInterfaceNumber() const {
        assert(getElementType().typeKind == tkInterface);
        return interfaceNumber;
    }

#ifdef DEBUG
    void printValue(LogModuleObject &f, JavaArray *array,
                    int maxRecursion = 0,
                    Uint32 printFlags = PRINT_DEFAULT_FLAGS,
                    const char *newlineString = NULL,
                    Vector<JavaObject *> *printedObjects = NULL) const;
#endif

    // Returns the fully qualified name of the class; unlike toString(),
	// does not prepend "class " in front of the name
	const char *getName() const;

  #ifdef DEBUG_LOG
	int printRef(LogModuleObject &f) const;
  #endif
  
	static void staticInit();
};

inline const Array &asArray(const Type &t) {
    assert(t.typeKind == tkArray);
    return *static_cast<const Array*>(&t);
}


// ----------------------------------------------------------------------------


// An abstract class that contains information common to Class and
// Interface objects.
struct NS_EXTERN ClassOrInterface: Type
{
	Package &package;							// The package that defines this class or interface
    const char * /*const*/ name;				// Unqualified name of the class or interface (stored in the ClassWorld's pool)
  private:
    mutable const char *fullName;				// Fully qualified name of the class or interface
    mutable const char *declName;				// Class declaration, as returned by toString()
  public:

	const Uint32 nInterfaces;					// Number of interfaces implemented directly by this class or interface
    Interface **interfaces;						// Array of nInterfaces interfaces implemented directly by this class or interface

    static ClassCentral *central;				// Repository of loaded classes
    ClassFileSummary *summary;					// Class loader for this class, NULL if none
	bool initialized;

	// Run all the static initializers of this class
	void runStaticInitializers();

	bool isInitialized() { return initialized; }
  
  protected:
    ClassOrInterface(const Type &metaType, TypeKind typeKind,
                     const Class *parent,
                     bool final, 
					 Uint32 nVTableSlots, Package &package,
					 const char *name, Uint32 nInterfaces, 
					 Interface **interfaces, 
					 ClassFileSummary *summary,
                     VTableOffset *interfaceVIndexTable = 0,
                     Uint16 *interfaceAssignableTable = 0);

  public:
  #ifdef DEBUG_LOG
	int printRef(LogModuleObject &f) const;
  #endif

	// Set the value of the ClassCentral object. This should be done
	// only once.
	static void setCentral(ClassCentral *c) { central = c; }

	// Returns fully qualified name of the class/interface in the form 
	// "class <classname>" or "interface <interfacename>"
	const char *toString() const;

	// Returns the fully qualified name of the class; unlike toString(),
	// does not prepend "class " in front of the name
	const char *getName() const;

	// Returns the modifiers (access flags) for this class object.
	Uint32 getModifiers() const;

	// Returns the class loader object for this class. Need to think about
	// this a little bit.
	// ClassLoader getClassLoader() const;

	// Returns summaries for the interfaces of this class.
	// If this class object represents a class, returns an array containing 
	// objects representing the interfaces directly implemented by the class. 
	// If this Class object represents an interface, returns an array
	// containing objects representing the direct superinterfaces of the 
	// interface. Returns number of array interfaces actually returned.
	// This number could be zero, if the class is a primitive type or
	// implements no interfaces.
	Int32 getInterfaces(Interface **&summaries) const;

	// If this class or interface is a member of another class, returns the 
	// class object representing the class of which it is a member. If this is
	// not the case, returns NULL. Hmmm. Need to think about how to implement 
	// this.
	// Class *getDeclaringClass();

	// Returns an array containing class objects representing all the public 
	// classes and interfaces that are members of the class represented by this
	// class object. Returns number of class objects returned.
	Int32 getClasses(Class **&summaries);

	// Returns an array containing information about the public fields of this 
	// object. Returns number of fields in object.
	Int32 getFields(const Field **&fields);

	// Returns an array containing information about the public methods of 
	// this object. Returns number of fields in object.
	Int32 getMethods(const Method **&methods);

	// Returns an array containing information about the public constructors of
	// this object. Returns number of constructors.
	Int32 getConstructors(const Constructor **&constructors);

	// Look up fields/methods/constructors. These methods search through 
	// publicly accessible fields/methods/constructors. They throw a
	// runtimeError on failure.
	Field &getField(const char *name);
	Method &getMethod(const char *name, const Type *parameterTypes[],
					  Int32 numParams);

	// Look up a method based on its name and signature. This routine searches
	// only in the current class, and does not search in superclasses.
	// If publicOrProtected is true, only public and protected methods are searched;
	// otherwise, all declared methods are examined. If the method was not found,
	// returns NULL.
	Method *getMethod(const char *name, const char *sig, bool publicOrProtcted);
	
	Constructor &getConstructor(const Type *parameterTypes[],
								Int32 numParams);

	// The following functions deal with *all* declared fields, methods, or
	// constructors, be they public, protected, or private.
	Int32 getDeclaredClasses(Class **&summaries);
	Int32 getDeclaredFields(const Field **&fields);
	Int32 getDeclaredMethods(const Method **&methods);
	Int32 getDeclaredConstructors(const Constructor **&constructors);
    
    Field &getDeclaredField(const char *name);
	Method &getDeclaredMethod(const char *name, const Type *parameterTypes[], 
							  Int32 numParams);

	Constructor &getDeclaredConstructor(const Type *parameterTypes[], 
										Int32 numParams);
		
private:
    void getFullNames() const;
};


// Runtime reflection information about a class, such as its inheritance
// hierarchy, fields and field layout, etc.
// A Class remains in memory as long as a class is alive.  It does
// not keep track of a class's constant pool, etc.
struct NS_EXTERN Class: ClassOrInterface
{
	Uint32 instanceSize;						// Size of instance objects in bytes
  private:

	Class(Package &package, const char *name, const Class *parent, Uint32 nInterfaces, Interface **interfaces,
		  ClassFileSummary *summary, bool final, Uint32 instanceSize, Uint32 nVTableSlots,
          VTableOffset *interfaceVOffsetTable, Uint16 *interfaceAssignableTable);
	Class(bool isOwnClass, const char *name, Uint32 instanceSize, Uint32 nVTableSlots);

  public:
	static Class &make(Package &package, const char *name, const Class *parent, Uint32 nInterfaces, Interface **interfaces,
					   ClassFileSummary *summary, bool final, Uint32 extraSize, Uint32 nExtraVTableSlots,
                       VTableOffset *interfaceVOffsetTable,
                       Uint16 *interfaceAssignableTable);
	// Given a fully qualified className, attempts to load and link the class
	// and returns the corresponding Class object. The name could be of
    // the form java/lang/Object or java.lang.Object .
	static Class &forName(const char *className);

	// Ensure that the object obj is a primitive class and
	// return the raw primitive value corresponding to the class,
	// converted to a Uint32. 
	static Uint32 getPrimitiveValue(JavaObject &obj);

	// Creates an instance of the class if the class is instantiable.
	JavaObject &newInstance();

    static JavaObject &makePrimitiveObject(TypeKind tk, void *address);

    JavaObject *clone(JavaObject &inObj) const;
	

#ifdef DEBUG
    void printValue(LogModuleObject &f, JavaObject *obj,
                    int maxRecursion = 0,
                    Uint32 printFlags = PRINT_DEFAULT_FLAGS, 
                    const char *newlineString = NULL,
                    Vector<JavaObject *> *printedObjects = NULL);
#endif

  private:
	static Class &makeSpecial(bool isOwnClass, const char *name, Uint32 instanceSize, Uint32 nVTableSlots);
	void setParent(const Class &p);
  public:

	const Class *getParent() const;
	bool implements(const Class &c) const;
	bool implements(const Interface &i) const;

	friend class Standard;	// Standard needs to call makeSpecial and explicitly set the parents of the first few classes created
							// for bootstrapping reasons.
};

inline const Class &asClass(const Type &t);

const Uint32 vTableOffset = sizeof(Class);		// Offset from a Type to beginning of its vtable
inline Int32 vTableIndexToOffset(Uint32 vIndex) {return (Int32)(vTableOffset + vIndex*sizeof(VTableEntry));}

struct NS_EXTERN Interface: ClassOrInterface
{
private:
    // Unique serial number among all interfaces and arrays of interfaces
    Uint32 interfaceNumber;
    
public:
    Interface(Package &package, const char *name, Uint32 nInterfaces, 
			  Interface **interfaces, 
			  ClassFileSummary *summary);

	bool implements(const Interface &i) const;

    void addInterfaceArraysToAssignableTable(InterfaceDispatchTable *interfaceDispatchTable,
                                             int numDimensions);

    Uint32 getInterfaceNumber() const {return interfaceNumber;};
};
inline const Interface &asInterface(const Type &t);


// ----------------------------------------------------------------------------
// Standard objects


// Standard classes
enum StandardClass {
	cNone, 
	cObject,						// java.lang.Object
	cPrimitiveType,					// internal variant of java.lang.Class
	cPrimitiveArray,				// internal variant of java.lang.Class
	cObjectArray,					// internal variant of java.lang.Class
	cClass,							// internal variant of java.lang.Class
	cInterface,						// internal variant of java.lang.Class
	cField,                         // internal variant of java.lang.Field
	cMethod,                        // internal variant of java.lang.Method
	cConstructor,                   // internal variant of java.lang.Constructor
	cThrowable,						// java.lang.Throwable
	cException,						// java.lang.Exception
	cRuntimeException,				// java.lang.RuntimeException
	cArithmeticException,			// java.lang.ArithmeticException
	cArrayStoreException,			// java.lang.ArrayStoreException
	cClassCastException,			// java.lang.ClassCastException
	cIllegalMonitorStateException,  // java.lang.illegalMonitorStateException
	cArrayIndexOutOfBoundsException,// java.lang.ArrayIndexOutOfBoundsException
	cNegativeArraySizeException,	// java.lang.NegativeArraySizeException
	cNullPointerException,			// java.lang.NullPointerException
	cError,							// java.lang.Error
	cThreadDeath,					// java.lang.ThreadDeath
	cBoolean,						// java.lang.Boolean
	cByte,							// java.lang.Byte
	cCharacter,						// java.lang.Character
	cShort,							// java.lang.Short
	cInteger,						// java.lang.Integer
	cLong,							// java.lang.Long
	cFloat,							// java.lang.Float
	cDouble,					    // java.lang.Double
    cString,					    // java.lang.String
	cInterruptedException			// java.lang.InterruptedException
};
const uint nStandardClasses = cString + 1;
const uint firstOrdinaryStandardClass = cThrowable;

// Standard interfaces
enum StandardInterface {
	iCloneable						// java.lang.Cloneable
};
const uint nStandardInterfaces = iCloneable + 1;


// Standard objects
enum StandardObject {
	oArithmeticException,			// new java.lang.ArithmeticException()
	oArrayStoreException,			// new java.lang.ArrayStoreException()
	oClassCastException,			// new java.lang.ClassCastException()
	oIllegalMonitorStateException,	// new java.lang.IllegalMonitorStateException
	oArrayIndexOutOfBoundsException,// new java.lang.ArrayIndexOutOfBoundsException()
	oNegativeArraySizeException,	// new java.lang.NegativeArraySizeException()
	oNullPointerException,			// new java.lang.NullPointerException()
	oInterruptedException           // new java.lang.InterruptedException()
};
const uint nStandardObjects = oNullPointerException + 1;


class NS_EXTERN Standard
{
	static const Class *standardClasses[nStandardClasses];
	static const Interface *standardInterfaces[nStandardInterfaces];
	static const JavaObject *standardObjects[nStandardObjects];

public: 
	static bool isPrimitiveWrapperClass(const Class &clazz, TypeKind *tk = 0);
    static bool isDoubleWordPrimitiveWrapperClass(const Class &clazz);
	static const Class &get(StandardClass sc)  {assert(standardClasses[sc]); return *standardClasses[sc];}
	static const Interface &get(StandardInterface si)  {assert(standardInterfaces[si]); return *standardInterfaces[si];}
	static const JavaObject &get(StandardObject so)  {assert(standardObjects[so]); return *standardObjects[so];}

	static void initStandardObjects(ClassCentral &c);
};


// --- INLINES ----------------------------------------------------------------

extern const NS_EXTERN Int8 typeKindSizes[nTypeKinds];

//
// Return the number of bytes taken by a value that has the given TypeKind.
//
inline int getTypeKindSize(TypeKind tk)
{
	assert((uint)tk < nTypeKinds);
	return typeKindSizes[tk];
}


extern const NS_EXTERN Int8 lgTypeKindSizes[nTypeKinds];
//
// Return the base-2 logarithm of the number of bytes taken by a value
// that has the given TypeKind.
//
inline int getLgTypeKindSize(TypeKind tk)
{
	assert((uint)tk < nTypeKinds);
	return lgTypeKindSizes[tk];
}


extern const Int8 typeKindNSlots[nTypeKinds];
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
    arrayType = &Array::make(*this);
	return *arrayType;
}


//
// Assert that this JavaObject is really a Type and return that Type.
//
inline Type &asType(JavaObject &o)
{
    const Type *type = &o.getType();

	assert(type &&
		   (type == &Standard::get(cPrimitiveType) ||
		    type == &Standard::get(cPrimitiveArray) ||
		    type == &Standard::get(cObjectArray) ||
			type == &Standard::get(cClass) ||
			type == &Standard::get(cInterface)));
	return *static_cast<Type *>(&o);
}

inline Type &asType(const Array &o) {
  return *static_cast<Type *>(const_cast<Array *>(&o));
}

inline Type &asType(const Class &o) {
  return *static_cast<Type *>(const_cast<Class *>(&o));
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

// Return the offset from an array's address to its first element.
inline int arrayEltsOffset(TypeKind tk)
{
	assert((uint)tk < nTypeKinds);
    int lgtks = lgTypeKindSizes[tk];

    // Round up address so elements are aligned
    return (((sizeof(JavaArray) + typeKindSizes[tk] - 1) >> lgtks) << lgtks);
}

// ----------------------------------------------------------------------------


//
// Assert that this Type is really a Class and return that Class.
//
inline const Class &asClass(const Type &t)
{
	assert(t.typeKind == tkObject);
	return *static_cast<const Class *>(&t);
}

inline const Class &JavaObject::getClass() const {
    return asClass(getType());
}

//
// Assert that this Type is really an Interface and return that Interface.
//
inline const Interface &asInterface(const Type &t)
{
	assert(t.typeKind == tkInterface);
	return *static_cast<const Interface *>(&t);
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
										  TypeKind typeKind,
                                          const Class *parent, bool final, 
										  Uint32 nVTableSlots,
										  Package &package, const char *name,
										  Uint32 nInterfaces, 
										  Interface **interfaces, 
										  ClassFileSummary *summary,
                                          VTableOffset *interfaceVOffsetTable,
                                          Uint16 *interfaceAssignableTable):
	Type(metaType, &asType(*parent), typeKind, final, nVTableSlots, interfaceVOffsetTable, interfaceAssignableTable),
	package(package),
	name(name),
	fullName(0),
	declName(0),
	nInterfaces(nInterfaces),
	interfaces(interfaces),
	summary(summary),
	initialized(false)
{ }

//
// Get this class's parent.  Return nil if this class is Object.
//
inline const Class *Class::getParent() const
{
	// Make sure that we've set the parent already.
	assert(superType || this == &Standard::get(cObject));
	return static_cast<const Class *>(getSuperClass());
}


//
// Get the vtable entry at the given index.
//
inline VTableEntry &Type::getVTableEntry(Uint32 vIndex) const
{
	assert(vIndex < nVTableSlots);
    assert((typeKind == tkObject) || (typeKind == tkArray));
	return *(VTableEntry *)((ptr)this + vTableIndexToOffset(vIndex));
}

//
// Set the vtable entry at the given index to point either to a Class or to a 
// method's code.
//
inline void Type::setVTableEntry(Uint32 vIndex, void *classOrCodePtr)
{
	assert(vIndex < nVTableSlots);
    assert((typeKind == tkObject) || (typeKind == tkArray));
	VTableEntry *vTableEntry = (VTableEntry *)((ptr)this + vTableIndexToOffset(vIndex));
    vTableEntry->classOrCodePtr = classOrCodePtr;
}

#ifdef DEBUG
void printValue(LogModuleObject &f, void *valPtr, const Type &type,
                int maxRecursionLevel = 0,
                Uint32 printFlags = PRINT_DEFAULT_FLAGS,
                const char *newlineString = NULL,
                Vector<JavaObject *> *printedObjects = NULL);

void dump(JavaObject *obj, int maxRecursion = 0, Uint32 printFlags = PRINT_DEFAULT_FLAGS,
          const char *newlineString = "\n");
#endif

#endif
