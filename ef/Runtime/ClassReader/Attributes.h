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
#include "LogModule.h"

UT_EXTERN_LOG_MODULE(Debugger);

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
		    const char *nameInfo, Uint32 _code, Uint32 _length) :
    p(pool), name(nameInfo), length(_length), code(_code) {  } 
  
  Uint32 getLength() const {
    return length;
  }

  const char *getName() const {
    return name;
  }

  Uint32 getCode() const {
    return code;
  }

#ifdef DEBUG
  virtual void dump(int /*nIndents*/) const {
    
  }
#endif

protected:
  Pool &p;

private:
  const char *name;
  Uint32 length;
  Uint32 code;
};


class AttributeSourceFile : public AttributeInfoItem {
  friend class ClassFileReader;
public:
  AttributeSourceFile(Pool &pool, ConstantUtf8 *utf8) : 
  AttributeInfoItem(pool, "SourceFile", CR_ATTRIBUTE_SOURCEFILE, 2) {
    srcName = utf8;
  }
  
  ConstantUtf8 *getSrcName() const {
    return srcName;
  }
  
#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif

private:  
  ConstantUtf8 *srcName;
};


class AttributeConstantValue : public AttributeInfoItem {
public:
  AttributeConstantValue(Pool &pool, ConstantPoolItem *val, Int16 index) : 
      AttributeInfoItem(pool, "ConstantValue", CR_ATTRIBUTE_CONSTANTVALUE, 2), value(val), cindex(index) { }

  
  ConstantPoolItem *getValue() const {
    return value;
  }

  Uint16 getConstantPoolIndex() const {
      return cindex;
  }

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif

private:
  ConstantPoolItem *value;
  Uint16 cindex;
};


struct ExceptionItem {
#ifdef DEBUG
  void dump(int nIndents) const;
#endif

  Uint16 startPc, endPc, handlerPc, catchType;    
};


class AttributeCode : public AttributeInfoItem {

public:
  AttributeCode(Pool &pool, Uint16 _length, 
		Uint16 _maxStack, Uint16 _maxLocals, Uint32 _codeLength);

  bool setNumExceptions(Uint32 _numExceptions);

  bool setNumAttributes(Uint32 _numAttributes);

  /* Get the size of the code */
  Uint32 getCodeLength() const {
    return codeLength;
  }

  Uint32 getMaxStack() const {
    return maxStack;
  }

  Uint32 getMaxLocals() const {
    return maxLocals;
  }

  Uint32 getNumExceptions() const {
    return numExceptions;
  }

  const ExceptionItem *getExceptions() const {
    return exceptions;
  }

  Uint32 getNumAttributes() const {
    return numAttributes;
  }

  const AttributeInfoItem **getAttributes() const {
    return attributes;
  }
    
  /* Looks up an attribute by attribute-name */
  const AttributeInfoItem *getAttribute(const char *_name) const;

  const char *getCode() const {
    return (const char *) code;
  }

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif

private:
  Uint32 maxStack, maxLocals, codeLength, numExceptions;
  char *code;
  const ExceptionItem *exceptions;
  Uint32 numAttributes;
  const AttributeInfoItem **attributes;
};

class AttributeExceptions : public AttributeInfoItem {
friend class AttributeHandlerExceptions;
public:
  AttributeExceptions(Pool &pool, Uint32 _length) : 
  AttributeInfoItem(pool, "Exceptions", CR_ATTRIBUTE_EXCEPTIONS, _length) {
    numExceptions = 0, exceptions = 0;
  }

  Uint32 getNumExceptions() const {
    return numExceptions;
  }

  ConstantClass **getExceptions() const {
    return exceptions;
  }

  bool setNumExceptions(Uint32 n);

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif

private:		      
  Uint32 numExceptions;
  ConstantClass **exceptions;
};

typedef struct {
  Uint16 startPc;
  Uint16 lineNumber;

#ifdef DEBUG_LOG
  void dump(int nIndents) const {
    print(nIndents, "startPc %d lineNumber %d", startPc, lineNumber);
  }
#endif

} LineNumberEntry;


