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
#include "AttributeHandlers.h"

#ifndef NO_NSPR
#include "plstr.h"
#endif

/* Implementation of class AttributeHandler */
AttributeHandler::AttributeHandler(Pool &pool, FileReader *_reader, 
				   ClassFileReader *_cfr,
				   const char *_name): 
  p(pool), name(_name), reader(_reader), cfr(_cfr)
{
  constantPool = (ConstantPoolItem **) cfr->getConstantPool();
  constantPoolCount = cfr->getConstantPoolCount();
}

int AttributeHandler::validateNameAndLength(const char *_name, 
					    uint32 *length,
					    CrError *status) {
  *status = crErrorNone;
    
#ifdef NO_NSPR
  if (_name && name && strcmp(name, _name) != 0)
#else
  if (_name && name && PL_strcmp(name, _name) != 0)
#endif
    return false;
    
  /* Get the length */
  if (!reader->readU4(length, 1)) {
    *status = crErrorIO;
    return false;
  }
    
  return true;
}


#define READ_INDEX(index) \
uint16 index;\
if (!reader->readU2(&index, 1)) {\
  *status = crErrorIO;\
  return 0;\
}

/* Implementation of AttributeHandlerSourceFile */
AttributeInfoItem *AttributeHandlerSourceFile::handle(const char *_name, 
						      uint16 nindex, 
						      CrError *status)
{
  uint32 length;
  if (!validateNameAndLength(_name, &length, status)) 
    return 0;
    
  READ_INDEX(sourceFileIndex);

  if (invalidAttribute(sourceFileIndex, CR_CONSTANT_UTF8)) {
    *status = crErrorInvalidAttribute;
    return 0;
  }
    
  AttributeSourceFile *item = 
    new (p) 
    AttributeSourceFile(p, (ConstantUtf8 *) constantPool[sourceFileIndex],
			sourceFileIndex, nindex);
    return item;
}

/* Implementation of AttributeHandlerConstantValue */
AttributeInfoItem *AttributeHandlerConstantValue::handle(const char *_name, 
							 uint16 nindex, 
							 CrError *status)
{
  uint32 length;
  if (!validateNameAndLength(_name, &length, status))
    return 0;
  
  READ_INDEX(constantValueIndex);
  
  if (invalidIndex(constantValueIndex)) {
    *status = crErrorInvalidAttribute;
    return 0;
  }
  
  int type = constantPool[constantValueIndex]->getType();
  
  /* constantValueIndex must point to a long, float, double,
   * integer, or string
   */
  if (!((type > 2 && type < 7) ||
	(type == CR_CONSTANT_STRING))) {
#ifdef DEBUG
    printf("Invalid CONSTANT_VALUE attribute: type is %d", type);
#endif
    
    *status = crErrorInvalidAttribute;
    return 0;
  }
  
  AttributeConstantValue *item = new (p)
    AttributeConstantValue(p, constantPool[constantValueIndex],
			   constantValueIndex, nindex);
  return item;
}


/* Implementation of AttributeHandlerCode */
AttributeInfoItem *AttributeHandlerCode::handle(const char *_name, 
						uint16 nindex, 
						CrError *status)  {
  uint32 length;
  if (!validateNameAndLength(_name, &length, status))
    return 0;
  
  uint16 maxStack, maxLocals;
  uint32 codeLength;
  
  if (!reader->readU2(&maxStack, 1)) {
    *status = crErrorIO;
    return 0;
  }
  
  if (!reader->readU2(&maxLocals, 1)) {
    *status = crErrorIO;
    return 0;
  }
  
  if (!reader->readU4(&codeLength, 1)) {
    *status = crErrorIO;
    return 0;
  }

#if 0
  printf("---> maxStack %d maxLocals %d codeLength %d\n", 
	 maxStack, maxLocals, codeLength);
#endif

  AttributeCode *code = new (p) AttributeCode(p, length, maxStack, 
					      maxLocals, codeLength, 
					      nindex, 
					      status);
  
  if (*status != crErrorNone)
    return 0;
  
  char *codeStr = (char *) code->getCode();
  
  if (codeLength > 0) {
    if (!reader->readU1(codeStr, codeLength)) {
      *status = crErrorIO;
      return 0;
    }
  } 
#ifdef DEBUG
  else {
    print(0, "Warning: CODE array has length <= 0 in class %s",
	  cfr->getThisClass()->getUtf()->getUtfString());
  }
#endif
  
  uint16 numExceptions;
  if (!reader->readU2(&numExceptions, 1)) {
    *status = crErrorIO;
    return 0;
  }
  
  if (!code->setNumExceptions(numExceptions)) {
    *status = crErrorNoMem;
    return 0;
  }
  
  ExceptionItem **exceptions = code->getExceptions();
  
  for (int i = 0; i < numExceptions; i++) {
    uint16 array[4], catchIndex;
    
    /* Read in startPc, endPc, handlerPc, catchType */
    if (!reader->readU2(array, 4)) {
      *status = crErrorIO;
      return 0;
    }
    
    if ((catchIndex = array[3]) > 0 && (invalidAttribute(catchIndex, 
							  CR_CONSTANT_CLASS))) {
#ifdef DEBUG
      print(0, "AttributeHandlerCode(): invalid catchIndex %d, type %d",
	    catchIndex, constantPool[catchIndex]->getType());
#endif
      
      *status = crErrorInvalidAttribute;
      return 0;
    }
    
    if (catchIndex > 0) 
      exceptions[i] = new (p) ExceptionItem(array[0], array[1], array[2],
					    (ConstantClass *) constantPool[catchIndex],
					    catchIndex);
    else
      exceptions[i] = new (p) ExceptionItem(array[0], array[1], array[2],
					    0,
					    0);

  }
  
  uint16 attrCount;
  /* Ok, so we can have nested attributes here */
  if (!reader->readU2(&attrCount, 1)) {
    *status = crErrorIO;
    return 0;
  }
  
  if (!code->setNumAttributes(attrCount)) {
    *status = crErrorNoMem;
    return 0;
  }
  
  AttributeInfoItem **attributes = code->getAttributes();
  
  if ((*status = cfr->readAttributes(attributes, attrCount)) != 0) {
    return 0;
  }
  
  return code;
}


