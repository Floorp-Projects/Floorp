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
#ifndef _CR_ATTRIBUTES_H_
#define _CR_ATTRIBUTES_H_

#include "ConstantPool.h"
#include "DoublyLinkedList.h"

/* Attribute tags for handlers that we know (care) about */
#define CR_ATTRIBUTE_SOURCEFILE              1
#define CR_ATTRIBUTE_CONSTANTVALUE           2
#define CR_ATTRIBUTE_LINENUMBERTABLE         3
#define CR_ATTRIBUTE_LOCALVARIABLETABLE      4
#define CR_ATTRIBUTE_CODE                    5
#define CR_ATTRIBUTE_EXCEPTIONS              6


class AttributeInfoItem : public DoublyLinkedEntry<AttributeInfoItem> {
public:
  AttributeInfoItem(Pool &pool,
		    const char *nameInfo, uint32 _code, uint32 _length,
		    uint16 _nameIndex) :
    p(pool), name(nameInfo), length(_length), code(_code),
    nameIndex(_nameIndex) {  } 
  
  uint32 getLength() const {
    return length;
  }

  const char *getName() const {
    return name;
  }

  uint32 getCode() const {
    return code;
  }

  uint16 getNameIndex() const {
    return nameIndex;
  }
#ifdef DEBUG
  virtual void dump(int /*nIndents*/) {
    
  }
#endif

protected:
  Pool &p;

private:
  const char *name;
  uint32 length;
  uint32 code;
  uint16 nameIndex;
};


class AttributeSourceFile : public AttributeInfoItem {
  friend class ClassFileReader;
public:
  AttributeSourceFile(Pool &pool, ConstantUtf8 *utf8, uint16 i, uint16 nindex) : 
    AttributeInfoItem(pool, "SourceFile", CR_ATTRIBUTE_SOURCEFILE, 2, nindex), index(i) {
    srcName = utf8;
  }
  
  ConstantUtf8 *getSrcName() const {
    return srcName;
  }
  
  uint16 getIndex() const {
    return index;
  }
			   
#ifdef DEBUG
  virtual void dump(int nIndents);
#endif

private:  
  ConstantUtf8 *srcName;
  uint16 index;
};


class AttributeConstantValue : public AttributeInfoItem {
public:
  AttributeConstantValue(Pool &pool, ConstantPoolItem *val, uint16 i, uint16 nindex) : 
  AttributeInfoItem(pool, "ConstantValue", CR_ATTRIBUTE_CONSTANTVALUE, 2, nindex),
    index(i) {
    value = val;
  }
  
  ConstantPoolItem *getValue() const {
    return value;
  }

  uint16 getIndex() const {
    return index;
  }

#ifdef DEBUG
  virtual void dump(int nIndents);
#endif

private:
  ConstantPoolItem *value;
  uint16 index;
};


class ExceptionItem {
public:
  ExceptionItem(uint16 _startPc, uint16 _endPc, 
		uint16 _handlerPc, ConstantClass *_catcher, uint16 cIndex);
  
  uint16 getStartPc() const {
    return startPc;
  }
  
  uint16 getEndPc() const {
    return endPc;
  }
  
  uint16 getHandlerPc() const {
    return handlerPc;
  }
  
  ConstantClass *getCatcher() const {
    return catcher;
  }

  uint16 getCatcherIndex() const {
    return catcherIndex;
  }

#ifdef DEBUG
  void dump(int nIndents);
#endif

private:
  uint16 startPc, endPc, handlerPc;
  uint16 catcherIndex;
  ConstantClass *catcher;
};


class AttributeCode : public AttributeInfoItem {

public:
  AttributeCode(Pool &pool, uint16 _length, 
		uint16 _maxStack, uint16 _maxLocals, uint32 _codeLength,
		uint16 nindex,
		CrError *status);

  int setNumExceptions(int _numExceptions);

  int setNumAttributes(int _numAttributes);

  /* Get the size of the code */
  uint32 getCodeLength() const {
    return codeLength;
  }

