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
#include "ClassReader.h"


#include "AttributeHandlers.h"
#include "ErrorHandling.h"
#include "DebugUtils.h"
#include "LogModule.h"

#include <stdio.h>
#include "plstr.h"

UT_DEFINE_LOG_MODULE(ClassReader);

#ifdef DEBUG_LOG
#include <stdarg.h>

void indent(int n) 
{
  for (; n; n--)
    UT_LOG(ClassReader, PR_LOG_ALWAYS, ("  ")); 
}
  
void title(int nIndents, const char *s)
{
  indent(nIndents);
  UT_LOG(ClassReader, PR_LOG_ALWAYS, ("%s\n", s)); 
}

void print(int nIndents, const char *fmt, ...)
{
  va_list args;
  static char buffer[2048];

  indent(nIndents);
  va_start(args, fmt);
  vsprintf(buffer, fmt, args);		// FIX-ME use the mallocing sprintf
  va_end(args);

  UT_LOG(ClassReader, PR_LOG_ALWAYS, ("%s\n", buffer)); 
}
#endif



/* Implementation of ConstantPool */
#ifdef DEBUG_LOG
//
// Print the given item of the constant pool in a compact format, never
// spanning more than one line and not including a trailing newline.
// print ???? if there is no such item.
//
void ConstantPool::printItem(LogModuleObject &f, Uint32 index) const
{
	if (index == 0)
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("NIL"));
	else if (index < numItems && items[index])
		items[index]->print(f);
	else
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("????"));
}
#endif


#ifdef DEBUG
/* Implementation of ConstantPoolItem */
void ConstantPoolItem::dump(int indent) const
{
  ::print(indent, "Unknown item %d", type);
}
#endif

#ifdef DEBUG_LOG
//
// Print this constant pool item in a compact format, never
// spanning more than one line and not including a trailing newline.
//
void ConstantPoolItem::print(LogModuleObject &f) const
{
  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("???? item %d", type));
}
#endif


/* Implementation of ConstantUtf8 */
ConstantUtf8::ConstantUtf8(Pool &pool, const char *str, Uint32 len) : 
  ConstantPoolItem(pool, CR_CONSTANT_UTF8) 
{
  data = str;
  dataLen = len;
}

#ifdef DEBUG_LOG
void ConstantUtf8::dump(int nIndents) const
{    
  title(nIndents, "CONSTANT_UTF8");
  ::print(nIndents, "String: %s", data);
}

void ConstantUtf8::print(LogModuleObject &f) const
{ 
  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s", data)); 
}
#endif


/* Implementation of ConstantClass */
bool ConstantClass::resolveAndValidate()
{
  if (pool)
    utfString = (ConstantUtf8 *) pool->get(index);
  
  return (utfString && utfString->getType() == CR_CONSTANT_UTF8);
}

#ifdef DEBUG
void ConstantClass::dump(int nIndents) const
{
  title(nIndents, "CONSTANT_CLASS");
  utfString->dump(nIndents+1);
}
#endif

#ifdef DEBUG_LOG
void ConstantClass::print(LogModuleObject &f) const
{    
  utfString->print(f);
}
#endif

/* Implementation of ConstantNameAndType */
bool ConstantNameAndType::resolveAndValidate()
{
  if (pool) {
    nameInfo = (ConstantUtf8 *) pool->get(nameIndex);
    descInfo = (ConstantUtf8 *) pool->get(descIndex);
  }
  
  return (nameInfo && descInfo &&
	  nameInfo->getType() == CR_CONSTANT_UTF8 &&
	  descInfo->getType() == CR_CONSTANT_UTF8);
}

#ifdef DEBUG
void ConstantNameAndType::dump(int nIndents) const
{
  title(nIndents, "CONSTANT_NAMEANDTYPE (name/desc)");
  nameInfo->dump(nIndents+1);
  descInfo->dump(nIndents+1);
}
#endif

#ifdef DEBUG_LOG
void ConstantNameAndType::print(LogModuleObject &f) const
{    
  nameInfo->print(f);
  UT_OBJECTLOG(f, PR_LOG_ALWAYS, (":"));
  descInfo->print(f);
}
#endif

ConstantNameAndType::ConstantNameAndType(Pool &pool,
					 ConstantPool *array, 
					 Uint16 nindex, Uint16 dIndex) : 
  ConstantPoolItem(pool, CR_CONSTANT_NAMEANDTYPE), pool(array)
{
  nameIndex = nindex, descIndex = dIndex;
  nameInfo = descInfo = 0;
}

/* Implementation of ConstantRef */
bool ConstantRef::resolveAndValidate()
{
  if (pool) {
    classInfo = (ConstantClass *) pool->get(classIndex);
    typeInfo = (ConstantNameAndType *) pool->get(typeIndex);
  }
  
  return (classInfo && typeInfo &&
	  classInfo->getType() == CR_CONSTANT_CLASS &&
	  typeInfo->getType() == CR_CONSTANT_NAMEANDTYPE);
}

#ifdef DEBUG
void ConstantRef::dump(int nIndents) const
{
  ::print(nIndents+2, "Printing ClassInfo and typeInfo");
  classInfo->dump(nIndents+1);
  typeInfo->dump(nIndents+1);
}
#endif

#ifdef DEBUG_LOG
void ConstantRef::print(LogModuleObject &f) const
{    
  classInfo->print(f);
  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("."));
  typeInfo->print(f);
}
#endif

