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
#ifndef CLASSFILESUMMARY_H
#define CLASSFILESUMMARY_H

#include "ClassReader.h"
#include "Pool.h"
#include "StringPool.h"
#include "ClassWorld.h"
#include "Method.h"
#include "Collector.h"
#include "InterfaceDispatchTable.h"

#define ALIGN(size,mod) ((size)+(((size)%(mod)) ? ((mod)-((size)%(mod))) : 0))
#define ALIGNMENT 4

typedef struct _AnnotationItem AnnotationItem;
typedef Uint16 ConstantPoolIndex;

struct Field;
struct Method;
struct FieldOrMethod;
class ClassCentral;


/* Hashtable key operations where the key is a pointer to a Method */
struct MethodKeyOps {
  static bool equals(const Method * userKey, const Method * hashTableKey) {
    return (userKey == hashTableKey);
  }

  static Method *copyKey(Pool &/*pool*/, const Method * userKey) {
    return const_cast<Method *>(userKey);
  }

  static Uint32 hashCode(const Method * userKey) {
    return reinterpret_cast<Uint32>(userKey);
  }
};

/* A hashtable where a pointer to a Method is the key */
template<class N>
class MethodHashTable:public HashTable<N, const Method *, MethodKeyOps> {
public:
  MethodHashTable(Pool &p):HashTable<N, const Method *, MethodKeyOps>(p) {};
};

/* symbolic V-Table node */
struct VTableNode;

/* A ClassFileSummary encapsulates all the compile-time information about a
   Java class.  It is different from the Class objects which are Java objects
   that describe a Java class. */
class NS_EXTERN ClassFileSummary {
public:  
  ValueKind lookupConstant(ConstantPoolIndex cpi, Value &value) const;


  TypeKind lookupStaticField(ConstantPoolIndex cpi, ValueKind &vk, addr &a, 
							 bool &isVolatile, bool &isConstant);

  TypeKind lookupInstanceField(ConstantPoolIndex cpi, ValueKind &vk, 
							   Uint32 &offset, bool &isVolatile, 
							   bool &isConstant);

  Class &lookupClass(ConstantPoolIndex cpi);

  Interface &lookupInterface(ConstantPoolIndex cpi);

  Type &lookupType(ConstantPoolIndex cpi);

  const Method &lookupStaticMethod(ConstantPoolIndex cpi, Signature &sig, 
								   addr &a);

  const Method *lookupVirtualMethod(ConstantPoolIndex cpi, Signature &sig, 
									Uint32 &vIndex, addr &a);

  const Method *lookupInterfaceMethod(ConstantPoolIndex cpi, Signature &sig,
                                      Uint32 &vIndex,
									  Uint32 &interfaceNumber, addr &a, uint nArgs);

  const Method &lookupSpecialMethod(ConstantPoolIndex cpi, Signature &sig,
  									bool &isInit, addr &a);

  const Method *lookupImplementationOfInterfaceMethod(const Method *interfaceMethod,
                                                      Uint32 &vIndex, addr &a);

  /* returns the fully qualified name of this class. */
  const char *getClassName() const {
    return className;
  }

  const char *getSuperclassName() const;

  /* returns the minor version of this class from the class file */
  Uint16 getMinorVersion() const;

  /* Returns the major version of this class from the class file */
  Uint16 getMajorVersion() const;

  /* returns the access flags for this class from the class file */
  Uint16 getAccessFlags() const;
  
  /* Returns the class object corresponding to this class */
  Type *getThisClass() const { return clazz; }

  /* returns the class object corresponding to the superclass
   * of this class, NIL if this class is Object or an interface.
   */
  Type *getSuperClass() const;

  /* returns the class object corresponding to the supertype
   * of this class, NIL if this class is Object.  For all objects,
   * except arrays and interfaces, the supertype is the same as the
   * superclass.
   */
  Type *getSuperType() const;

  /* Returns total space taken up by this object. For array classes,
   * returns total space taken up by the component class.
   */
  Uint32 getObjSize() const;

  const ConstantPool *getConstantPool() const;

  Uint32 getConstantPoolCount() const;
  
  const Interface **getInterfaces();

  Uint16 getInterfaceCount() const;

  const Field **getFields();

  Uint16 getFieldCount() const;

  const Method **getMethods();

  const Method **getDeclaredMethods();
  
  Uint16 getDeclaredMethodCount();

  const Constructor **getConstructors();

  Uint16 getConstructorCount();

  const MethodInfo **getMethodInfo() const;

  Uint16 getMethodCount() const;

  Field **getPublicFields();

  Uint16 getPublicFieldCount();
  
  Method **getPublicMethods();

  Uint16 getPublicMethodCount();

