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
#ifndef _CONSTANT_POOL_H_
#define _CONSTANT_POOL_H_

#include "FloatUtils.h"
#include "prtypes.h"
#include "utils.h"
#include "Pool.h"
#include "ErrorHandling.h"
#include "Value.h"
#include "LogModule.h"

/* Constant pool types */
#define CR_CONSTANT_CLASS                7
#define CR_CONSTANT_FIELDREF             9
#define CR_CONSTANT_METHODREF           10
#define CR_CONSTANT_INTERFACEMETHODREF  11
#define CR_CONSTANT_STRING               8
#define CR_CONSTANT_INTEGER              3
#define CR_CONSTANT_FLOAT                4
#define CR_CONSTANT_LONG                 5
#define CR_CONSTANT_DOUBLE               6
#define CR_CONSTANT_NAMEANDTYPE         12
#define CR_CONSTANT_UTF8                 1

class ConstantPoolItem;

/* The Constant Pool */
class ConstantPool {
private:
  const ConstantPoolItem **items;
  Uint32 numItems;

public:
  /* Only ClassFileReader can create the constant pool */
  ConstantPool(Uint32 numItems) : numItems(numItems) {
    items = new const ConstantPoolItem *[numItems];
  }

  /* Set the item at given index in the constant pool. */
  void set(Uint32 index, const ConstantPoolItem *item) {
    assert(index < numItems); items[index] = item;
  }

  /* Get the constant pool item at index "index" in the constant pool. */
  const ConstantPoolItem *get(Uint32 index) const {
    return items[index];
  }

  /* return the number of items in the constant pool */
  Uint32 count() const {
    return numItems;
  }

#ifdef DEBUG_LOG
  void printItem(LogModuleObject &f, Uint32 index) const;
#endif
};


/* Denizens of the ConstantPool */
class ConstantPoolItem {
protected:
  ConstantPoolItem(Pool &pool, Uint8 type) :
    p(pool), type(type) { }

public:    
  Uint8 getType() const {
    return type;
  }
  
  /* ResolveAndValidate indices into actual pointers; return true if successful */
  virtual bool resolveAndValidate() {
    return true;
  }

#ifdef DEBUG
  virtual void dump(int indent) const;
#endif
#ifdef DEBUG_LOG
  virtual void print(LogModuleObject &f) const;
#endif
  
protected:
  // Pool for allocating dynamic storage for this ConstantPoolItem
  Pool &p;

private:  
  // CR_CONSTANT_ type of this ConstantPoolItem
  Uint8 type;
};



class ConstantUtf8 : public ConstantPoolItem {
public:
  ConstantUtf8(Pool &p, const char *str, Uint32 len);
  
  const char *getUtfString() const {
    return data;
  }

  const int getUtfLen() const {
    return dataLen;
  }
  
#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif
#ifdef DEBUG_LOG
  void print(LogModuleObject &f) const;
#endif

private:
  /* Pointer to the interned version of the string */
  const char *data;
  Uint32 dataLen;
};


/* A base class for constant-pool items that point to Utf strings */
class ConstantUtf : public ConstantPoolItem {
friend class ClassFileReader;
public:
  ConstantUtf(Pool &p, Uint8 _type, ConstantUtf8 *utf8) : 
  ConstantPoolItem(p, _type) {
    utfString = utf8;
  }

  ConstantUtf8 *getUtf() const {
    return utfString;
  }
  
protected:
  ConstantUtf(Pool &p, Uint8 _type) : 
  ConstantPoolItem(p, _type) {
    utfString = 0;
  }

  ConstantUtf8 *utfString;
};



class ConstantClass : public ConstantUtf {
public:
  static ConstantClass &cast(ConstantPoolItem &item) {
    if (item.getType() != CR_CONSTANT_CLASS) 
      verifyError(VerifyError::noClassDefFound);
    return *static_cast<ConstantClass *>(&item);
  }

  virtual bool resolveAndValidate();

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif
#ifdef DEBUG_LOG
  void print(LogModuleObject &f) const;
#endif

  ConstantClass(Pool &p, ConstantPool *array, int _index) : 
    ConstantUtf(p, CR_CONSTANT_CLASS), index(_index), pool(array) {}
private:
  int index;
  // The constant pool of which this ConstantPoolItem is a member
  ConstantPool *pool;
};


class ConstantNameAndType : public ConstantPoolItem {
friend class ClassFileReader;
public:
  ConstantNameAndType(Pool &p, ConstantUtf8 *name, ConstantUtf8 *desc):
  ConstantPoolItem(p, CR_CONSTANT_NAMEANDTYPE) {
    nameInfo = name;
    descInfo = desc;
  }
  
  /* Get the name of the constant */
  ConstantUtf8 *getName() const {
    return nameInfo;
  }

  /* returns a class that points to the type string */
  ConstantUtf8 *getDesc() const {
    return descInfo;
  }

  virtual bool resolveAndValidate();

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif
#ifdef DEBUG_LOG
  void print(LogModuleObject &f) const;
#endif

private:
  ConstantNameAndType(Pool &p, 
		      ConstantPool *array, Uint16 nindex, Uint16 dIndex);

