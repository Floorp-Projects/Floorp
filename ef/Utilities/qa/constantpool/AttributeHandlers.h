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

#ifndef _CR_ATTRIBUTEHANDLERS_H_
#define _CR_ATTRIBUTEHANDLERS_H_

#include "ClassReader.h"
#include "FileReader.h"


/* An Attribute handler is responsible for parsing and allocating
 * a particular attribute
 */
class AttributeHandler {
public:
  AttributeHandler(Pool &p, FileReader *_reader, ClassFileReader *_cfr,
		   const char *_name);

  /* The handle() method determines whether or not it can handle an
   * attribute whose name is _name. If it can, then it further parses the
   * file and returns a pointer to the attribute. Else, returns NULL
   */
  virtual AttributeInfoItem *handle(const char *_name, uint16 nindex, 
				    CrError *status) = 0;
  
  /* Get name of attribute that this handler will handle */
  const char *getName() {
    return name;
  }
  
  FileReader *getReader() {
    return reader;
  }

  ClassFileReader *getClassReader() {
    return cfr;
  }

protected:
  Pool &p;
  const char *name;
  FileReader *reader;
  ClassFileReader *cfr;
  ConstantPoolItem **constantPool;
  int constantPoolCount;

  int validateNameAndLength(const char *_name, uint32 *length,
			    CrError *status);

  bool invalidIndex(int index) {
    return index < 1 || index >= constantPoolCount;
  }

  bool invalidAttribute(int index, int type) {
    return (invalidIndex(index) || constantPool[index]->getType() != type);
  }
};

/* The following classes handle various attributes that we are
 * currently interested in.
 */
class AttributeHandlerSourceFile : public AttributeHandler {
public:
  AttributeHandlerSourceFile(Pool &pool, FileReader *_reader, 
			     ClassFileReader *_cfr):
  AttributeHandler(pool, _reader, _cfr, "SourceFile") {
    
  }
  
  virtual AttributeInfoItem *handle(const char *_name, uint16 nindex, CrError *status);

private:
};


class AttributeHandlerConstantValue : public AttributeHandler {
public:
  AttributeHandlerConstantValue(Pool &pool, 
				FileReader *_reader, ClassFileReader *_cfr):
  AttributeHandler(pool, _reader, _cfr, "ConstantValue") {
    
  }
  
  virtual AttributeInfoItem *handle(const char *_name, uint16 nindex, CrError *status);

private:
};


class AttributeHandlerCode : public AttributeHandler {
public:
  AttributeHandlerCode(Pool &pool, 
		       FileReader *_reader, ClassFileReader *_cfr):
  AttributeHandler(pool, _reader, _cfr, "Code") {
    
  }
  
  virtual AttributeInfoItem *handle(const char *_name, uint16 nindex, CrError *status);
  
private:
};


class AttributeHandlerLineNumberTable : public AttributeHandler {
public:
  AttributeHandlerLineNumberTable(Pool &pool, 
				  FileReader *_reader, 
				  ClassFileReader *_cfr):
  AttributeHandler(pool, _reader, _cfr, "LineNumberTable") {
    
  }
  
  virtual AttributeInfoItem *handle(const char *_name, uint16 nindex, CrError *status);

private:
  
};


class AttributeHandlerLocalVariableTable : public AttributeHandler {
public:
  AttributeHandlerLocalVariableTable(Pool &pool, FileReader *_reader, 
				     ClassFileReader *_cfr):
  AttributeHandler(pool, _reader, _cfr, "LocalVariableTable") {
    
  }
  
  virtual AttributeInfoItem *handle(const char *_name, uint16 nindex, CrError *status);

private:
  
};


class AttributeHandlerExceptions : public AttributeHandler {
public:
  AttributeHandlerExceptions(Pool &pool, FileReader *_reader, 
			     ClassFileReader *_cfr):
  AttributeHandler(pool, _reader, _cfr, "Exceptions") {
    
  }
  
  virtual AttributeInfoItem *handle(const char *_name, uint16 nindex, CrError *status);

private:

};

/* This handles attributes that we don't know (or care) about */
class AttributeHandlerDummy : public AttributeHandler {
public:
  AttributeHandlerDummy(Pool &pool, 
			FileReader *_reader, ClassFileReader *_cfr):
  AttributeHandler(pool, _reader, _cfr, 0) {
    
  }
  
  virtual AttributeInfoItem *handle(const char *_name, uint16 nindex, CrError *status);

private:
};

#endif /* _CR_ATTRIBUTEHANDLERS_H_ */