  Field *getField(const char *name);

  Field *getDeclaredField(const char *name);

  Constructor **getPublicConstructors();
  Uint16 getPublicConstructorCount();

  Method *getMethod(const char *name, const Type *parameterTypes[],
					Int32 numParameterTypes);

  Method *getMethod(const char *name, const char *sig);

  Method *getDeclaredMethod(const char *name, const Type *parameterTypes[],
							Int32 numParameterTypes);

  Constructor *getConstructor(const Type *parameterTypes[],
							  Int32 numParameterTypes);

  Constructor *getDeclaredConstructor(const Type *parameterTypes[],
									  Int32 numParameterTypes);

  const AttributeInfoItem **getAttributeInfo() const;

  Uint16 getAttributeCount() const;

  /* Returns parent's summary. */
  ClassFileSummary *getParentInfo() { return parentInfo; }

  /* Don't call this directly; call ClassCentral::addClass instead. */
  ClassFileSummary(Pool &staticp, Pool &runtimep, 
				   StringPool &sp, ClassCentral &central, 
				   ClassWorld &world,
				   const char *className, 
				   FileReader *fr,
				   bool arrayType, 
				   Uint32 numDimensions,
				   bool primitiveType,
				   TypeKind tkind,
				   ClassFileSummary *component);

  /* Return a pointer to the interned version of the string <init>. This is
   * meant to be used by objects Class, Method and Constructor.
   */
  const char *getInitString() { return initp; }

  /* Return a pointer to the interned version of the string <clinit>. 
   * This is meant to be used by objects Class, Method and Constructor.
   */
  const char *getClinitString() { return clinitp; }
  
private:
  const FieldOrMethod *resolveName(const Uint8 type, const char *name,
								   const char *desc, const InfoItem *&item);

  void resolveFieldOrMethod(const Uint8 type, Uint16 index);
  
  const Field *lookupField(ConstantPoolIndex cpi, 
						   ValueKind &vk, 
						   TypeKind &tk,
						   bool &isVolatile, 
						   bool &isConstant,
						   bool &isStatic);

  void getPackageNames();

  ClassOrInterface &lookupClassOrInterface(ConstantPoolIndex cpi, 
										   bool &isInterface);

  const Method *lookupMethod(Uint8 type, ConstantPoolIndex cpi, 
							 Signature &sig, 
							 bool &isStatic,
							 bool &dynamicallyResolved, Uint32 &vIndex, 
							 addr &a,
							 bool &isInit, bool &isClinit, bool special);

  void resolveSpecial(const Uint8 type, ConstantPoolIndex index);

  void annotate(Uint8 type,
				ConstantPoolIndex index, const InfoItem *item,
				const FieldOrMethod *descriptor,
				ClassFileSummary *info,
				const char *name,
				const char *sig);

	
  Type *makeClazz();

  void computeSize();

  /* Do not call this method directly; use the lookup methods instead */
  Uint32 getVTable(VTableNode *&vt) const;

public:
  /* Set the size of the parent object.
   * Don't call this directly; this is for use by ClassCentral only.
   */
  void setParent(ClassFileSummary *info);

  /* Add a sub-class, whose information is given by info, to the list
   * of sub-classes of this class.
   */
  void addSubclass(ClassFileSummary &info) {
	subClasses.append(&info);
  }

  /* Update the clazz' vtable to point to a newly compiled method's code */
  void updateVTable(Method &method);

#ifdef DEBUG_LOG
  void dumpVTable();
#endif

  void addVIndexForMethod(Method *method, Uint32 vIndex);

  Field &buildField(Uint32 fieldIndex);
  Method &getMethod(Uint32 methodIndex) {
    Method *method = methods.get(methodIndex);
    if (method)
      return *method;
    return buildMethod(methodIndex);
  }
  Interface &buildInterface(Uint32 interfaceIndex);

private:
  Method &buildMethod(Uint32 methodIndex);
  Field &buildPublicField(Uint32 index);
  Method &buildPublicMethod(Uint32 index);
  Method &buildDeclaredMethod(Uint32 index);
  Constructor &buildConstructor(Uint32 index);
  Constructor &buildPublicConstructor(Uint32 index);

public:
  /* These are declared public only for use by internal routines of
   * ClassInfo. These are not to be used directly
   */
  void buildFields();
  void buildMethods();
  void buildConstructors();
  void buildPublicFields();
  void buildPublicMethods();
  void buildDeclaredMethods();
  void buildPublicConstructors();

  const ClassFileReader *getReader() const { return reader; }

private:
  void buildInterfaces();

  bool isSubClassOf(ClassFileSummary *summary);

