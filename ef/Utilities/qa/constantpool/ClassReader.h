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

#ifndef _EF_ClassFileReader_H_
#define _EF_ClassFileReader_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ConstantPool.h"
#include "Attributes.h"
#include "InfoItem.h"
#include "Pool.h"
#include "StringPool.h"
#include "FileReader.h"

#define CR_ALIGNMENT   4
  
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

struct InterfaceInfo {
  ConstantPoolItem *item;
  uint16 index;
};

/* Reads and parses a class file */
class ClassFileReader {
friend class AttributeHandlerCode;

public:
  /* returns crErrorNone on success, appropriate error status otherwise */
  ClassFileReader(Pool &p, StringPool &sp, const char *classFileName, 
		  CrError *status);

  ~ClassFileReader() { fr->FileReader::~FileReader(); }

  uint16 getMinorVersion() {
    return minorVersion;
  }

  uint16 getMajorVersion() {
    return majorVersion;
  }

  uint16 getAccessFlags() {
    return accessFlags;
  }

  /* returns a pointer to the class/interface described by the class file */
  ConstantClass *getThisClass() {
    return (ConstantClass *) constantPool[thisClassIndex];
  }

  /* returns an index into the constant pool rather than a pointer to 
   * the class itself
   */
  int getThisClassIndex() {
    return thisClassIndex;
  }

  /* returns a pointer to the super-class */
  ConstantClass *getSuperClass() {
    return (superClassIndex > 0) ? (ConstantClass *) 
      constantPool[superClassIndex] : 0;
  }

  /* returns an index into the constant pool table that points to the
   * superclass; index is zero if there is no superclass
   */
  int getSuperClassIndex() {
    return superClassIndex;
  }

  /* Return total space taken up by all instance fields in this object */
  int getObjSize() {
    return objSize;
  }

  /* Constant pool table */
  const ConstantPoolItem **getConstantPool() {
    return (const ConstantPoolItem **) constantPool;
  }

  uint16 getConstantPoolCount() {
    return constantPoolCount;
  }
  
  /* Interface table */
  const InterfaceInfo *getInterfaceInfo() {
    return (const InterfaceInfo *) interfaceInfo;
  }

  uint16 getInterfaceCount() {
    return interfaceCount;
  }

  /* Field table */
  const FieldInfo **getFieldInfo() {
    return (const FieldInfo **) fieldInfo;
  }

  uint16 getFieldCount() {
    return fieldCount;
  }

  /* Method table */
  const MethodInfo **getMethodInfo() {
    return (const MethodInfo **) methodInfo;
  }

  uint16 getMethodCount() {
    return methodCount;
  }

  /* Attribute table */
  const AttributeInfoItem **getAttributeInfo() {
    return (const AttributeInfoItem **) attributeInfo;
  }
  
  uint16 getAttributeCount() {
    return attributeCount;
  }

  bool lookupConstant(uint16 index, ValueKind &vk, Value &value) const;

  /* Look up  name "name" in the constant-pool of this
   * class. If there is a constant of that name in the
   * constant pool of this class whose type is "type", and
   * whose type descriptor matches the string typeStr, 
   * return true and return the index of that item via "index". 
   * Else, return false
   *
   */
  bool lookupConstant(const char *name, const char *typeStr, 
		      uint8 type, uint16 &index) const;

  /* Convenience functions */

  /* Looks up a static field. If this method sets isConstant to true, 
   * it guarantees that the field will never change value from its 
   * current value (currently in memory at a).
   * Index is an index into the constant pool of the class
   */
  bool lookupStaticField(uint16 index, JavaType *&type, addr &a,
			 bool &isVolatile, bool &isConstant);

  /* Looks up an instance field. If this method sets isConstant to true, 
   * it guarantees that the field will never change value from its 
   * current value.
   * Index is an index into the constant pool of the class
   */
  bool lookupInstanceField(uint16 index, JavaType *&type, uint32 &offset,
			   bool &isVolatile, bool &isConstant);

  /* Looks up a method, and returns its signature and type. 
   * Index is an index into the constant pool of the class
   */
  bool lookupMethod(uint16 index, const JavaType *&ret, 
		    int &nParams, const JavaType *&paramTypes,
		    bool &isStatic, bool &isNative, bool &isAbstract);

#ifdef DEBUG
  void dump();
  void dumpClass(char *dumpfile);
  void dumpConstantPool(FILE *fp);
  void dumpInterfaces(FILE *fp);
  void dumpFields(FILE *fp);
  void dumpMethods(FILE *fp);
  void dumpAttributes(FILE *fp);
#endif

private:
  uint16 minorVersion, majorVersion;
  uint16 accessFlags;
  uint32 objSize;

  uint16 thisClassIndex, superClassIndex;
  
  ConstantPoolItem **constantPool;
  uint16 constantPoolCount;

  InterfaceInfo *interfaceInfo;
  uint16 interfaceCount;

  FieldInfo **fieldInfo;
  uint16 fieldCount;

  AttributeInfoItem **attributeInfo;
  uint16 attributeCount;

  MethodInfo **methodInfo;
  uint16 methodCount;
  
  class FileReader *fr;
  CrError readConstantPool();
  CrError readInterfaces();
  CrError readMethods();
  CrError readAttributes(AttributeInfoItem **attrInfo,
			 int attrCount);
  CrError readFields();

  CrError readInfoItems(int count, InfoItem **info, int field);

  /* Read one attribute from the current position in the file */
  AttributeInfoItem *readAttribute(CrError *status);
  
  AttributeHandler **attrHandlers;
  int numAttrHandlers;
  int attrSize;

  void addAttrHandler(AttributeHandler *handler);

  bool invalidIndex(int index) {
    return (index < 1 || index >= constantPoolCount);
  }

  Pool &p;
  StringPool &sp;
};


#endif /* _EF_ClassFileReader_H_ */



