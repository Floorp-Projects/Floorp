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
#include "ErrorHandling.h"

#include "plstr.h"

/* Implementation of class AttributeHandler */
AttributeHandler::AttributeHandler(Pool &pool, FileReader *_reader, 
				   ClassFileReader *_cfr,
				   const char *_name): 
  p(pool), name(_name), reader(_reader), cfr(_cfr)
{
  constantPool = cfr->getConstantPool();
}

int AttributeHandler::validateNameAndLength(const char *_name, 
					    Uint32 *length) {
    
  if (_name && name && PL_strcmp(name, _name) != 0)
    return false;
    
  /* Get the length */
  if (!reader->readU4(length, 1)) 
    verifyError(VerifyError::noClassDefFound);

  return true;
}


#define READ_INDEX(index) \
Uint16 index;\
if (!reader->readU2(&index, 1)) \
  verifyError(VerifyError::noClassDefFound);

/* Implementation of AttributeHandlerSourceFile */
AttributeInfoItem *AttributeHandlerSourceFile::handle(const char *_name)
{
  Uint32 length;
  if (!validateNameAndLength(_name, &length)) 
    return 0;
    
  READ_INDEX(sourceFileIndex);

  if (invalidAttribute(sourceFileIndex, CR_CONSTANT_UTF8))
    verifyError(VerifyError::noClassDefFound);
    
  AttributeSourceFile *item = 
    new (p) 
    AttributeSourceFile(p, (ConstantUtf8 *) constantPool->get(sourceFileIndex));
  return item;
}

/* Implementation of AttributeHandlerConstantValue */
AttributeInfoItem *AttributeHandlerConstantValue::handle(const char *_name)
{
  Uint32 length;
  if (!validateNameAndLength(_name, &length))
    return 0;
  
  READ_INDEX(constantValueIndex);
  
  if (invalidIndex(constantValueIndex)) 
    verifyError(VerifyError::noClassDefFound);
  
  int type = constantPool->get(constantValueIndex)->getType();
  
  /* constantValueIndex must point to a long, float, double,
   * integer, or string
   */
  if (!((type > 2 && type < 7) ||
	(type == CR_CONSTANT_STRING))) 
    verifyError(VerifyError::noClassDefFound);
  
  AttributeConstantValue *item = new (p)
    AttributeConstantValue(p, const_cast<ConstantPoolItem *> (constantPool->get(constantValueIndex)), constantValueIndex);
  return item;
}


/* Implementation of AttributeHandlerCode */
AttributeInfoItem *AttributeHandlerCode::handle(const char *_name)
{
  Uint32 length;
  if (!validateNameAndLength(_name, &length))
    return 0;
  
  Uint16 maxStack, maxLocals;
  Uint32 codeLength;
  
  if (!reader->readU2(&maxStack, 1))
    verifyError(VerifyError::noClassDefFound);
  
  if (!reader->readU2(&maxLocals, 1))
    verifyError(VerifyError::noClassDefFound);
  
  if (!reader->readU4(&codeLength, 1))
    verifyError(VerifyError::noClassDefFound);

  AttributeCode *code = new (p) AttributeCode(p, (Uint16) length, maxStack, 
					      maxLocals, codeLength);
  
  char *codeStr = (char *) code->getCode();
  
  if (codeLength > 0) {
    if (!reader->readU1(codeStr, codeLength)) 
      verifyError(VerifyError::noClassDefFound);
  } 
  
  Uint16 numExceptions;
  if (!reader->readU2(&numExceptions, 1)) 
    verifyError(VerifyError::noClassDefFound);
  
  if (!code->setNumExceptions(numExceptions)) 
    verifyError(VerifyError::noClassDefFound);
  
  ExceptionItem *exceptions = const_cast<ExceptionItem *>
    (code->getExceptions());
  
  for (int i = 0; i < numExceptions; i++) {
    Uint16 array[4], catchIndex;
    
    /* Read in startPc, endPc, handlerPc, catchType */
    if (!reader->readU2(array, 4)) 
      verifyError(VerifyError::noClassDefFound);

    if ((catchIndex = array[3]) > 0 && (invalidAttribute(catchIndex, 
							 CR_CONSTANT_CLASS))) 
      verifyError(VerifyError::noClassDefFound);
    
    exceptions[i].startPc = array[0];
    exceptions[i].endPc = array[1];
    exceptions[i].handlerPc = array[2];
    exceptions[i].catchType = catchIndex;
  }
  
  Uint16 attrCount;
  /* Ok, so we can have nested attributes here */
  if (!reader->readU2(&attrCount, 1)) 
    verifyError(VerifyError::noClassDefFound);
  
  if (!code->setNumAttributes(attrCount))
    verifyError(VerifyError::noClassDefFound);
  
  AttributeInfoItem **attributes = (AttributeInfoItem **) 
    code->getAttributes();
  
  cfr->readAttributes(attributes, attrCount);  
  return code;
}