ConstantRef::ConstantRef(Pool &pool,
			 Uint8 _type, ConstantPool *array, Uint16 cIndex, 
			 Uint16 tIndex) : ConstantPoolItem(pool, _type), pool(array)
{
  classIndex = cIndex, typeIndex = tIndex;
  classInfo = 0;
  typeInfo = 0;
}

/* Implementation of ConstantFieldRef */
#ifdef DEBUG
void ConstantFieldRef::dump(int nIndents) const
{
  title(nIndents, "CONSTANT_FIELD_REF");
  ConstantRef::dump(nIndents);
}

/* Implementation of ConstantMethodRef */
void ConstantMethodRef::dump(int nIndents) const {
  title(nIndents, "CONSTANT_METHOD_REF");
  ConstantRef::dump(nIndents);
}

/* Implementation of class ConstantInterfaceMethodRef */
void ConstantInterfaceMethodRef::dump(int nIndents) const {
    title(nIndents, "CONSTANT_INTERFACEMETHOD_REF");
    ConstantRef::dump(nIndents);
  }
#endif

/* Implementation of ConstantString */
bool ConstantString::resolveAndValidate()
{
  if (pool) 
    utfString = (ConstantUtf8 *) pool->get(index);
   
  return (utfString && utfString->getType() == CR_CONSTANT_UTF8);
}

#ifdef DEBUG
void ConstantString::dump(int nIndents) const
{
  title(nIndents, "CONSTANT_STRING");
  utfString->dump(nIndents+1);
}
#endif

#ifdef DEBUG_LOG
void ConstantString::print(LogModuleObject &f) const
{    
  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\""));
  utfString->print(f);
  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\""));
}
#endif

/* Implementation of ConstantVal */
#ifdef DEBUG_LOG
void ConstantVal::dump(int nIndents) const
{
  switch (vk) {
  case vkInt:
    title(nIndents, "CONSTANT_INT");
    ::print(nIndents, "Value: %d", value.i);
    break;

  case vkFloat:
    title(nIndents, "CONSTANT_FLOAT");
    ::print(nIndents, "Value: %f", value.f);
    break;

  case vkDouble:
    title(nIndents, "CONSTANT_DOUBLE");
    ::print(nIndents, "Value: %lf", value.d);
    break;

  case vkLong:
    title(nIndents, "CONSTANT_LONG");
	indent(nIndents);
	printInt64(UT_LOG_MODULE(ClassReader), value.l);
	UT_LOG(ClassReader, PR_LOG_ALWAYS, ("\n"));
    break;
      
  default:
    break;
  }
}
#endif

#ifdef DEBUG_LOG
void ConstantVal::print(LogModuleObject &f) const
{
  value.print(f, vk);
}
#endif

/* Implementation of ConstantFloat */
ConstantFloat::ConstantFloat(Pool &pool, Uint32 raw): 
  ConstantVal(pool, CR_CONSTANT_FLOAT, vkFloat) 
{

  value.setValueContents(*((Flt32 *) &raw));

#if 0
  if (raw == 0x7f80000)
    value.setValueContents(floatPositiveInfinity);
  else if (raw == 0xff800000)
    value.setValueContents(floatNegativeInfinity);
  else if ((raw >= 0x7f800001 && raw <= 0x7fffffff) ||
	   (raw >= 0xff800001 && raw <= 0xffffffff))
    value.setValueContents(floatNaN);
  else {
    
    Uint32 s = ((raw >> 31) == 0) ? 1 : -1;
    Uint32 e = ((raw >> 23) &0xff);
    Uint32 m = (e == 0) ? (raw & 0x7fffff) << 1: 
    (raw & 0x7fffff) | 0x800000;
    
    value.setValueContents((Flt32) s * m * pow(2.0, (double)e-150));
  }
#endif
}

/* Implementation of ConstantLong */
ConstantLong::ConstantLong(Pool &pool, Uint32 lo, Uint32 hi) : 
  ConstantVal(pool, CR_CONSTANT_LONG, vkLong) 
{
#ifdef HAVE_LONG_LONG
  Int64 lw, llo, lhi;

  llo = (Int64) lo;
  lhi = (Int64) hi;
  lhi <<= 32;
  
  lw = llo | lhi;
  
  value.setValueContents(lw);
#else
  value.l.lo = lo;
  value.l.hi = hi;
#endif  
}


/* Implementation of ConstantDouble */
ConstantDouble::ConstantDouble(Pool &pool, char *data): 
  ConstantVal(pool, CR_CONSTANT_DOUBLE, vkDouble)
{
  union {
    unsigned char c[8];
    Flt64 d;
  } u;
  
#ifdef IS_LITTLE_ENDIAN
  u.c[0] = data[7];
  u.c[1] = data[6];
  u.c[2] = data[5];
  u.c[3] = data[4];

  u.c[4] = data[3];
  u.c[5] = data[2];
  u.c[6] = data[1];
  u.c[7] = data[0];     
#else
  u.c[0] = data[0];
  u.c[1] = data[1];
  u.c[2] = data[2];
  u.c[3] = data[3];

  u.c[4] = data[4];
  u.c[5] = data[5];
  u.c[6] = data[6];
  u.c[7] = data[7];
#endif
  value.d = u.d;  
}

/* Implementation of AttributeSourceFile */
#ifdef DEBUG_LOG
void AttributeSourceFile::dump(int nIndents) const {    
  title(nIndents, "SOURCE-FILE");    
  srcName->dump(nIndents+1);
}
#endif