class AttributeLineNumberTable : public AttributeInfoItem {
public:
  AttributeLineNumberTable(Pool &pool, Uint32 _length) :
  AttributeInfoItem(pool, "LineNumberTable", CR_ATTRIBUTE_LINENUMBERTABLE, 
		    _length) {
    numEntries = 0, entries = 0;
  }

  bool setNumEntries(Uint32 n);

  Uint32 getNumEntries() const {
    return numEntries;
  }

  LineNumberEntry *getEntries() const {
    return entries;
  }
  
  /* returns the Pc value corresponding to lineNumber, -1 if not found */
  Int32 getPc(Uint32 lineNumber);

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif

private:
  Uint32 numEntries;
  LineNumberEntry *entries;
};

class VariableLocation {
	bool inRegister;		// Is this variable in a register ?
	bool invalid;			// The variable is dead at this point
	
	union {
		Uint8	reg;
		Uint8   mem;		// Need to decide a good way to represent a
						    // memory address. 
	};
		
 public:

#ifdef DEBUG_LOG
	void dump() {
		UT_LOG(Debugger, PR_LOG_ALWAYS, ("inreg: %d, reg: %d\n", inRegister,
										 reg));
	}
#endif

	VariableLocation() {
		invalid = true;
	}
	
	VariableLocation(bool _inRegister, Uint8 _reg) {
		inRegister = _inRegister;
		reg = _reg;
		invalid = false;
	}
	
};

//
// Interval mapping is a mapping from a contiguous block of native code to a
// physical location, where the value of a given local variable may be found.
//
class IntervalMapping : public DoublyLinkedEntry<IntervalMapping> {
 public:
	
	VariableLocation &getVariableLocation() { return loc; };
	void setVariableLocation(VariableLocation l) { loc = l; };

	IntervalMapping() : loc(VariableLocation(false, (Uint8) 0)) {};
	void setStart(Uint32 _start) { start = _start; }
	void setEnd(Uint32 _end) { end = _end; }

#ifdef DEBUG_LOG
	void dump() {
		UT_LOG(Debugger, PR_LOG_ALWAYS, ("start: %d, end: %d\n", start, end));
		loc.dump();
	}
#endif
	
 private:
	VariableLocation loc;		
	Uint32 start;		// The mapping is from the interval [start, end)
	Uint32 end;

};

typedef struct {
	Uint16 startPc;
	Uint16 length;
	ConstantUtf8 *name;
	ConstantUtf8 *descriptor;
	Uint16 index;
	DoublyLinkedList<IntervalMapping> mappings;

#ifdef DEBUG
	void dump(int nIndents) const {
		print(nIndents,
			  "startPc %d length %d index %d name %s desc %d", startPc, length,
			  index, name->getUtfString(), descriptor->getUtfString());
	}
#endif

#ifdef DEBUG_LOG
	void dump() {
		UT_LOG(Debugger, PR_LOG_ALWAYS, ("name: %s, startpc: %d, length: %d, "
			   "index %d\n", name->getUtfString(), startPc, length, index)); 
	}
#endif
} LocalVariableEntry;


class AttributeLocalVariableTable : public AttributeInfoItem {
friend class ClassFileReader;
  
public:
  AttributeLocalVariableTable(Pool &pool, Uint32 _length) : 
  AttributeInfoItem(pool, "LocalVariableTable", 
		    CR_ATTRIBUTE_LOCALVARIABLETABLE,
		    _length) {
    entries = 0;
  }
  

  bool setNumEntries(Uint32 n);

  Uint32 getNumEntries() const {
    return numEntries;
  }
  
  LocalVariableEntry *getEntries() const {
    return entries;
  }
  
  LocalVariableEntry *getEntry(const char *name);
  LocalVariableEntry *getEntry(Uint16 index);
  LocalVariableEntry& getEntryByNumber(Uint32 index) { return entries[index]; }

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif

private:
  Uint32 numEntries;
  LocalVariableEntry *entries;
};



#endif /* _CR_ATTRIBUTES_H_ */