/* Implementation of AttributeHandlerLineNumberTable */
AttributeInfoItem *AttributeHandlerLineNumberTable::handle(const char *_name)
{
  Uint32 length;
  if (!validateNameAndLength(_name, &length)) 
    return 0;
  
  Uint16 numEntries;
  if (!reader->readU2(&numEntries, 1)) 
    verifyError(VerifyError::noClassDefFound);
  
  AttributeLineNumberTable *table = new (p) 
    AttributeLineNumberTable(p, length);
  
  if (!table->setNumEntries(numEntries)) 
    verifyError(VerifyError::noClassDefFound);
  
  LineNumberEntry *entries = table->getEntries();
  
  for (int i = 0; i < numEntries; i++) {
    Uint16 temp[2];
    
    if (!reader->readU2(temp, 2))
      verifyError(VerifyError::noClassDefFound);
    
    entries[i].startPc = temp[0];
    entries[i].lineNumber = temp[1];
  }
  
  return table;
}


/* Implementation of AttributeHandlerLocalVariableTable */
AttributeInfoItem *AttributeHandlerLocalVariableTable::handle(const char *_name)
{
  Uint32 length;
  if (!validateNameAndLength(_name, &length)) 
    return 0;
  
  Uint16 numEntries;
  if (!reader->readU2(&numEntries, 1)) 
    verifyError(VerifyError::noClassDefFound);
  
  AttributeLocalVariableTable *table = new (p) 
    AttributeLocalVariableTable(p, length);
  
  if (!table->setNumEntries(numEntries))     
    verifyError(VerifyError::noClassDefFound);

  LocalVariableEntry *entries = table->getEntries();
  
  for (int i = 0; i < numEntries; i++) {
    Uint16 temp[5];
    
    if (!reader->readU2(temp, 5))
      verifyError(VerifyError::noClassDefFound);
    
    entries[i].startPc = temp[0];
    entries[i].length = temp[1];
    if (invalidAttribute(temp[2], CR_CONSTANT_UTF8)) 
      verifyError(VerifyError::noClassDefFound);

    entries[i].name = (ConstantUtf8 *) constantPool->get(temp[2]);
    
    if (invalidAttribute(temp[3], CR_CONSTANT_UTF8))
      verifyError(VerifyError::noClassDefFound);

    entries[i].descriptor = (ConstantUtf8 *) constantPool->get(temp[3]);    
    entries[i].index = temp[4];
  }
  
  return table;
}


/* Implementation of AttributeHandlerExceptions */
AttributeInfoItem *AttributeHandlerExceptions::handle(const char *_name)
{
  Uint32 length;
  if (!validateNameAndLength(_name, &length)) 
    return 0;
  
  Uint16 numExceptions;
  
  if (!reader->readU2(&numExceptions, 1)) 
    verifyError(VerifyError::noClassDefFound);
  
  AttributeExceptions *exception = new (p) AttributeExceptions(p, length);
  ConstantClass **exceptionList;
  
  if (!exception->setNumExceptions(numExceptions)) 
    verifyError(VerifyError::noClassDefFound);
  
  exceptionList = exception->getExceptions();
  
  for (int i = 0; i < numExceptions; i++) {
    Uint16 index;
    
    if (!reader->readU2(&index, 1)) 
      verifyError(VerifyError::noClassDefFound);
    
    if (invalidAttribute(index, CR_CONSTANT_CLASS)) 
      verifyError(VerifyError::noClassDefFound);
    
    exceptionList[i] = (ConstantClass *) constantPool->get(index);
  }
  
  return exception;
}


/* Implementation of AttributeHandlerDummy */
AttributeInfoItem *AttributeHandlerDummy::handle(const char *_name)
{
  Uint32 length;
  if (!validateNameAndLength(_name, &length)) 
    return 0;
  
  char *data = new (p) char[length];
  
  /* Read the data */
  if (!reader->readU1(data, length)) 
    verifyError(VerifyError::noClassDefFound);
  
  AttributeInfoItem *item = new (p) AttributeInfoItem(p, name, 0, length);
  return item;
}