/* Implementation of AttributeConstantValue */
#ifdef DEBUG_LOG
void AttributeConstantValue::dump(int nIndents) const
{
  title(nIndents, "CONSTANT-VALUE");    
  value->dump(nIndents+1);
}
#endif

#ifdef DEBUG
void ExceptionItem::dump(int nIndents) const {    
  title(nIndents, "ExceptionItem");
  print(nIndents+1, 
	"Start %u End %u Handler %u Catcher %u", startPc, endPc, 
	handlerPc, catchType);
}
#endif

/* Implementation of AttributeCode */
AttributeCode::AttributeCode(Pool &pool, Uint16 _length, 
			     Uint16 _maxStack, Uint16 _maxLocals, 
			     Uint32 _codeLength) :
  AttributeInfoItem(pool, "Code", CR_ATTRIBUTE_CODE, _length)
{
  maxStack = _maxStack, maxLocals = _maxLocals, 
    codeLength = _codeLength;
  
  numExceptions = 0;
  exceptions = 0;
  numAttributes = 0;
  attributes = 0;
  
  if (codeLength > 0)
    code = new (p) char[codeLength];
  else
    code = 0;
}

bool AttributeCode::setNumExceptions(Uint32 _numExceptions)
{
  exceptions = new (p) ExceptionItem[_numExceptions];
  numExceptions = _numExceptions;
  return true;
}

bool AttributeCode::setNumAttributes(Uint32 _numAttributes)
{
  attributes = new (p) const AttributeInfoItem *[_numAttributes];
  numAttributes = _numAttributes;
  return true;
}

const AttributeInfoItem *
AttributeCode::getAttribute(const char *_name) const
{
    for (Uint32 i = 0; i < numAttributes; i++) {
		const AttributeInfoItem &attribute = *attributes[i];
		
		if (!(PL_strncmp(_name, attribute.getName(), attribute.getLength())))
			return &attribute;
	}
	
	return NULL;
}

#ifdef DEBUG
void AttributeCode::dump(int nIndents) const {
  Uint32 i;
  
  title(nIndents, "CODE");
  ::print(nIndents, 
	"maxStack %u maxLocals %u codeLength %u numExceptions %u",
	maxStack, maxLocals, codeLength, numExceptions);

  if (numExceptions > 0) {
    title(nIndents, "Exceptions");
    for (i = 0; i < numExceptions; i++)
      exceptions[i].dump(nIndents+1);
  }

  if (numAttributes > 0) {
    title(nIndents, "Attributes");
    for (i = 0; i < numAttributes; i++)
      attributes[i]->dump(nIndents+1);
  }
}
#endif

/* Implementation of AttributeExceptions */
bool AttributeExceptions::setNumExceptions(Uint32 n)
{
  exceptions = new (p) ConstantClass *[n];
  numExceptions = n;
  return true;
}

#ifdef DEBUG
void AttributeExceptions::dump(int nIndents) const
{
  Uint32 i;
  
  title(nIndents, "EXCEPTIONS");
  ::print(nIndents, "numExceptions %d", numExceptions);
  
  if (numExceptions > 0) {
    title(nIndents, "Exceptions");
    for (i = 0; i < numExceptions; i++)
      exceptions[i]->dump(nIndents+1);
  }
}
#endif

/* Implementation of AttributeLineNumberTable */
bool AttributeLineNumberTable::setNumEntries(Uint32 n)
{
  entries = new (p) LineNumberEntry[n];
  numEntries = n;
  return true;
}

Int32 AttributeLineNumberTable::getPc(Uint32 lineNumber) {
  Uint32 i;
  
  /* The line numbers are not guaranteed to be in order */
  for (i = 0; i < numEntries; i++)
    if (entries[i].lineNumber == lineNumber)
      return entries[i].startPc;
  
  return -1;
}
  
#ifdef DEBUG
void AttributeLineNumberTable::dump(int nIndents) const
{
  title(nIndents, "LINE-NUMBER-ENTRY");
  
  for (Uint32 i = 0; i < numEntries; i++)
    entries[i].dump(nIndents+1);
}
#endif


/* Implementation of AttributeLocalVariableTable */
bool AttributeLocalVariableTable::setNumEntries(Uint32 n) {
  entries = new (p) LocalVariableEntry[n];
  numEntries = n;
  return true;    
}

//
// Looks up a local variable by name
// Returns a pointer to the LocalVariableEntry or NULL if not found
//
LocalVariableEntry *
AttributeLocalVariableTable::getEntry(const char *name)
{
	for (Uint32 i=0; i < numEntries; i++) {
		if (PL_strcmp(entries[i].name->getUtfString(), name) == 0)
			return &entries[i];
	}

	return NULL;
}

//
// Looks up a local variable by index
// Returns a pointer to the LocalVariableEntry or NULL if not found
//
LocalVariableEntry *
AttributeLocalVariableTable::getEntry(Uint16 index)
{
	for (Uint32 i=0; i < numEntries; i++) {
		if (entries[i].index == index)
			return &entries[i];
	}

	return NULL;
}

#ifdef DEBUG
void AttributeLocalVariableTable::dump(int nIndents) const
{
  title(nIndents, "LOCAL-VARIABLE-TABLE");
  
  for (Uint32 i = 0; i < numEntries; i++)
    entries[i].dump(nIndents+1);
}
#endif

