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
#include "JavaTypes.h"
#include "prtypes.h"
#include "utils.h"
#include "Pool.h"

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

typedef enum {
  crErrorNone=0,
  crErrorFileNotFound,
  crErrorInvalidFileType,
  crErrorIO,
  crErrorNoMem,
  crErrorInvalidField,
  crErrorInvalidConstant,
  crErrorInvalidMethod,
  crErrorInvalidReference,
  crErrorInvalidInterface,
  crErrorInvalidAttribute,
  crErrorUnknownConstantType,
  crErrorSpuriousBytes,    /* Extra (unrecognized) bytes at end of file */
  crErrorSuperClassFinal,  /* Super-class is a final class and cannot be
			    * subclassed
			    */
  crErrorNoSuperClass,
  crErrorNoClass          /* No class defined in class file */
} CrError;

/* Denizens of the ConstantPool */
class ConstantPoolItem {
public:
  ConstantPoolItem(Pool &pIn, uint8 _type) : pool(0), p(pIn), type(_type) {

  }
    
  virtual uint8 getType() const {
    return type;
  }
  
  /* ResolveAndValidate indices into actual pointers */
  virtual int resolveAndValidate() {
    return true;
  }

#ifdef DEBUG
  virtual void dump(int /*indent*/) {   }
#endif
  
protected:
  ConstantPoolItem **pool;
  Pool &p;
  ConstantPoolItem(Pool &pIn, ConstantPoolItem **array, uint8 _type) :
    pool(array), p(pIn), type(_type) { }
  

private:  
  uint8 type;
};



class ConstantUtf8 : public ConstantPoolItem {
friend class ClassFileReader;
public:
  ConstantUtf8(Pool &p, char *str, int len);
  
  const char *getUtfString() {
    return data;
  }

  const int getUtfLen() {
    return dataLen;
  }
  
#ifdef DEBUG
  virtual void dump(int nIndents);
#endif

private:
  ConstantUtf8(Pool &p, int len);
  
  /* Pointer to the interned version of the string */
  const char *data;
  int dataLen;
};


/* A base class for constant-pool items that point to Utf strings */
class ConstantUtf : public ConstantPoolItem {
friend class ClassFileReader;
public:
  ConstantUtf(Pool &p, uint8 _type, ConstantUtf8 *utf8) : 
  ConstantPoolItem(p, _type) {
    utfString = utf8;
  }

  ConstantUtf8 *getUtf() {
    return utfString;
  }
  
protected:
  ConstantUtf(Pool &p, ConstantPoolItem **array, uint8 _type) : 
  ConstantPoolItem(p, array, _type) {
    utfString = 0;
  }

  ConstantUtf8 *utfString;
};



class ConstantClass : public ConstantUtf {
friend class ClassFileReader;
public:
  ConstantClass(Pool &p, ConstantUtf8 *utf8) : 
  ConstantUtf(p, CR_CONSTANT_CLASS, utf8) {
    utfString = utf8;
  }

  virtual ConstantUtf8 *getUtf() {
    return utfString;
  }
  
  virtual int resolveAndValidate();

#ifdef DEBUG
  virtual void dump(int nIndents);
#endif

private:
  ConstantClass(Pool &p, ConstantPoolItem **array, int _index) : 
  ConstantUtf(p, array, CR_CONSTANT_CLASS) {
    index = _index;
  }
  
  int index;
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
  ConstantUtf8 *getName() {
    return nameInfo;
  }

  /* returns a class that points to the type string */
  ConstantUtf8 *getDesc() {
    return descInfo;
  }

  virtual int resolveAndValidate();

#ifdef DEBUG
  virtual void dump(int nIndents);
#endif

private:
  ConstantNameAndType(Pool &p, 
		      ConstantPoolItem **array, int nindex, int dIndex);

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
  ConstantRef(Pool &p, uint8 _type, 
	      ConstantClass *cInfo, ConstantNameAndType *_typeInfo) : 
  ConstantPoolItem(p, _type) {
    classInfo = cInfo;
    typeInfo = _typeInfo;
  }
  
  ConstantClass *getClassInfo() {
    return classInfo;
  }

  ConstantNameAndType *getTypeInfo() {
    return typeInfo;
  }

  virtual int resolveAndValidate();

#ifdef DEBUG
  virtual void dump(int nIndents);
#endif

protected:
  ConstantRef(Pool &p, uint8 _type, ConstantPoolItem **array, int cIndex, 
	      int tIndex);
  
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
  virtual void dump(int nIndents);
#endif

private:
  ConstantFieldRef(Pool &p, ConstantPoolItem **array, int cIndex, 
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
virtual void dump(int nIndents);
#endif

private:
ConstantMethodRef(Pool &p, ConstantPoolItem **array, int cIndex, 
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
  virtual void dump(int nIndents);
#endif

private:
  ConstantInterfaceMethodRef(Pool &p, ConstantPoolItem **array, int cIndex, 
			     int tIndex) :
  ConstantRef(p, CR_CONSTANT_INTERFACEMETHODREF, array, cIndex, tIndex) {

  }

};


class ConstantString : public ConstantUtf {
friend class ClassFileReader;
public:
  ConstantString(Pool &p, ConstantUtf8 *utf8) : 
  ConstantUtf(p, CR_CONSTANT_STRING, utf8) {
    utfString = utf8;
  }

  ConstantUtf8 *getUtf() {
    return utfString;
  }
  
  virtual int resolveAndValidate();

#ifdef DEBUG
  virtual void dump(int nIndents);
#endif

private:
  ConstantString(Pool &p, ConstantPoolItem **array, int _index): 
  ConstantUtf(p, array, CR_CONSTANT_STRING) {
    index = _index;
  }

  int index;
};

/* Base class for all constant types that hold a numberic value */
class ConstantVal : public ConstantPoolItem {
public:
  ConstantVal(Pool &p, uint8 _type, ValueKind kind) : 
    ConstantPoolItem(p, _type), vk(kind) {

  }
  
  virtual Value getValue() {
    return value;
  }

  virtual ValueKind getValueKind() {
    return vk;
  }
  
#ifdef DEBUG
  virtual void dump(int nIndents);
#endif

protected:
  Value value;
  ValueKind vk;
};

class ConstantInt : public ConstantVal {
public:
  ConstantInt(Pool &p, int32 val): 
    ConstantVal(p, CR_CONSTANT_INTEGER, vkInt) {
    value.setValueContents(val);
  }
  
};


class ConstantFloat : public ConstantVal {
public:
  ConstantFloat(Pool &p, uint32 raw);
  
  uint32 getRaw() const { return raw; }
private:
  uint32 raw;
};


class ConstantLong : public ConstantVal {
public:
  ConstantLong(Pool &p, uint32 lo, uint32 hi);
    
  void getBits(uint32 &_lo, uint32 &_hi) const { _lo = lo, _hi = hi; }
private:
  uint32 lo, hi;
};


class ConstantDouble : public ConstantVal {
public:
  ConstantDouble(Pool &p, char *data);

  char *getData() { return data; }
private:
  char data[8];
};



#endif /* _CONSTANT_POOL_H_ */


