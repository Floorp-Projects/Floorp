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
#ifndef _ClassFileReader_H_
#define _ClassFileReader_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ConstantPool.h"
#include "Attributes.h"
#include "InfoItem.h"
#include "Pool.h"
#include "FastHashTable.h"
#include "StringPool.h"
#include "FileReader.h"
  
class ClassFileReader;
class FileReader;
class AttributeHandler;
class AttributeHandlerCode;

/* Class access flags */
#define CR_ACC_PUBLIC        0x0001
#define CR_ACC_FINAL         0x0010
#define CR_ACC_SUPER         0x0020
#define CR_ACC_INTERFACE     0x0200
#define CR_ACC_ABSTRACT      0x0400

struct InfoNode {
  const char *sig;  /* descriptor for field/method */
  Int32 index;      /* Index into field/method table */
  InfoNode(const char *sig, Int32 index) : sig(sig), index(index) { }
};

/* Reads and parses a class file */
class ClassFileReader {

public:
  ClassFileReader(Pool &p, StringPool &sp, FileReader &fr);

  Uint16 getMinorVersion() const {
    return minorVersion;
  }

  Uint16 getMajorVersion() const {
    return majorVersion;
  }

  Uint16 getAccessFlags() const {
    return accessFlags;
  }

  /* returns a pointer to the class/interface described by the class file */
  ConstantClass *getThisClass() const {
    return (ConstantClass *) constantPool->get(thisClassIndex);
  }

  /* returns an index into the constant pool rather than a pointer to 
   * the class itself
   */
  Uint16 getThisClassIndex() const {
    return thisClassIndex;
  }

  /* returns a pointer to the super-class or nil if there is none */
  ConstantClass *getSuperClass() const {
    return (superClassIndex > 0) ? (ConstantClass *) 
      constantPool->get(superClassIndex) : 0;
  }

  /* returns an index into the constant pool table that points to the
   * superclass; index is zero if there is no superclass
   */
  Uint16 getSuperClassIndex() const {
    return superClassIndex;
  }

  /* Constant pool table */
  const ConstantPool *getConstantPool() const {
    return (const ConstantPool *) constantPool;
  }

  Uint16 getConstantPoolCount() const {
    return (constantPool) ? (Uint16) constantPool->count() : 0;
  }
  
  /* Interface table */
  const ConstantPool *getInterfaceInfo() const {
    return (const ConstantPool *) interfaceInfo;
  }

  Uint16 getInterfaceCount() const {
    return (interfaceInfo) ? (Uint16) interfaceInfo->count() : 0;
  }

  /* Return an array of all fields in the class. */
  const FieldInfo **getFieldInfo() const {
    return (const FieldInfo **) fieldInfo;
  }

  /* return total number of fields in the field table of the class. */
  Uint16 getFieldCount() const {
    return fieldCount;
  }

  /* Return an array pointing to the public fields of the class. 
   * array[i] is an index into the fields array of the class, and points
   * to a FieldInfo structure.
   */
  const Uint16 *getPublicFields() const {
    return (const Uint16 *) publicFields;
  }

  /* return number of public fields in the class */
  Uint16 getPublicFieldCount() const {
    return publicFieldCount;
  }

  
  /* return an array of all methods in the class */
  const MethodInfo **getMethodInfo() const {
    return (const MethodInfo **) methodInfo;
  }

  /* return total number of methods in the method table of the class */
  Uint16 getMethodCount() const {
    return methodCount;
  }

  /* Return an array of the public methods of the class. This does not
   * include <init> and <clinit> methods. array[i] is an index into the
   * methods array of the class, and points to a MethodInfo structure
   * located at that index.
   */
  const Uint16 *getPublicMethods() const {
    return (const Uint16 *) publicMethods;
  }

  /* return number of public methods in the class. */
  Uint16 getPublicMethodCount() const { 
    return publicMethodCount;
  }

  /* Return an array of the declared methods of the class. This does not
   * include <init> and <clinit> methods. array[i] is an index into the
   * methods array of the class, and points to a MethodInfo structure
   * located at that index.
   */
  const Uint16 *getDeclaredMethods() const {
    return (const Uint16 *) declaredMethods;
  }

  /* return number of declared methods in the class. */
  Uint16 getDeclaredMethodCount() const { 
    return declaredMethodCount;
  }

  /* return an array of constructors of the class. Constructors are
   * methods with the special name <init>. array[i] is an index into the
   * methods array of the class, and points to a MethodInfo structure
   * located at that index.
   */
  const Uint16 *getConstructors() const {
    return (const Uint16 *) constructors;
  }
  
  /* return total number of constructors in the class. */
  Uint16 getConstructorCount() const {
    return constructorCount;
  }

  /* return an array consisting of the public constructors of the class.
   * array[i] is an index into the
   * methods array of the class, and points to a MethodInfo structure
   * located at that index.
   */
  const Uint16 *getPublicConstructors() const {
    return (const Uint16 *) publicConstructors;
  }

  /* return total number of public constructors in the class */
  Uint16 getPublicConstructorCount() const {
    return publicConstructorCount;
  }

  /* Attribute table */
  const AttributeInfoItem **getAttributeInfo() const {
    return (const AttributeInfoItem **) attributeInfo;
  }
  