/* Implementation of InfoItem */
InfoItem::InfoItem(Pool &pool, Uint16 aflags, ConstantUtf8 *classInfo,
		   ConstantUtf8 *nameInfo, 
		   ConstantUtf8 *descInfo) : p(pool)
{
  accessFlags = aflags;
  name = nameInfo;
  descriptor = descInfo;
  className = classInfo;
  attrCount = 0;    
}

bool InfoItem::addAttribute(AttributeInfoItem &attribute)
{
  attributes.addLast(attribute);  
  attrCount++;
  return true;
}

AttributeInfoItem *InfoItem::getAttribute(const char *_name) const
{
  for (DoublyLinkedList<AttributeInfoItem>::iterator i = attributes.begin();
       !attributes.done(i); i = attributes.advance(i)) {
    AttributeInfoItem &attribute = attributes.get(i);

    if (!(PL_strncmp(_name, attribute.getName(), attribute.getLength())))
      return &attribute;
  }
    
  return 0;
}

AttributeInfoItem *InfoItem::getAttribute(const Uint32 code) const
{
  for (DoublyLinkedList<AttributeInfoItem>::iterator i = attributes.begin();
       !attributes.done(i); i = attributes.advance(i)) {
    AttributeInfoItem &attribute = attributes.get(i);

    if (attribute.getCode() == code)
      return &attribute;
  }
  
  return 0;
}

  
#ifdef DEBUG_LOG
void InfoItem::dump(int nIndents) const
{
  print(nIndents, "%s() descriptor:%s",
	name->getUtfString(), descriptor->getUtfString());
  print(nIndents, "AccessFlags %x attrCount %d", 
	accessFlags, attrCount);
  
  if (attrCount > 0) {
    print(nIndents, "-> Attributes (%d):", attrCount);
    for (DoublyLinkedList<AttributeInfoItem>::iterator i = 
	   attributes.begin();
	 !attributes.done(i); i = attributes.advance(i))
      attributes.get(i).dump(nIndents+1);
  }
}
#endif  

/* Implementation of FieldInfo */
void FieldInfo::getInfo(const char *&sig,
			bool &isVolatile, bool &isConstant,
			bool &isStatic) const
{
  Uint16 aFlags = getAccessFlags();

  isVolatile = (aFlags & CR_FIELD_VOLATILE) != 0;
  isConstant = false;  
  isStatic = (aFlags & CR_FIELD_STATIC) != 0;
  sig = getDescriptor()->getUtfString();
}


#ifdef DEBUG_LOG
void FieldInfo::dump(int nIndents) const
{
  InfoItem::dump(nIndents);

  if (getAccessFlags() & CR_FIELD_STATIC)
    print(nIndents, "Field is STATIC");
  else
    print(nIndents, "Field is an instance field");
  
}
#endif  

/* Implementation of MethodInfo */
void MethodInfo::getInfo(const char *&sig,
			 bool &isAbstract, bool &isStatic,
			 bool &isFinal, bool &isSynchronized,
			 bool &isNative) const
{
  sig = getDescriptor()->getUtfString();
  Uint16 aFlags = getAccessFlags();
  isAbstract = (aFlags & CR_METHOD_ABSTRACT) != 0;
  isStatic = (aFlags & CR_METHOD_STATIC) != 0;
  isFinal = (aFlags & CR_METHOD_FINAL) != 0;
  isSynchronized = (aFlags & CR_METHOD_SYNCHRONIZED) != 0;
  isNative = (aFlags & CR_METHOD_NATIVE) != 0;
}


/* Implementation of ClassFileReader */
/* Look up a constant of type integer, long, float, or double in
 * the constant pool of the class. Returns true on success,
 * false if it couldn't find the field or on error.
 */
bool ClassFileReader::lookupConstant(Uint16 index, ValueKind &vk, 
				     Value &value) const 
{
  if (index < 1 || index >= constantPool->count())
    return false;

  /* Are we dealing with a constant that returns a value? */    
  int type = constantPool->get(index)->getType();

  /* String constants */
  if (type == CR_CONSTANT_STRING) {
    vk = vkAddr;
    
    ConstantString *str = (ConstantString *) constantPool->get(index);    
    JavaString *string = sp.getStringObj(str->getUtf()->getUtfString());
    assert(string);
    value.a = staticAddress((void *) string);
    return true;
  }

  /* If we're here, index must point to a long, float, double, or integer */
  if (!((type > 2 && type < 7)))
    return false;
    
  ConstantVal *val = (ConstantVal *) constantPool->get(index);
  vk = val->getValueKind();
  value = val->getValue();
    
  return true;
}

#ifdef DEBUG_LOG
void ClassFileReader::dump() const
{
  Uint32 i;

  print(0, "AccessFlags %x", accessFlags);
  print(0, "Major Version %d, Minor Version %d", majorVersion, 
	minorVersion);

  print(0, "\nThis Class:");
  constantPool->get(thisClassIndex)->dump(0);
  
  if (superClassIndex > 0) {
    print(0, "\nSuper Class:");
    constantPool->get(superClassIndex)->dump(0);
  }
  
  print(0, "\nConstant Pool Count: %d", constantPool->count());
  for (i = 1; i < constantPool->count(); i++) {  
    print(0, "%d.", i);
    
    if (constantPool->get(i))
      constantPool->get(i)->dump(0);
    else
      print(0, "NULL Constant-pool entry, probably result of a previous\n"
	    "DOUBLE or LONG");    
  }
  
  print(0, "\nField Count: %d", fieldCount);
  for (i = 0; i < fieldCount; i++) {
    print(0, "%d.", i+1);
    fieldInfo[i]->dump(0);
  }
  
  print(0, "\nInterface Count: %d", interfaceInfo->count());
  for (i = 0; i < interfaceInfo->count(); i++) {
    print(0, "%d.", i+1);
    interfaceInfo->get(i)->dump(0);
  }
  
  print(0, "\nMethod Count: %d", methodCount);
  for (i = 0; i < methodCount; i++) {
    print(0, "%d.", i+1);
    methodInfo[i]->dump(0);
  }
}
#endif