  uint32 getMaxStack() const {
    return maxStack;
  }

  uint32 getMaxLocals() const {
    return maxLocals;
  }

  uint32 getNumExceptions() const {
    return numExceptions;
  }

  ExceptionItem **getExceptions() const {
    return exceptions;
  }

  uint32 getNumAttributes() const {
    return numAttributes;
  }

  AttributeInfoItem **getAttributes() const {
    return attributes;
  }

  const char *getCode() const {
    return (const char *) code;
  }

#ifdef DEBUG
  virtual void dump(int nIndents);
#endif

private:
  uint32 maxStack, maxLocals, codeLength, numExceptions;
  char *code;
  ExceptionItem **exceptions;
  uint16 numAttributes;
  AttributeInfoItem **attributes;
};


class AttributeExceptions : public AttributeInfoItem {
friend class AttributeHandlerExceptions;
public:
  AttributeExceptions(Pool &pool, uint32 _length, uint16 nindex) : 
  AttributeInfoItem(pool, "Exceptions", CR_ATTRIBUTE_EXCEPTIONS, _length, nindex) {
    numExceptions = 0, exceptions = 0;
  }

  uint16 getNumExceptions() const {
    return numExceptions;
  }

  ConstantClass **getExceptions() const {
    return exceptions;
  }

  int setNumExceptions(uint16 n);

  uint16 *getExcIndices() const {
    return excIndices;
  }

#ifdef DEBUG
  virtual void dump(int nIndents);
#endif

private:		      
  uint16 numExceptions;
  ConstantClass **exceptions;
  uint16 *excIndices;
};

typedef struct {
  uint16 startPc;
  uint16 lineNumber;

#ifdef DEBUG
  void dump(int nIndents) {
    print(nIndents, "startPc %d lineNumber %d", startPc, lineNumber);
  }
#endif

} LineNumberEntry;


class AttributeLineNumberTable : public AttributeInfoItem {
public:
  AttributeLineNumberTable(Pool &pool, uint32 _length, uint16 nindex) :
  AttributeInfoItem(pool, "LineNumberTable", CR_ATTRIBUTE_LINENUMBERTABLE, 
		    _length, nindex) {
    numEntries = 0, entries = 0;
  }

  int setNumEntries(uint16 n);

  uint16 getNumEntries() {
    return numEntries;
  }

  LineNumberEntry *getEntries() {
    return entries;
  }
  
  /* returns the Pc value corresponding to lineNumber, -1 if not found */
  int getPc(uint16 lineNumber);

#ifdef DEBUG
  virtual void dump(int nIndents);
#endif

private:
  uint16 numEntries;
  LineNumberEntry *entries;
};


typedef struct {
  uint16 startPc;
  uint16 length;
  ConstantUtf8 *name;
  uint16 nameIndex;
  ConstantUtf8 *descriptor;
  uint16 descIndex;
  uint16 index;

#ifdef DEBUG
  void dump(int nIndents) {
    print(nIndents,
	  "startPc %d length %d name %s desc %s", startPc, length,
	  name->getUtfString(), descriptor->getUtfString());
  }
#endif
} LocalVariableEntry;


class AttributeLocalVariableTable : public AttributeInfoItem {
friend class ClassFileReader;
  
public:
  AttributeLocalVariableTable(Pool &pool, uint32 _length, uint16 nindex) : 
  AttributeInfoItem(pool, "LocalVariableTable", 
		    CR_ATTRIBUTE_LOCALVARIABLETABLE,
		    _length, nindex) {
    entries = 0;
  }
  

  int setNumEntries(uint16 n);

  uint16 getNumEntries() {
    return numEntries;
  }
  
  LocalVariableEntry *getEntries() {
    return entries;
  }
  
  /* Not yet implemented */
  LocalVariableEntry *getEntry(const char * /*_name*/) {
    return 0;
  }

#ifdef DEBUG
  virtual void dump(int nIndents);
#endif

private:
  uint16 numEntries;
  LocalVariableEntry *entries;
};



#endif /* _CR_ATTRIBUTES_H_ */