  void buildVTable();
  void addInterfaceToVTable(const Interface *superInterface, 
                            VTableNode *vtable,
                            Uint32 &numEntries);

  static void runStatics(ClassOrInterface *clz);

  Field *getField(const char *name, bool publik, bool interned);   
  Method *getMethod(const char *name, const Type *parameterTypes[],
					Int32 numParameterTypes, bool publik,
					bool interned);

  /* Name of the class represented by this summary */
  const char *className;

  /* Pool used to allocate all static information (other than the Class,
   * Interface, Field and Method classes
   */
  Pool &staticPool;

  /* Pool used to allocate all dynamic (runtime) information (Class,
   * Interface, Method and Field classes
   */
  Pool &runtimePool;

  /* String hash table where interned strings are kept. */
  StringPool &sp;

  /* ClassFileReader class that reads the class file into memory */
  const ClassFileReader *reader;

  /* ClassCentral class that holds all existing ClassFileSummary objects
   * in the universe
   */
  ClassCentral &central;

  /* ClassWorld class that holds all existing Class, Interface and Type
   * objects in the universe. ClassCentral is a compile-time repository,
   * and ClassWorld is a runtime repository.
   */
  ClassWorld &world;

  /* Constant Pool of class */
  const ConstantPool *constantPool;
  Uint32 nConstants;

  /* Resolved information about each constantPool item. 
   * annotations[i].resolved is true if the item at index i in the
   * constant pool has been resolved.
   */
  AnnotationItem *annotations;    

  /* Total size of instances of this class and its superclass (if any) */
  Uint32 totalSize, parentSize;
  
  /* VTable for this class */
  VTableNode *vtable;
  Uint32 numVTableEntries;

  /* Table used to lookup interface methods within class */
  InterfaceDispatchTable *interfaceDispatchTable;

  /* Summary for the parent class of this class. NULL if this is the class Object. */
  ClassFileSummary *parentInfo;

  /* Type class for this class, if it exists */
  Type *clazz;

  /* Set of all fields (public, protected, private) declared by this class
   * only. This does not include fields which may have been declared in 
   * a super class of this class and are accessible from this class.
   */
  Collector<Field> fields;
  
  /* Set of offsets for each field in the "fields" array*/
  Uint32 *fieldOffsets;

  /* Set of all public fields accessible from this class. This includes
   * public fields in super-classes which are accessible from this class.
   * Note that those fields declared in this class will also be part of
   * the fields collection.  
   */
  Collector<Field> publicFields;

  /* Set of all methods (public, protected, private) declared by this
   * class only. This does not include methods which may have been
   * declared in superclasses and are accessible from this class.
   * However, this does include constructors (<init>) and static
   * initializer methods (<clinit>).
   */
  Collector<Method> methods;

  /* Set of all methods (public, protected, private) declared by this
   * class only. This does not include methods which may have been
   * declared in superclasses and are accessible from this class.
   * It also does not include constructors and static initializers
   */
  Collector<Method> declaredMethods;

  /* Set of all public methods accessible from this class. This includes
   * public methods declared in superclasses that are visible from this class.
   * Note that those methods declared in this class will also be part of
   * the methods collection.
   */
  Collector<Method> publicMethods;

  /* Set of all constructors implemented by this class. Note that these
   * will also be part of the methods collection.
   */
  Collector<Constructor> constructors;

  /* Set of all public constructors. Note that these
   * will also be part of the methods collection.
   */
  Collector<Constructor> publicConstructors;

  /* Set of direct superinterfaces of this class */
  Collector<Interface> interfaces;

  /* True if we're an array type */
  bool arrayType;
  
  /* If we're an array type, this is the number of dimensions in the array */
  Uint32 numDimensions;

  /* If an array type, this is the pointer to our element class
     or interface, unless this is an array of primitive elements  */
  ClassFileSummary *elementSummary;
  
  /* True if our element type is primitive */
  bool primitiveElementType;
  
  /* If our element type is primitive, this is its primitive type */
  TypeKind tk;

  /* Pointers to interned versions of init and clinit */
  const char *initp, *clinitp;

  /* Package Name and unqualified name of the class.
   * packageName("java/lang/Object") = "java.lang"; and
   * unqualifiedName("java/lang/Object") = "Object".
   */
  const char *packageName;
  const char *unqualName;

  /* Global lookup table for interface methods implemented by this class */
  InterfaceDispatchTable interfaceTable;

  /* Table of Vindex's used by a given Method */
  MethodHashTable<Vector<Uint32>* > vIndicesUsed;

  /* Set of all sub-classes of this class that have currently been loaded
   * by the VM
   */
  Vector<ClassFileSummary *> subClasses;
};

#endif /* CLASSFILESUMMARY_H */