#define CR_BUFFER_SIZE 16184


#define JAVA_MAGIC 0xCAFEBABE

/* Reads and parses the constant pool section of the class file.
 * constantPoolCount must have been initialized and the constantPool
 * array (but not its elements) allocated before calling this.
 */
void ClassFileReader::readConstantPool()
{
  Uint16 cIndex, tIndex;
  Uint16 index;
  Uint32 low, high;
  Uint32 i;
  constantPool->set(0, 0);

  Uint32 constantPoolCount = constantPool->count();

  for (i = 1; i < constantPoolCount; i++) {
    char tag;

    if (!fr.readU1(&tag, 1)) 
      verifyError(VerifyError::noClassDefFound);

    switch (tag) {
    case CR_CONSTANT_UTF8: {
      Uint16 len;

      if (!fr.readU2(&len, 1)) 
	verifyError(VerifyError::noClassDefFound);
      
      char *buf = new char[len+1];
      fr.readU1(buf, len);
      buf[len] = 0;
      
      const char *str = sp.intern(buf);
      delete [] buf;
      ConstantUtf8 *utf = new (p) ConstantUtf8(p, str, len);      

      constantPool->set(i, utf);
      break;
    }

    case CR_CONSTANT_INTEGER: {
      Int32 val;


      if (!fr.readU4((Uint32 *) &val, 1)) 
	verifyError(VerifyError::noClassDefFound);

      constantPool->set(i, new (p) ConstantInt(p, val));
      break;
    }

    case CR_CONSTANT_FLOAT: {
      Uint32 raw;

      if (!fr.readU4(&raw, 1)) 
	verifyError(VerifyError::noClassDefFound);

      constantPool->set(i, new (p) ConstantFloat(p, raw));
      break;
    }

    case CR_CONSTANT_LONG:
      if (!fr.readU4(&high, 1)) 
	verifyError(VerifyError::noClassDefFound);

      if (!fr.readU4(&low, 1)) 
	verifyError(VerifyError::noClassDefFound);

      constantPool->set(i, new (p) ConstantLong(p, low, high));
      constantPool->set(i+1, 0);
      
      /* Longs and doubles take up two constant-pool entries */
      i++;
      break;
      
    case CR_CONSTANT_DOUBLE: {
      char buf[8];

      if (!fr.readU1(buf, 8)) 
	verifyError(VerifyError::noClassDefFound);

      constantPool->set(i, new (p) ConstantDouble(p, buf));
      constantPool->set(i+1, 0);

      /* Longs and doubles take up two constant-pool entries */
      i++;
      break;
    }

    case CR_CONSTANT_CLASS:
      if (!fr.readU2(&index, 1)) 
	verifyError(VerifyError::noClassDefFound);
      
      if (invalidIndex(index)) 
	verifyError(VerifyError::noClassDefFound);

      constantPool->set(i, new (p) ConstantClass(p, constantPool, index));
      break;

    case CR_CONSTANT_STRING:
      if (!fr.readU2(&index, 1)) 
	verifyError(VerifyError::noClassDefFound);

      if (invalidIndex(index)) 
	verifyError(VerifyError::noClassDefFound);

      constantPool->set(i, new (p) ConstantString(p, constantPool, index));
      break;


    case CR_CONSTANT_FIELDREF:
      if (!fr.readU2(&cIndex, 1)) 
	verifyError(VerifyError::noClassDefFound);

      if (!fr.readU2(&tIndex, 1))
	verifyError(VerifyError::noClassDefFound);

      if (invalidIndex(cIndex) || invalidIndex(tIndex))
	verifyError(VerifyError::noClassDefFound);
	
      constantPool->set(i, new (p) ConstantFieldRef(p, constantPool, 
						   cIndex, tIndex));
      break;

    case CR_CONSTANT_METHODREF:
      if (!fr.readU2(&cIndex, 1)) 
	verifyError(VerifyError::noClassDefFound);

      if (!fr.readU2(&tIndex, 1)) 
	verifyError(VerifyError::noClassDefFound);

      if (invalidIndex(cIndex) || invalidIndex(tIndex))
	verifyError(VerifyError::noClassDefFound);

      constantPool->set(i, new (p) ConstantMethodRef(p, constantPool,
						    cIndex, tIndex));
      break;

    case CR_CONSTANT_INTERFACEMETHODREF:
      if (!fr.readU2(&cIndex, 1)) 
	verifyError(VerifyError::noClassDefFound);

      if (!fr.readU2(&tIndex, 1)) 
	verifyError(VerifyError::noClassDefFound);

      if (invalidIndex(cIndex) || invalidIndex(tIndex)) 
	verifyError(VerifyError::noClassDefFound);

      constantPool->set(i, new (p) ConstantInterfaceMethodRef(p, constantPool, 
							     cIndex, tIndex));
      break;
      
    case CR_CONSTANT_NAMEANDTYPE: {
      Uint16 nIndex, dIndex;

      if (!fr.readU2(&nIndex, 1)) 	
	verifyError(VerifyError::noClassDefFound);

      if (!fr.readU2(&dIndex, 1))
	verifyError(VerifyError::noClassDefFound);

      if (invalidIndex(nIndex) || invalidIndex(dIndex))
	verifyError(VerifyError::noClassDefFound);

      constantPool->set(i, new (p) ConstantNameAndType(p, constantPool, 
						      nIndex, dIndex));
      
      break;
    }

    default:
	verifyError(VerifyError::noClassDefFound);
    }
  } 
  
  for (i = 1; i < constantPoolCount; i++) {
    ConstantPoolItem *item = const_cast<ConstantPoolItem *>(constantPool->get(i));
    if (item && !item->resolveAndValidate()) 
      verifyError(VerifyError::noClassDefFound);
  }
}