  Uint16 getAttributeCount() const {
    return attributeCount;
  }

  /* Look up a constant of type integer, long, float, or double whose
   * offset in the constant pool of the class is index. Returns true on success,
   * false if it couldn't find the field or on error.
   */
  bool lookupConstant(Uint16 index, ValueKind &vk, Value &value) const;

  /* look up a fieldref, methodref or interfacemethodref with the
   * given name and signature, and return the index in the field/method
   * array of the class that the field/method/interface method occupies.
   * returns true on success, false on failure.
   */
  bool lookupConstant(const char *name, const char *sig,
		      Uint8 type, Uint16 &index) const;


  /* Look up a field based on given name and type descriptor. On success, 
   * returns the FieldInfo for the field; on failure, returns NULL
   */
  const FieldInfo *lookupField(const char *name, const char *type,
			       Uint16 &index) const {
    return static_cast<const FieldInfo *>
      (lookupFieldOrMethod(CR_CONSTANT_FIELDREF, name, type, index));
  }

  /* Look up a method based on given name and signature. On success,
   * returns the MethodInfo for the field; on failure, returns NULL.
   * sig can be nil if the method is not overloaded.
   */
  const MethodInfo *lookupMethod(const char *name, const char *sig,
				 Uint16 &index) const {
    return static_cast<const MethodInfo *>
      (lookupFieldOrMethod(CR_CONSTANT_METHODREF, name, sig, index));
  }

  /* Look up a field or method based on given name and type descriptor. 
   * On success, returns the InfoItem for the field or method; on failure, 
   * returns NULL. type can be either CR_CONSTANT_FIELDREF (look for a field)
   * or CR_CONSTANT_METHODREF (look for a method). Assumption: both name
   * and sig are interned strings. On success, index is set to the
   * index of the matched field or method in the field or method table
   * of the class.
   */
  const InfoItem *lookupFieldOrMethod(Uint8 type, const char *name, 
				      const char *sig, Uint16 &index) const;

#ifdef DEBUG
  void dump() const;
#endif

private:
  /* Copies of data read from the class file */
  Uint16 minorVersion, majorVersion;
  Uint16 accessFlags;
  Uint16 thisClassIndex, superClassIndex;
  
  /* Array of  constantpoolCount constant pool items  */
  ConstantPool *constantPool;
  
  /* Array of interfaceCount interface references */
  ConstantPool *interfaceInfo;
  
  /* Array of fieldCount fields */
  FieldInfo **fieldInfo;
  Uint16 fieldCount;
  
  /* Array of attributeCount global attributes */
  AttributeInfoItem **attributeInfo;
  Uint16 attributeCount;
  
  /* Array of methodCount methods */
  MethodInfo **methodInfo;
  Uint16 methodCount;
  
  /* Array of publicFieldCount public fields. publicFields[i] is an
   * index into the field array (fieldInfo)
   */
  Uint16 *publicFields;
  Uint16 publicFieldCount;
  
  /* Array of publicMethodCount public methods. publicMethods[i] is an
   * index into the method array (methodInfo)
   */
  Uint16 *publicMethods;
  Uint16 publicMethodCount;

  /* Array of declaredMethodCount declared methods. declaredMethods[i] is an
   * index into the method array (methodInfo). A declared method is a public,
   * protected or private method that is not <init> or <clinit>.
   */
  Uint16 *declaredMethods;
  Uint16 declaredMethodCount;

  /* Array of constructorCount constructors. constructors[i] is an
   * index into the method array (methodInfo)
   */
  Uint16 *constructors;
  Uint16 constructorCount;
  
  /* Array of publicConstructorCount public constructors. 
   * publicConstructors[i] is an index into the method array (methodInfo)
   */
  Uint16 *publicConstructors;
  Uint16 publicConstructorCount;

  /* Array of numAttrHandlers handler functions that handle attributes that 
   * we know about
   */
  AttributeHandler **attrHandlers;
  int numAttrHandlers;
  /* Physical size of the attrHandlers array */
  int attrSize;
  
  /* Pool used to allocate dynamic structures */
  Pool &p;
  
  /* Pool that holds interned strings */
  StringPool &sp;
  
  /* Class that reads from the class file */
  FileReader &fr;
  
  /* Hashtables for fast lookups for fields and methods, indexed by the
   * name of the field/method
   */
  FastHashTable<InfoNode *> fieldTable, methodTable;

  /* Reads and parses the constant pool section of the class file */
  void readConstantPool();
  
  /* Reads and parses the interface section of the class file */
  void readInterfaces();
  
  /* Reads and parses the method section of the class file */
  void readMethods();
  
public:
  void readAttributes(AttributeInfoItem **attrInfo,
		      Uint32 attrCount);
private:
  void readFields();
  void readInfoItems(Uint32 count, InfoItem **info, bool field);
  
  /* Read one attribute from the current position in the file */
  AttributeInfoItem *readAttribute();
  
  void addAttrHandler(AttributeHandler *handler);
  
  bool invalidIndex(Uint32 index) {
    return (index < 1 || index >= constantPool->count());
  }

};


#endif /* _ClassFileReader_H_ */