/* Implementation of AttributeHandlerLineNumberTable */
AttributeInfoItem *AttributeHandlerLineNumberTable::handle(const char *_name, 
							   uint16 nindex, 
							   CrError *status)
{
  uint32 length;
  if (!validateNameAndLength(_name, &length, status)) 
    return 0;
  
  uint16 numEntries;
  if (!reader->readU2(&numEntries, 1)) {
    *status = crErrorIO;
    return 0;
  }
  
  AttributeLineNumberTable *table = new (p) 
    AttributeLineNumberTable(p, length, nindex);
  
  if (!table->setNumEntries(numEntries)) {
    *status = crErrorNoMem;
    return 0;
  }
  
  LineNumberEntry *entries = table->getEntries();
  
  for (int i = 0; i < numEntries; i++) {
    uint16 temp[2];
    
    if (!reader->readU2(temp, 2)) {
      *status = crErrorNoMem;
      return 0;
    }
    
    entries[i].startPc = temp[0];
    entries[i].lineNumber = temp[1];
  }
  
  return table;
}


/* Implementation of AttributeHandlerLocalVariableTable */
AttributeInfoItem *AttributeHandlerLocalVariableTable::handle(const char *_name, 
							      uint16 nindex, 
							      CrError *status)
{
  uint32 length;
  if (!validateNameAndLength(_name, &length, status)) 
    return 0;
  
  uint16 numEntries;
  if (!reader->readU2(&numEntries, 1)) {
    *status = crErrorIO;
    return 0;
  }
  
  AttributeLocalVariableTable *table = new (p) 
    AttributeLocalVariableTable(p, length, nindex);
  
  if (!table->setNumEntries(numEntries)) {
    *status = crErrorNoMem;
    return 0;
  }
  
  LocalVariableEntry *entries = table->getEntries();
  
  for (int i = 0; i < numEntries; i++) {
    uint16 temp[5];
    
    if (!reader->readU2(temp, 5)) {
      *status = crErrorNoMem;
      return 0;
    }
    
    entries[i].startPc = temp[0];
    entries[i].length = temp[1];
    if (invalidAttribute(temp[2], CR_CONSTANT_UTF8)) {
      *status = crErrorInvalidConstant;
      return 0;
    }
    entries[i].name = (ConstantUtf8 *) constantPool[temp[2]];
    entries[i].nameIndex = temp[2];

    if (invalidAttribute(temp[3], CR_CONSTANT_UTF8)) {
      *status = crErrorInvalidConstant;
      return 0;
    }
    entries[i].descriptor = (ConstantUtf8 *) constantPool[temp[3]];
    entries[i].descIndex = temp[3];

    entries[i].index = temp[4];
  }
  
  return table;
}


/* Implementation of AttributeHandlerExceptions */
AttributeInfoItem *AttributeHandlerExceptions::handle(const char *_name, 
						      uint16 nindex, 
						      CrError *status)
{
  uint32 length;
  if (!validateNameAndLength(_name, &length, status)) 
    return 0;
  
  uint16 numExceptions;
  
  if (!reader->readU2(&numExceptions, 1)) {
    *status = crErrorIO;
    return 0;
  }
  
  AttributeExceptions *exception = new (p) AttributeExceptions(p, length,
							       nindex);
  ConstantClass **exceptionList;
  
  if (!exception->setNumExceptions(numExceptions)) {
    *status = crErrorNoMem;
    return 0;
  }
  
  exceptionList = exception->getExceptions();
  uint16 *excIndices = exception->getExcIndices();

  for (int i = 0; i < numExceptions; i++) {
    uint16 index;
    
    if (!reader->readU2(&index, 1)) {
      *status = crErrorIO;
      return 0;
    }
    
    if (invalidAttribute(index, CR_CONSTANT_CLASS)) {
      *status = crErrorIO;
      return 0;
    }
    
    exceptionList[i] = (ConstantClass *) constantPool[index];
    excIndices[i] = index;
  }
  
  return exception;
}


/* Implementation of AttributeHandlerDummy */
AttributeInfoItem *AttributeHandlerDummy::handle(const char *_name, 
						 uint16 nindex, 
						 CrError *status)
{
  uint32 length;
  if (!validateNameAndLength(_name, &length, status)) 
    return 0;
  
  char *data = new (p) char[length];
  
  /* Read the data */
  if (!reader->readU1(data, length)) {
    *status = crErrorIO;
    return 0;
  }  
  
  AttributeInfoItem *item = new (p) AttributeInfoItem(p, name, 0, length,
						      nindex);
  return item;
}