/* Reads and parses the interface section of the class file.
 * interfaceCount must have been initialized and the interfaceInfo
 * array allocated before calling this.
 */
void ClassFileReader::readInterfaces()
{
  Uint32 interfaceCount = interfaceInfo->count();

  for (Uint32 i = 0; i < interfaceCount; i++) {
    Uint16 index;

    if (!fr.readU2(&index, 1))
 	verifyError(VerifyError::noClassDefFound);

    const ConstantPoolItem *item = constantPool->get(index);
    if (invalidIndex(index) || item->getType() != CR_CONSTANT_CLASS) 
      verifyError(VerifyError::noClassDefFound);
    
    interfaceInfo->set(i, const_cast<ConstantPoolItem *>(item));
  }
}


/* Read an attribute at the current position in the file, classify it
 * using the list of attribute handlers, and return the resulting attribute.
 * Never returns nil.
 */
AttributeInfoItem *ClassFileReader::readAttribute()
{
  Uint16 nameIndex;
  AttributeInfoItem *item;
  ConstantUtf8 *utf8;

  if (!fr.readU2(&nameIndex, 1))
    verifyError(VerifyError::noClassDefFound);

  if (invalidIndex(nameIndex))
    verifyError(VerifyError::noClassDefFound);
  
  const ConstantPoolItem *constantPoolItem = constantPool->get(nameIndex);
  if (constantPoolItem->getType() != CR_CONSTANT_UTF8)
    verifyError(VerifyError::noClassDefFound);
  
  utf8 = (ConstantUtf8 *) constantPoolItem;
  
  for (Int32 i = 0; i < numAttrHandlers; i++)
    if ((item = attrHandlers[i]->handle(utf8->getUtfString())) != 0)
      return item;

  verifyError(VerifyError::noClassDefFound);
  return 0;
}


/* Read count InfoItems from the file and store them into the info array.
 * If field is true, creates the FieldInfo array; otherwise creates the
 * MethodInfo array.
 */
void ClassFileReader::readInfoItems(Uint32 count, InfoItem **info, bool field)
{
  for (Uint32 i = 0; i < count; i++) {
    Uint16 aFlags, nameIndex, descIndex, attrCount;
    
    if (!fr.readU2(&aFlags, 1))
	verifyError(VerifyError::noClassDefFound);

    if (!fr.readU2(&nameIndex, 1))
	verifyError(VerifyError::noClassDefFound);

    if (invalidIndex(nameIndex)) 
	verifyError(VerifyError::noClassDefFound);

    if (!fr.readU2(&descIndex, 1))
	verifyError(VerifyError::noClassDefFound);

    if (invalidIndex(descIndex)) 
	verifyError(VerifyError::noClassDefFound);

    if (!fr.readU2(&attrCount, 1))
	verifyError(VerifyError::noClassDefFound);

    if (constantPool->get(nameIndex)->getType() != CR_CONSTANT_UTF8 ||
	constantPool->get(descIndex)->getType() != CR_CONSTANT_UTF8) 
	verifyError(VerifyError::noClassDefFound);
    
    ConstantUtf8 *desc = (ConstantUtf8 *) constantPool->get(descIndex);
    ConstantUtf8 *name = (ConstantUtf8 *) constantPool->get(nameIndex);
      
    if (field) {
      FieldInfo *fInfo = new (p) FieldInfo(p, aFlags, 
					   (ConstantUtf8 *) constantPool->get(thisClassIndex),
					   name,
					   desc);
      

      info[i] = fInfo;

      /* If this field is public, add it to the public field array */
      if (aFlags & CR_FIELD_PUBLIC) 
	publicFields[publicFieldCount++] = (Uint16) i;

      /* Add this field into the hashtable for faster lookup */
      InfoNode *node = new (p) InfoNode(desc->getUtfString(), i);      
      fieldTable.add(name->getUtfString(), node);
    } else {
      MethodInfo *mInfo;

      mInfo = new (p) MethodInfo(p, aFlags, 
				 ((ConstantClass *) constantPool->get(thisClassIndex))->getUtf(),
				 name,
				 desc);
            
      info[i] = mInfo;

      const char *methodName = ((ConstantUtf8 *) constantPool->get(nameIndex))->getUtfString();
      
      /* If this method is <init>, then add it to the Constructor array.  
       * Else, if it is public, add it to the list of public methods.
       */
      if (PL_strcmp(methodName, "<clinit>") != 0) {
	  if (!PL_strcmp(methodName, "<init>")) {
	      constructors[constructorCount++] = (Uint16) i;
	      
	  if ((aFlags & CR_METHOD_PUBLIC))
	      publicConstructors[publicConstructorCount++] = (Uint16) i;
	  
	  } else {
	      if (aFlags & CR_METHOD_PUBLIC)
		  publicMethods[publicMethodCount++] = (Uint16) i;
	      
	      declaredMethods[declaredMethodCount++] = (Uint16) i;
	  }
      }

      /* add this method into the method hash table for faster lookup */
      InfoNode *node = new (p) InfoNode(desc->getUtfString(), i);
      methodTable.add(name->getUtfString(), node);
    }

    for (Uint32 j = 0; j < attrCount; j++) {
      AttributeInfoItem *attr = readAttribute();
      info[i]->addAttribute(*attr);
    }
  }
}