  // The constant pool of which this ConstantPoolItem is a member
  ConstantPool *pool;

  ConstantUtf8 *nameInfo;
  ConstantUtf8 *descInfo;

  int nameIndex, descIndex;
};


/* Base class used by ConstantFieldRef, ConstantMethodRef, 
 * ConstantInterfaceMethodRef: members of a class
 */
class ConstantRef : public ConstantPoolItem {
friend class ClassFileReader;
public:
  ConstantRef(Pool &p, Uint8 _type, 
	      ConstantClass *cInfo, ConstantNameAndType *_typeInfo) : 
  ConstantPoolItem(p, _type) {
    classInfo = cInfo;
    typeInfo = _typeInfo;
  }
  
  ConstantClass *getClassInfo() const {
    return classInfo;
  }

  ConstantNameAndType *getTypeInfo() const {
    return typeInfo;
  }

  virtual bool resolveAndValidate();

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif
#ifdef DEBUG_LOG
  void print(LogModuleObject &f) const;
#endif

protected:
  ConstantRef(Pool &p, Uint8 _type, ConstantPool *array, Uint16 cIndex, 
	      Uint16 tIndex);
  
  // The constant pool of which this ConstantPoolItem is a member
  ConstantPool *pool;

  ConstantClass *classInfo;
  ConstantNameAndType *typeInfo;
 
  int classIndex, typeIndex;
};


class ConstantFieldRef : public ConstantRef {
friend class ClassFileReader;
public:
  ConstantFieldRef(Pool &p, ConstantClass *cInfo, 
		   ConstantNameAndType *_type):
  ConstantRef(p, CR_CONSTANT_FIELDREF, cInfo, _type) {

  }

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif

private:
  ConstantFieldRef(Pool &p, ConstantPool *array, int cIndex, 
		   int tIndex) :
  ConstantRef(p, CR_CONSTANT_FIELDREF, array, cIndex, tIndex) {

  }
};

class ConstantMethodRef : public ConstantRef {
friend class ClassFileReader;

public:
  ConstantMethodRef(Pool &p, ConstantClass *cInfo, 
		    ConstantNameAndType *_type):
  ConstantRef(p, CR_CONSTANT_METHODREF, cInfo, _type) {
    
  }

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif

private:
ConstantMethodRef(Pool &p, ConstantPool *array, int cIndex, 
		  int tIndex) :
  ConstantRef(p, CR_CONSTANT_METHODREF, array, cIndex, tIndex) {
    
  }

};

class ConstantInterfaceMethodRef : public ConstantRef {
friend class ClassFileReader;

public:
  ConstantInterfaceMethodRef(Pool &p, ConstantClass *cInfo, 
			     ConstantNameAndType *_type):
  ConstantRef(p, CR_CONSTANT_INTERFACEMETHODREF, cInfo, _type) {

  }

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif

private:
  ConstantInterfaceMethodRef(Pool &p, ConstantPool *array, int cIndex, 
			     int tIndex) :
    ConstantRef(p, CR_CONSTANT_INTERFACEMETHODREF, array, cIndex, tIndex) {

  }

};


class ConstantString : public ConstantUtf {
friend class ClassFileReader;
public:
  ConstantString(Pool &p, ConstantUtf8 *utf8) : 
  ConstantUtf(p, CR_CONSTANT_STRING, utf8)  {
    utfString = utf8;
  }

  ConstantUtf8 *getUtf() const {
    return utfString;
  }
  
  virtual bool resolveAndValidate();

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif
#ifdef DEBUG_LOG
  void print(LogModuleObject &f) const;
#endif

  ConstantString(Pool &p, ConstantPool *array, int _index): 
		ConstantUtf(p, CR_CONSTANT_STRING), pool(array), index(_index) { }

private:
  // The constant pool of which this ConstantPoolItem is a member
  ConstantPool *pool;

  int index;
};

/* Base class for all constant types that hold a numberic value */
class ConstantVal : public ConstantPoolItem {
public:
  ConstantVal(Pool &p, Uint8 _type, ValueKind kind) : 
    ConstantPoolItem(p, _type), vk(kind) {

  }
  
  Value getValue() const {
    return value;
  }

  ValueKind getValueKind() const {
    return vk;
  }
  
#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif
#ifdef DEBUG_LOG
  void print(LogModuleObject &f) const;
#endif

protected:
  Value value;
  ValueKind vk;
};

class ConstantInt : public ConstantVal {
public:
  ConstantInt(Pool &p, Int32 val): 
    ConstantVal(p, CR_CONSTANT_INTEGER, vkInt) {
    value.setValueContents(val);
  }
  
};


class ConstantFloat : public ConstantVal {
public:
  ConstantFloat(Pool &p, Uint32 raw);
};


class ConstantLong : public ConstantVal {
public:
  ConstantLong(Pool &p, Uint32 lo, Uint32 hi);
    
};


class ConstantDouble : public ConstantVal {
public:
  ConstantDouble(Pool &p, char *data);

};


#endif /* _CONSTANT_POOL_H_ */