/* Reads and parses the methods section of the class file.
 * methodCount must have been initialized and the methodInfo
 * array allocated before calling this.
 */
void ClassFileReader::readMethods()
{
  readInfoItems(methodCount, (InfoItem **) methodInfo, false);  
}


/* Reads and parses the fields section of the class file.
 * fieldCount must have been initialized and the fieldInfo
 * array allocated before calling this.
 */
void ClassFileReader::readFields()
{
  readInfoItems(fieldCount, (InfoItem **) fieldInfo, true);
}


/* Reads and parses attrCount attributes starting at the current
 * place in the class file.  Stores the attributes into the attrInfo
 * array, which should have been allocated before calling this.
 */
void ClassFileReader::readAttributes(AttributeInfoItem **attrInfo,
					Uint32 attrCount)
{
  for (Uint32 i = 0; i < attrCount; i++)
    attrInfo[i] = readAttribute();
}

/* Reads and parses the class file given by the canonical name fileName.
 * Throws a VerifyError on errors or failures. pool is used to allocate
 * all dynamic structures during the process of parsing the file. strPool
 * is used to intern all strings encountered during the parsing.
 */
ClassFileReader::ClassFileReader(Pool &pool, StringPool &strPool,
				 FileReader &fr) :
  constantPool(0), 
  interfaceInfo(0), fieldInfo(0), fieldCount(0), 
  attributeInfo(0), attributeCount(0),
  methodInfo(0), methodCount(0), 
  publicFields(0), publicFieldCount(0),
  publicMethods(0), publicMethodCount(0),
  declaredMethods(0), declaredMethodCount(0),
  constructors(0), constructorCount(0),
  publicConstructors(0), publicConstructorCount(0),
  p(pool), sp(strPool), fr(fr), fieldTable(pool), methodTable(pool)
{
  Uint32 magic;
  
  numAttrHandlers = 0;
  attrSize = 10;
  attrHandlers = new (p) AttributeHandler *[attrSize];

  if (!fr.readU4(&magic, 1))
    verifyError(VerifyError::noClassDefFound);
    
  if (magic != JAVA_MAGIC)
    verifyError(VerifyError::noClassDefFound);
  
  /* get the version numbers */
  if (!fr.readU2(&minorVersion, 1) || !fr.readU2(&majorVersion, 1))
    verifyError(VerifyError::noClassDefFound);

  /* XXX Need to put a check in here to see if we can handle the version
   * numbers in the file
   */

  Uint16 constantPoolCount;
  /* Constant pool count */
  if (!fr.readU2(&constantPoolCount, 1))
    verifyError(VerifyError::noClassDefFound);

  constantPool = new (p) ConstantPool(constantPoolCount);  

  /* Parse the constant pool */
  readConstantPool();

  /* Register attribute handlers for all attributes that we can
   * handle 
   */
  addAttrHandler(new (p) AttributeHandlerSourceFile(p, &fr, this));
  addAttrHandler(new (p) AttributeHandlerConstantValue(p, &fr, this));
  addAttrHandler(new (p) AttributeHandlerCode(p, &fr, this));
  addAttrHandler(new (p) AttributeHandlerExceptions(p, &fr, this));
  addAttrHandler(new (p) AttributeHandlerLineNumberTable(p, &fr, this));
  addAttrHandler(new (p) AttributeHandlerLocalVariableTable(p, &fr, this));

  /* This must always be added after everything else, since it handles
   * all attributes that we know nothing about
   */
  addAttrHandler(new (p) AttributeHandlerDummy(p, &fr, this));
		 
  /* Access Flags */
  if (!fr.readU2(&accessFlags, 1)) 
    verifyError(VerifyError::noClassDefFound);

  /* This Class, super Class */
  if (!fr.readU2(&thisClassIndex, 1)) 
    verifyError(VerifyError::noClassDefFound);

  if (!fr.readU2(&superClassIndex, 1)) 
    verifyError(VerifyError::noClassDefFound);

  if (invalidIndex(thisClassIndex)) 
    verifyError(VerifyError::badClassFormat);

  if (superClassIndex != 0 && invalidIndex(superClassIndex)) 
    verifyError(VerifyError::badClassFormat);

  /* If we don't have a super-class, we must be java/lang/Object */
  if (superClassIndex == 0) {
    if (PL_strcmp(ConstantClass::cast(*const_cast<ConstantPoolItem *>(constantPool->get(thisClassIndex))).getUtf()->getUtfString(), "java/lang/Object") != 0) {
      UT_LOG(ClassReader, PR_LOG_DEBUG, ("No superclass, but we don't seem to be Object\n"));
      verifyError(VerifyError::noClassDefFound);
    }
  } else {    
    if (constantPool->get(superClassIndex)->getType() != CR_CONSTANT_CLASS) 
      verifyError(VerifyError::badClassFormat);
  }

  /* Interfaces */
  Uint16 interfaceCount;
  if (!fr.readU2(&interfaceCount, 1)) 
    verifyError(VerifyError::noClassDefFound);
  
  if (interfaceCount > 0) {
    interfaceInfo = new (p) ConstantPool(interfaceCount);

    readInterfaces();
  }

  /* Fields */
  if (!fr.readU2(&fieldCount, 1))
    verifyError(VerifyError::noClassDefFound);
  
  if (fieldCount > 0) {
    fieldInfo = new (p) FieldInfo *[fieldCount];
    publicFields = new (p) Uint16[fieldCount];

    readFields();
  }

  /* Methods */
  if (!fr.readU2(&methodCount, 1))
    verifyError(VerifyError::noClassDefFound);

  if (methodCount > 0) {
    methodInfo = new (p) MethodInfo *[methodCount];
    publicMethods = new (p) Uint16[methodCount];
    declaredMethods = new (p) Uint16[methodCount];
    constructors = new (p) Uint16[methodCount];
    publicConstructors = new (p) Uint16[methodCount];

    readMethods();
  }

  /* Attributes */
  if (!fr.readU2(&attributeCount, 1)) 
    verifyError(VerifyError::noClassDefFound);
  
  if (attributeCount > 0) {
    attributeInfo = new (p) AttributeInfoItem *[attributeCount];

    readAttributes(attributeInfo, attributeCount);
  }

  /* At this point, we must be at the end of the file....*/
#ifndef DEBUG_SMSILVER
  char dummy;
  if (fr.readU1(&dummy, 1))
    verifyError(VerifyError::noClassDefFound);
#endif
}

/* Install an attribute handler, growing the attrHandlers array if
 * needed.  The attribute handlers will be called in the order in which
 * they are installed until one of them succeeds.
 */
void ClassFileReader::addAttrHandler(AttributeHandler *handler)
{
  if (numAttrHandlers+1 > attrSize) {
    AttributeHandler **oldAttrHandlers = attrHandlers;
    attrSize += 5;
    attrHandlers = new (p) AttributeHandler *[attrSize];
    
    for (Int32 i = 0; i < numAttrHandlers; i++)
      attrHandlers[i] = oldAttrHandlers[i];
  }

  attrHandlers[numAttrHandlers++] = handler;
}

/* look up a fieldref, methodref or interfacemethodref with the
 * given name and signature, and return the index in the field/method
 * array of the class that the field/method/interface method occupies.
 * returns true on success, false on failure.
 */
bool ClassFileReader::lookupConstant(const char *name, const char *sig,
				     Uint8 type, 
				     Uint16 &index) const
{
  /* Get interned versions of the name and type string */
  const char *namei = sp.get(name);
  const char *sigi = sp.get(sig);

  for (Uint32 i = 1; i < constantPool->count(); i++) {
    const ConstantPoolItem *item = constantPool->get(i);
    if (item && item->getType() == type) {
      switch (type) {
      case CR_CONSTANT_METHODREF:
      case CR_CONSTANT_INTERFACEMETHODREF:
      case CR_CONSTANT_FIELDREF: {
	ConstantNameAndType *ninfo = ((ConstantRef *) item)->getTypeInfo();
	ConstantUtf8 *nameUtf = ninfo->getName();
	ConstantUtf8 *typeUtf = ninfo->getDesc();

	if (nameUtf->getUtfString() == namei &&
	    typeUtf->getUtfString() == sigi) {
	  index = (Uint16) i;
	  return true;
	}
	
	break;
      }
      
      default:
	/* If we're here, we don't yet support "type" */
	return false;
      }
    }
  }

  return false;
}

/* Look up a field or method based on given name and type descriptor. 
 * On success, returns the InfoItem for the field or method; on failure, 
 * returns NULL. type can be either CR_CONSTANT_FIELDREF (look for a field)
 * or CR_CONSTANT_METHODREF (look for a method). Assumption: both name
 * and sig are interned strings; however, sig can be nil if the method is
 * not overloaded. On success, index is set to the index of the matched
 * field or method in the field or method table of the class.
 */
const InfoItem *ClassFileReader::lookupFieldOrMethod(Uint8 type, 
						     const char *name, 
						     const char *sig,
						     Uint16 &index) const
{
  FastHashTable<InfoNode *> *table;
  const InfoItem **items;

  if (type == CR_CONSTANT_FIELDREF) {
    items = (const InfoItem **) fieldInfo;
    table = const_cast<FastHashTable<InfoNode *> *>(&fieldTable);
  } else {
    items = (const InfoItem **) methodInfo;
    table = const_cast<FastHashTable<InfoNode *> *>(&methodTable);
  }

  Vector<InfoNode *> vector(5);

  Int32 numMatches = table->getAll(name, vector);

  if (!numMatches)
    return 0;

  if (!sig) {
    if (numMatches == 1)
      return items[(index = (Uint16) vector[0]->index)];
    else
      return 0;
  }

  for (Int32 i = 0; i < numMatches; i++) {
    InfoNode *node = vector[i];
    if (node->sig == sig)
      return items[(index = (Uint16) node->index)];
  }

  return 0;
}






