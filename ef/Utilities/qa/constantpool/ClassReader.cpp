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
#include <stdio.h>

#ifndef NO_NSPR
#include "plstr.h"
#endif

#ifdef DEBUG
#include "DebugUtils.h"
#include "JavaBytecodes.h"
#include <stdarg.h>
void indent(int n) 
{
  for (; n; n--)
    printf("  ");
}
  
void title(int nIndents, const char *s)
{
  indent(nIndents);
  printf("%s\n", s);
}

void print(int nIndents, const char *fmt, ...)
{
  va_list args;
  static char buffer[2048];

  indent(nIndents);
  va_start(args, fmt);
  vsprintf(buffer, fmt, args);
  va_end(args);

  printf("%s\n", buffer);
}

#endif


/* Functions to parse method and field descriptors */
static bool parseFieldDescriptor(Pool &pool, const char *s, JavaType &type, 
				 const char **next)
{
  TypeKind tk;
  
  switch (*s) {
  case 'B':
    tk = tkByte;
    break;

  case 'C':
    tk = tkChar;
    break;

  case 'D':
    tk = tkDouble;
    break;
    
  case 'F':
    tk = tkFloat;
    break;

  case 'I':
    tk = tkInt;
    break;

  case 'J':
    tk = tkLong;
    break;
    
  case 'S':
    tk = tkShort;
    break;

  case 'Z':
    tk = tkBoolean;
    break;

  case '[': {
    JavaType *javaType = new (pool) JavaType;

    if (!javaType)
      return false;

    if (!parseFieldDescriptor(pool, s+1, *javaType, next))
      return false;

    type.set(*javaType);
    return true;
  }

  case 'L': {
#ifdef NO_NSPR
    char *t = strchr(++s, ';');
#else
    char *t = PL_strchr(++s, ';');
#endif

    if (!t)
      return false;
      
    int len = t-s+1;
    char *instanceClass = new (pool) char [len];

    if (!instanceClass)
      return false;

#ifdef NO_NSPR
    strncpy(instanceClass, s, len-1);
#else
    PL_strncpy(instanceClass, s, len-1);
#endif

    instanceClass[len-1] = 0;
    *next = (char *) t+1;
    type.set(instanceClass);
    return true;
  }

  default:
    return false;
  }
	
  type.set(tk);
  *next = s+1;
  return true;
}


inline int getJavaSize(const JavaType *type) {
  return getTypeKindSize(type->getKind());
}


/* Implementation of ConstantUtf8 */
ConstantUtf8::ConstantUtf8(Pool &pool, char *str, int len) : 
  ConstantPoolItem(pool, CR_CONSTANT_UTF8) 
{
  data = str;
  dataLen = len;
}

#ifdef DEBUG
void ConstantUtf8::dump(int nIndents)
{    
  title(nIndents, "CONSTANT_UTF8");
  print(nIndents, "String: %s", data);
}
#endif

ConstantUtf8::ConstantUtf8(Pool &pool, int len) : 
  ConstantPoolItem(pool, CR_CONSTANT_UTF8)
{  
  dataLen = len;
}

/* Implementation of ConstantClass */
int ConstantClass::resolveAndValidate()
{
  if (pool)
    utfString = (ConstantUtf8 *) pool[index];
  
  return (utfString && utfString->getType() == CR_CONSTANT_UTF8);
}

#ifdef DEBUG
void ConstantClass::dump(int nIndents)
{
  title(nIndents, "CONSTANT_CLASS");
  utfString->dump(nIndents+1);
}
#endif

/* Implementation of ConstantNameAndType */
int ConstantNameAndType::resolveAndValidate()
{
  if (pool) {
    nameInfo = (ConstantUtf8 *) pool[nameIndex];
    descInfo = (ConstantUtf8 *) pool[descIndex];
  }

  return (nameInfo && descInfo &&
	  nameInfo->getType() == CR_CONSTANT_UTF8 &&
	  descInfo->getType() == CR_CONSTANT_UTF8);
}

#ifdef DEBUG
void ConstantNameAndType::dump(int nIndents)
{
  title(nIndents, "CONSTANT_NAMEANDTYPE (name/desc)");
  nameInfo->dump(nIndents+1);
  descInfo->dump(nIndents+1);
}
#endif

ConstantNameAndType::ConstantNameAndType(Pool &pool,
					 ConstantPoolItem **array, 
					 int nindex, int dIndex) : 
  ConstantPoolItem(pool, array, CR_CONSTANT_NAMEANDTYPE) 
{
  nameIndex = nindex, descIndex = dIndex;
  nameInfo = descInfo = 0;
}

/* Implementation of ConstantRef */
int ConstantRef::resolveAndValidate()
{
  if (pool) {
    classInfo = (ConstantClass *) pool[classIndex];
    typeInfo = (ConstantNameAndType *) pool[typeIndex];
  }
  
  return (classInfo && typeInfo &&
	  classInfo->getType() == CR_CONSTANT_CLASS &&
	  typeInfo->getType() == CR_CONSTANT_NAMEANDTYPE);
}

#ifdef DEBUG
void ConstantRef::dump(int nIndents)
{
  print(nIndents+2, "Printing ClassInfo and typeInfo");
  classInfo->dump(nIndents+1);
  typeInfo->dump(nIndents+1);
}
#endif

ConstantRef::ConstantRef(Pool &pool,
			 uint8 _type, ConstantPoolItem **array, int cIndex, 
			 int tIndex) : ConstantPoolItem(pool, array, _type)
{
  classIndex = cIndex, typeIndex = tIndex;
  classInfo = 0;
  typeInfo = 0;
}

/* Implementation of ConstantFieldRef */
#ifdef DEBUG
void ConstantFieldRef::dump(int nIndents)
{
  title(nIndents, "CONSTANT_FIELD_REF");
  ConstantRef::dump(nIndents);
}
#endif

/* Implementation of ConstantMethodRef */
#ifdef DEBUG
void ConstantMethodRef::dump(int nIndents) {
  title(nIndents, "CONSTANT_METHOD_REF");
  ConstantRef::dump(nIndents);
}
#endif

/* Implementation of class ConstantInterfaceMethodRef */
#ifdef DEBUG
  void ConstantInterfaceMethodRef::dump(int nIndents) {
    title(nIndents, "CONSTANT_INTERFACEMETHOD_REF");
    ConstantRef::dump(nIndents);
  }
#endif

/* Implementation of ConstantString */
int ConstantString::resolveAndValidate()
{
  if (pool) 
    utfString = (ConstantUtf8 *) pool[index];
   
  return (utfString && utfString->getType() == CR_CONSTANT_UTF8);
}

#ifdef DEBUG
void ConstantString::dump(int nIndents) {
  title(nIndents, "CONSTANT_STRING");
  utfString->dump(nIndents+1);
}
#endif

/* Implementation of ConstantVal */
#ifdef DEBUG
void ConstantVal::dump(int nIndents)
{
  switch (vk) {
  case vkInt:
    title(nIndents, "CONSTANT_INT");
    print(nIndents, "Value: %d", value.i);
    break;

  case vkFloat:
    title(nIndents, "CONSTANT_FLOAT");
    print(nIndents, "Value: %f", value.f);
    break;

  case vkDouble:
    title(nIndents, "CONSTANT_DOUBLE");
    print(nIndents, "Value: %lf", value.d);
    break;

  case vkLong:
    title(nIndents, "CONSTANT_LONG");
    break;
      
  default:
    break;
  }
}
#endif

/* Implementation of ConstantFloat */
ConstantFloat::ConstantFloat(Pool &pool, uint32 _raw): 
  ConstantVal(pool, CR_CONSTANT_FLOAT, vkFloat), raw(_raw)
{
  if (raw == 0x7f80000)
    value.setValueContents(floatPositiveInfinity);
  else if (raw == 0xff800000)
    value.setValueContents(floatNegativeInfinity);
#if 0
  else if ((raw >= 0x7f800001 && raw <= 0x7fffffff) ||
	   (raw >= 0xff800001 && raw <= 0xffffffff))
#else
  else if ((raw & 0x7f800001) || (raw & 0xff800001))
#endif
    value.setValueContents(floatNaN);
  else {
    
    int s = ((raw >> 31) == 0) ? 1 : -1;
    int e = ((raw >> 23) &0xff);
    int m = (e == 0) ? (raw & 0x7fffff) << 1: 
    (raw & 0x7fffff) | 0x800000;
    
    value.setValueContents((Flt32) s * m * pow(2.0, e-150));
  }
}

/* Implementation of ConstantLong */
ConstantLong::ConstantLong(Pool &pool, uint32 _lo, uint32 _hi) : 
  ConstantVal(pool, CR_CONSTANT_LONG, vkLong), lo(_lo), hi(_hi)
{
#ifdef HAVE_LONG_LONG
  int64 lw, llo, lhi;

  llo = (int64) lo;
  lhi = (int64) hi;
  lhi <<= 32;
  
  lw = llo | lhi;
  
  value.setValueContents(lw);
#else
  value.l.lo = lo;
  value.l.hi = hi;
#endif  
}


/* Implementation of ConstantDouble */
ConstantDouble::ConstantDouble(Pool &pool, char *_data): 
  ConstantVal(pool, CR_CONSTANT_DOUBLE, vkDouble)
{
  union {
    unsigned char c[8];
    Flt64 d;
  } u;
  
  memcpy(data, _data, 8);
#ifdef LITTLE_ENDIAN
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
#ifdef DEBUG
void AttributeSourceFile::dump(int nIndents) {    
  title(nIndents, "SOURCE-FILE");    
  srcName->dump(nIndents+1);
}
#endif

/* Implementation of AttributeConstantValue */
#ifdef DEBUG
void AttributeConstantValue::dump(int nIndents)
{
  title(nIndents, "CONSTANT-VALUE");    
  value->dump(nIndents+1);
}
#endif

/* Implementation of ExceptionItem */
ExceptionItem::ExceptionItem(uint16 _startPc, uint16 _endPc, 
			     uint16 _handlerPc, ConstantClass *_catcher,
			     uint16 _catcherIndex) {
  startPc = _startPc, endPc = _endPc,
    handlerPc = _handlerPc, catcher = _catcher;
  catcherIndex = _catcherIndex;
}

#ifdef DEBUG
void ExceptionItem::dump(int nIndents) {    
  title(nIndents, "ExceptionItem");
  print(nIndents+1, 
	"Start %u End %u Handler %u", startPc, endPc, handlerPc);
  if (catcher) {
    print(nIndents+1, "CATCHER:");    
    catcher->dump(nIndents+2);
  }
}
#endif

/* Implementation of AttributeCode */
AttributeCode::AttributeCode(Pool &pool, uint16 _length, 
			     uint16 _maxStack, uint16 _maxLocals, 
			     uint32 _codeLength,
			     uint16 nindex,
			     CrError *status): 
  AttributeInfoItem(pool, "Code", CR_ATTRIBUTE_CODE, _length, nindex)
{
  *status = crErrorNone;
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

int AttributeCode::setNumExceptions(int _numExceptions)
{
  exceptions = new (p) ExceptionItem *[_numExceptions];
  numExceptions = _numExceptions;
  return true;
}

int AttributeCode::setNumAttributes(int _numAttributes)
{
  attributes = new (p) AttributeInfoItem *[_numAttributes];
  numAttributes = _numAttributes;
  return true;
}


#ifdef DEBUG
void AttributeCode::dump(int nIndents) {
  int32 i;
  
  title(nIndents, "CODE");
  print(nIndents, 
	"maxStack %u maxLocals %u codeLength %u numExceptions %u",
	maxStack, maxLocals, codeLength, numExceptions);

  if (numExceptions > 0) {
    title(nIndents, "Exceptions");
    for (i = 0; i < (int32) numExceptions; i++)
      exceptions[i]->dump(nIndents+1);
  }

  if (numAttributes > 0) {
    title(nIndents, "Attributes");
    for (i = 0; i < numAttributes; i++)
      attributes[i]->dump(nIndents+1);
  }
}
#endif

/* Implementation of AttributeExceptions */
int AttributeExceptions::setNumExceptions(uint16 n)
{
  exceptions = new (p) ConstantClass *[n];
  excIndices = new (p) uint16[n];

  numExceptions = n;
  return true;
}

#ifdef DEBUG
void AttributeExceptions::dump(int nIndents)
{
  int i;
  
  title(nIndents, "EXCEPTIONS");
  print(nIndents, "numExceptions %d", numExceptions);
  
  if (numExceptions > 0) {
    title(nIndents, "Exceptions");
    for (i = 0; i < numExceptions; i++)
      exceptions[i]->dump(nIndents+1);
  }
}
#endif

/* Implementation of AttributeLineNumberTable */
int AttributeLineNumberTable::setNumEntries(uint16 n)
{
  entries = new (p) LineNumberEntry[n];
  numEntries = n;
  return true;
}

int AttributeLineNumberTable::getPc(uint16 lineNumber) {
  int i;
  
  /* The line numbers are not guaranteed to be in order */
  for (i = 0; i < numEntries; i++)
    if (entries[i].lineNumber == lineNumber)
      return entries[i].startPc;
  
  return -1;
}
  
#ifdef DEBUG
void AttributeLineNumberTable::dump(int nIndents)
{
  title(nIndents, "LINE-NUMBER-ENTRY");
  
  for (int i = 0; i < numEntries; i++)
    entries[i].dump(nIndents+1);
}
#endif


/* Implementation of AttributeLocalVariableTable */
int AttributeLocalVariableTable::setNumEntries(uint16 n) {
  entries = new (p) LocalVariableEntry[n];
  numEntries = n;
  return true;    
}

#ifdef DEBUG
void AttributeLocalVariableTable::dump(int nIndents)
{
  title(nIndents, "LOCAL-VARIABLE-TABLE");
  
  for (int i = 0; i < numEntries; i++)
    entries[i].dump(nIndents+1);
}
#endif

/* Implementation of InfoItem */
InfoItem::InfoItem(Pool &pool, uint16 aflags, ConstantUtf8 *classInfo,
		   ConstantUtf8 *nameInfo, 
		   ConstantUtf8 *descInfo,
		   uint16 nIndex, uint16 dIndex) : p(pool), 
  nameIndex(nIndex), descIndex(dIndex)
{
  accessFlags = aflags;
  name = nameInfo;
  descriptor = descInfo;
  className = classInfo;
  attrCount = 0;    
}

int InfoItem::addAttribute(AttributeInfoItem &attribute)
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

#ifdef NO_NSPR
    if (!(strncmp(_name, attribute.getName(), attribute.getLength())))
#else
    if (!(PL_strncmp(_name, attribute.getName(), attribute.getLength())))
#endif
      return &attribute;
  }
    
  return 0;
}

AttributeInfoItem *InfoItem::getAttribute(const uint32 code) const
{
  for (DoublyLinkedList<AttributeInfoItem>::iterator i = attributes.begin();
       !attributes.done(i); i = attributes.advance(i)) {
    AttributeInfoItem &attribute = attributes.get(i);

    if (attribute.getCode() == code)
      return &attribute;
  }
  
  return 0;
}

  
#ifdef DEBUG
void InfoItem::dump(int nIndents)
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
FieldInfo::FieldInfo(Pool &pool, uint16 aflags, ConstantUtf8 *classInfo, 
		     ConstantUtf8 *nameInfo, 
		     ConstantUtf8 *descInfo,
		     uint16 nameindex, uint16 descindex) : 
  InfoItem(pool, aflags, classInfo, nameInfo, descInfo, nameindex, 
	   descindex) {
  type = new (pool) JavaType;
}

void FieldInfo::getInfo(JavaType *&typeRet, 
			bool &isVolatile, bool &isConstant,
			uint32 &offsetRet) const
{
  typeRet = type;
  offsetRet = pos.offset;
  isVolatile = (getAccessFlags() & CR_FIELD_VOLATILE);
  isConstant = false;  
}

void FieldInfo::getInfo(JavaType *&typeRet, 
			bool &isVolatile, bool &isConstant,
			addr &addrRet) const
{
  typeRet = type;
  addrRet = pos.a;
  isVolatile = (getAccessFlags() & CR_FIELD_VOLATILE);
  isConstant = false; 
}


#ifdef DEBUG
void FieldInfo::dump(int nIndents)
{
  InfoItem::dump(nIndents);

  if (getAccessFlags() & CR_FIELD_STATIC)
    print(nIndents, "Address: %x", pos.a);
  else
    print(nIndents, "Offset: %d", pos.offset);
  
}

#endif  

/* Implementation of MethodInfo */
MethodInfo::MethodInfo(Pool &pool, uint16 aflags, ConstantUtf8 *classInfo, 
		       ConstantUtf8 *nameInfo, 
		       ConstantUtf8 *descInfo,
		       uint16 nIndex, uint16 dIndex) : 
InfoItem(pool, aflags, classInfo, nameInfo, descInfo, nIndex, dIndex)
{
  paramSize = 10;
  
  params = new (pool) JavaType[paramSize];
}


const JavaType &MethodInfo::getSignature(int &nParams, 
					 const JavaType *&paramTypes,
					 bool &isStatic, bool &isNative, 
					 bool &isAbstract) const
{
  nParams = numParams;
  paramTypes = params;

  uint16 aFlags = getAccessFlags();
  isStatic = (aFlags & CR_METHOD_STATIC) != 0;
  isNative = (aFlags & CR_METHOD_NATIVE) != 0;
  isAbstract = (aFlags & CR_METHOD_ABSTRACT) != 0;
  return ret;
}
  
#if DEBUG_LAURENT
static inline JavaType *newJavaType(Pool& p, uint32 size) {return new(p) JavaType[size];}
#endif

bool MethodInfo::parseMethodDescriptor(const char *s)
{
  char *paramstr;
  const char *t;

  numParams = 0;

  if (!(getAccessFlags() & CR_METHOD_STATIC)) {
   /* We need a copy since ~JavaType() deletes the string 
    * passed to it
    */
    const char *t = getClassName()->getUtfString();

#ifdef NO_NSPR
    int len = strlen(t);
#else
    int len = PL_strlen(t);
#endif

    char *s = new (p) char [len+1];

#ifdef NO_NSPR
    strncpy(s, t, len);
#else
    PL_strncpy(s, t, len);
#endif

    s[len] = 0;

    params[0].set(s);
    numParams = 1;
  }

  if (*s++ != '(')
    return false;

  /* Get everything between this and ')' */
#ifdef NO_NSPR
  if (!(t = strchr(s, ')')))
#else
  if (!(t = PL_strchr(s, ')')))
#endif
    return false;

  int len = t-s+1;

  if (len != 1) {
    /* This is a very temporary allocation, so we don't use the pool
     * to allocate this. Also see the delete paramstr, below
     */
    if (!(paramstr = new char[len]))
      return false;
    
#ifdef NO_NSPR
    strncpy((char *) paramstr, s, len-1);
#else
    PL_strncpy((char *) paramstr, s, len-1);
#endif

    paramstr[len-1] = 0;

    const char *next = paramstr;

	while (*next) {

      if (numParams+1 > paramSize) {
	JavaType *oldParams = params;
		
#if DEBUG_LAURENT
	// This will avoid an internal compile error (gcc)
	params = newJavaType(p, paramSize);
#else
	params = new (p) JavaType[paramSize];
#endif

	for (int i = 0; i < numParams; i++)
	  params[i].move(oldParams[i]);
	
	/* We don't do this since we're using a pool to allocate, but 
	 * this is a potential source of runaway memory usage
	 */
	/*	delete [] oldParams;*/
      }
	
      if (!parseFieldDescriptor(p, next, params[numParams], &next))
	return false;
      
      numParams++;      
    }
    
    delete [] paramstr;
  }

  s += len;

  /* Parse the return descriptor */
  if (*s == 'V') {
    ret.set(tkVoid);
    return true;
  } else {
    const char *next;
    return parseFieldDescriptor(p, s, ret, &next) && !*next;
  }
}



/* Implementation of ClassFileReader */
bool ClassFileReader::lookupConstant(uint16 index, ValueKind &vk, 
				     Value &value) const 
{
  if (index < 1 || index >= constantPoolCount)
    return false;

  /* Are we dealing with a constant that returns a value? */    
  int type = constantPool[index]->getType();

  /* index must point to a long, float, double, or integer */
  if (!((type > 2 && type < 7)))
    return false;
    
  ConstantVal *val = (ConstantVal *) constantPool[index];
  vk = val->getValueKind();
  value = val->getValue();
    
  return true;
}

/* Index is an index into the field array */
bool ClassFileReader::lookupStaticField(uint16 index, JavaType *&type, 
					addr &a,
					bool &isVolatile, bool &isConstant) 
{    
  FieldInfo *fInfo;
  
  if (index >= fieldCount)
    return false;
  
  isConstant = false;
  
  fInfo = fieldInfo[index];
  uint16 aFlags = fInfo->getAccessFlags();
  
  if (!(aFlags & CR_FIELD_STATIC))
    return false;
  
  isVolatile = (aFlags & CR_FIELD_VOLATILE) != 0;
  fInfo->getType(type);
  a = fInfo->pos.a;

  return true;
}

/* Index is an index into the field array */
bool ClassFileReader::lookupInstanceField(uint16 index, JavaType *&type, 
					  uint32 &offset,
					  bool &isVolatile, 
					  bool &isConstant)
{
  FieldInfo *fInfo;

  if (index >= fieldCount)
    return false;

  isConstant = false;
  
  fInfo = fieldInfo[index];
    uint16 aFlags = fInfo->getAccessFlags();
  
  if (aFlags & CR_FIELD_STATIC)
    return false;
  
  isVolatile = (aFlags & CR_FIELD_VOLATILE) != 0;
  fInfo->getType(type);
  offset = fInfo->pos.offset;

  return true;
}
  
/* Index is an index into the method array */
bool ClassFileReader::lookupMethod(uint16 index, 
				   const JavaType *&ret, int &nParams, 
				   const JavaType *&paramTypes,
				   bool &isStatic, 
				   bool &isNative,
				   bool &isAbstract)
{
  if (index >= methodCount)
    return false;
  
  ret = &methodInfo[index]->getSignature(nParams, paramTypes, isStatic, 
					 isNative, isAbstract);
  return true;
}

#ifdef DEBUG
void ClassFileReader::dump()
{
  int i;

  print(0, "AccessFlags %x", accessFlags);
  print(0, "Major Version %d, Minor Version %d", majorVersion, 
	minorVersion);

  print(0, "\nThis Class:");
  constantPool[thisClassIndex]->dump(0);
  
  if (superClassIndex > 0) {
    print(0, "\nSuper Class:");
    constantPool[superClassIndex]->dump(0);
  }
  
  print(0, "\nConstant Pool Count: %d", constantPoolCount);
  for (i = 1; i < constantPoolCount; i++) {  
    print(0, "%d.", i);
    
    if (constantPool[i])
      constantPool[i]->dump(0);
    else
      print(0, "NULL Constant-pool entry, probably result of a previous\n"
	    "DOUBLE or LONG");    
  }
  
  print(0, "\nField Count: %d", fieldCount);
  for (i = 0; i < fieldCount; i++) {
    print(0, "%d.", i+1);
    fieldInfo[i]->dump(0);
  }
  
  print(0, "\nInterface Count: %d", interfaceCount);
  for (i = 0; i < interfaceCount; i++) {
    print(0, "%d.", i+1);
    interfaceInfo[i].item->dump(0);
  }
  
  print(0, "\nMethod Count: %d", methodCount);
  for (i = 0; i < methodCount; i++) {
    print(0, "%d.", i+1);
    methodInfo[i]->dump(0);
  }

  print(0, "\nAttribute Count: %d", attributeCount);
  for (i = 0; i < attributeCount; i++) {
    print(0, "%d.", i+1);
    attributeInfo[i]->dump(0);
  }
}

void ClassFileReader::dumpClass(char *dumpfile)
{
  /* Create file. */
  FILE *fp;
  fp = fopen(dumpfile,"w");

  /* Begin file */
  fprintf(fp,"#begin_class_file\n");
  fprintf(fp,"\n");

  /* Magic */
  uint32 magic = 0xCAFEBABE;
  fprintf(fp,"#magic\n");
  fprintf(fp,"0x%x\n",magic);

  fprintf(fp,"\n");

  /* Minor version */
  fprintf(fp,"#minor_version\n");
  fprintf(fp,"%i\n",minorVersion);

  fprintf(fp,"\n");

  /* Major version */
  fprintf(fp,"#major_version\n");
  fprintf(fp,"%i\n",majorVersion);

  fprintf(fp,"\n");

  /* Output to stdout. */
  if (true) {

    /* Magic */
    printf("#magic\n");
    printf("0x%x\n",magic);

    printf("\n");

    /* Minor version */
    printf("#minor_version\n");
    printf("%i\n",minorVersion);

    printf("\n");

    /* Major version */
    printf("#major_version\n");
    printf("%i\n",majorVersion);

    printf("\n");

  }

  /* Constant Pool */
  dumpConstantPool(fp);

  fprintf(fp,"\n");

  /* Access flags */
  fprintf(fp,"#access_flags\n");
  fprintf(fp,"0x%x\n",accessFlags);

  fprintf(fp,"\n");

  /* This class */
  fprintf(fp,"#this_class\n");
  fprintf(fp,"%i\n",thisClassIndex);

  fprintf(fp,"\n");
 
  /* Super class */
  fprintf(fp,"#super_class\n");
  fprintf(fp,"%i\n",superClassIndex);

  fprintf(fp,"\n");

  /* Output to stdout. */
  if (true) {

    printf("\n");
    
    /* Access flags */
    printf("#access_flags\n");
    printf("0x%x\n",(char)accessFlags);

    printf("\n");
    
    /* This class */
    printf("#this_class\n");
    printf("%i\n",thisClassIndex);

    printf("\n");

    /* Super class */
    printf("#super_class\n");
    printf("%i\n",superClassIndex);

    printf("\n");

  }

  /* Interfaces */
  dumpInterfaces(fp);

  fprintf(fp,"\n");
  
  /* Output to stdout. */
  if (true) printf("\n");

  /* Fields */
  dumpFields(fp);

  fprintf(fp,"\n");

  /* Output to stdout. */
  if (true) printf("\n");
  
  /* Methods */
  dumpMethods(fp);

  fprintf(fp,"\n");

  /* Output to stdout. */
  if (true) printf("\n");
  
  /* Attributes */
  dumpAttributes(fp);

  /* End class file */
  fprintf(fp,"\n");
  fprintf(fp,"#end_class_file");

  fclose(fp);

}

void ClassFileReader::dumpConstantPool(FILE *fp)
{

  fprintf(fp,"#constant_pool_count\n"); 
  fprintf(fp,"%i\n",constantPoolCount);
  fprintf(fp,"#begin_constant_pool\n");
  
  /* Entry 0 is reserved. */
  fprintf(fp,"\t#0 Reserved\n");

  /* Output to stdout. */
  if (true) {
    printf("#constant_pool_count\n");
    printf("%i\n",constantPoolCount);
    printf("#begin_constant_pool\n");

    /* Entry 0 is reserved. */
    printf("\t#0 Reserved\n");
  }

  for (int i = 1; i < constantPoolCount; i++) {  

    if (constantPool[i]) {

      switch(constantPool[i]->getType()) {
      
      case CR_CONSTANT_UTF8: {

	ConstantUtf8 *tempUtf8 = (ConstantUtf8 *)constantPool[i];

	fprintf(fp, "\t#%i Utf8\n",i);
	fprintf(fp, "\t%s\n",tempUtf8->data);

	/* Output to stdout. */
	if (true) {
	  printf("\t#%i Utf8\n",i);
	  printf("\t%s\n",tempUtf8->data);
	}

	break;

      }

      case CR_CONSTANT_INTEGER: {

	ConstantInt *tempInt = (ConstantInt *)constantPool[i];

	fprintf(fp, "\t#%i Integer\n", i);
	fprintf(fp, "\tvalue %i\n",tempInt->getValue());

 	/* Output to stdout. */
	if (true) {
	  printf("\t#%i Integer\n",i);
	  printf("\t#value %i\n",tempInt->getValue());
	}

	break;

      }

      case CR_CONSTANT_FLOAT: {

	int s,e,m;
	uint32 raw;

	ConstantFloat *tempFloat = (ConstantFloat *)constantPool[i];
	raw = (uint32)tempFloat->getRaw();
	
	/*Use raw to compute for s, e, and m.*/
	/*This doesn't check for +/- Infinity and Nan yet.*/
	s = ((raw >> 31) == 0) ? 1 : -1;
	e = ((raw >> 23) &0xff);
	m = (e == 0) ? (raw & 0x7fffff) << 1 : 
	  (raw & 0x7fffff) | 0x800000;

	fprintf(fp, "\t#%i Float\n",i);
	fprintf(fp, "\tvalue sign %i exponent %i mantissa %i\n",s,e,m);

 	/* Output to stdout. */
	if (true) {
	  printf("\t#%i Float\n",i);
	  printf("\tvalue sign %i exponent %i mantissa %i\n",s,e,m);
	}

	break;

      }

      case CR_CONSTANT_LONG: {

	uint32 hi,lo;

	ConstantLong *tempLong = (ConstantLong *)constantPool[i];
	tempLong->getBits(lo,hi);

	fprintf(fp, "\t#%i Long\n",i);
	fprintf(fp, "\tvalue hi %i lo %i\n",hi,lo);

 	/* Output to stdout. */
	if (true) {
	  printf("\t#%i Long\n",i);
	  printf("\tvalue hi %i lo %i\n",hi,lo);
	}

	//Increase count.  This is specified in the spec.
	i++;

	break;
      }

      case CR_CONSTANT_DOUBLE: {

	char* data;

	ConstantDouble *tempDouble = (ConstantDouble *)constantPool[i];
	data = tempDouble->getData();

	fprintf(fp, "\t#%i Double\n",i);
	fprintf(fp, "\tvalue ");
	
	/* The following will need to be fixed once we know how to handle Doubles. */
	int j;
	for (j = 0; j < 8; j++) {
	 fprintf(fp,"%x ",data[j]);
	}
	fprintf(fp, "\n");

 	/* Output to stdout. */
	if (true) {
	  printf("\t#%i Double\n",i);
	  printf("\tvalue ");
	  for(j = 0; j < 8 ; j++) printf("%x ",data[j]);
	  printf("\n");
	}

	//Increase count. This is specified in the spec.
	i++;

	break;

      }
    
      case CR_CONSTANT_CLASS: {

	ConstantClass *tempClass = (ConstantClass *)constantPool[i];

	fprintf(fp, "\t#%i Class\n",i);
	fprintf(fp, "\tindex %i\n",tempClass->index);

 	/* Output to stdout. */
	if (true) {
	  printf("\t#%i Class\n",i);
	  printf("\tindex %i\n",tempClass->index);
	}

	break;

    }

      case CR_CONSTANT_STRING: {

	ConstantString *tempString = (ConstantString *)constantPool[i];

	fprintf(fp, "\t#%i String\n",i);
	fprintf(fp, "\tindex %i\n",tempString->index);

 	/* Output to stdout. */
	if (true) {
	  printf("\t#%i String\n",i);
	  printf("\tindex %i\n",tempString->index);
	}

	break;
      }

      case CR_CONSTANT_FIELDREF: {

	ConstantFieldRef *tempFieldRef = (ConstantFieldRef *)constantPool[i];
	ConstantRef *tempRef = (ConstantRef *)tempFieldRef;

	fprintf(fp, "\t#%i FieldRef\n",i);
	fprintf(fp, "\tclassIndex %i typeIndex %i\n",tempRef->classIndex,tempRef->typeIndex);

 	/* Output to stdout. */
	if (true) {
	  printf("\t#%i FieldRef\n",i);
	  printf("\tclassIndex %i typeIndex %i\n",tempRef->classIndex,tempRef->typeIndex);
	}

	break;

      }

      case CR_CONSTANT_METHODREF: {

	ConstantMethodRef *tempMethodRef = (ConstantMethodRef *)constantPool[i];
	ConstantRef *tempRef = (ConstantRef *)tempMethodRef;

	fprintf(fp, "\t#%i MethodRef\n",i);
	fprintf(fp, "\tclassIndex %i typeIndex %i\n",tempRef->classIndex,tempRef->typeIndex);

 	/* Output to stdout. */
	if (true) {
	  printf("\t#%i MethodRef\n",i);
	  printf("\tclassIndex %i typeIndex %i\n",tempRef->classIndex,tempRef->typeIndex);
	}

	break;

      }

      case CR_CONSTANT_INTERFACEMETHODREF: {


	ConstantInterfaceMethodRef *tempInterfaceMethodRef = (ConstantInterfaceMethodRef *)constantPool[i];
	ConstantRef *tempRef = (ConstantRef *)tempInterfaceMethodRef;

	fprintf(fp, "\t#%i InterfaceMethodRef\n",i);
	fprintf(fp, "\tclassIndex %i typeIndex %i\n",tempRef->classIndex,tempRef->typeIndex);

 	/* Output to stdout. */
	if (true) {
	  printf("\t#%i InterfaceMethodRef\n",i);
	  printf("\tclassIndex %i typeIndex %i\n",tempRef->classIndex,tempRef->typeIndex);
	}

	break;

      } 

      case CR_CONSTANT_NAMEANDTYPE: {


	ConstantNameAndType *tempNameAndType = (ConstantNameAndType *)constantPool[i];

	fprintf(fp, "\t#%i NameAndType\n",i);
	fprintf(fp, "\tnameIndex %i descIndex %i\n",tempNameAndType->nameIndex,tempNameAndType->descIndex);

 	/* Output to stdout. */
	if (true) {
	  printf("\t#%i NameAndType\n",i);
	  printf("\tnameIndex %i descIndex %i\n",tempNameAndType->nameIndex,tempNameAndType->descIndex);
	}

	break;

      }

      default: {

	printf("Error in constant pool\n");
	return;
      }

      }
    }
    else
      print(0, "NULL Constant-pool entry, probably result of a previous\n"
	    "DOUBLE or LONG\n");    
  }

  fprintf(fp,"#end_constant_pool\n");
  
  /* Output to stdout. */
  if (true) {
    printf("#end_constant_pool\n");
  }
  
}

void ClassFileReader::dumpInterfaces(FILE *fp)
{
  fprintf(fp,"#interfaces_count\n");
  fprintf(fp,"%i\n",interfaceCount);

  /* Output to stdout. */
  if (true) {
    printf("#interfaces_count\n");
    printf("%i\n",interfaceCount);
  }

  if (interfaceCount > 0) {

    fprintf(fp,"#begin_interfaces\n");

    /* Output to stdout. */
    if (true) printf("#begin_interfaces\n");

    for (uint16 i = 0; i < interfaceCount; i++ ) {
      
      fprintf(fp,"\t#%i\n",i); 
      fprintf(fp,"\tindex %i\n",interfaceInfo[i].index);

      /* Output to stdout. */
      if (true) {
	printf("\t#%i\n",i);
	printf("\tindex %i\n",interfaceInfo[i].index);
      }
    }

    fprintf(fp,"#end_interfaces\n");

    /* Output to stdout. */
    if (true) printf("#end_interfaces\n");
    
  }
}

void ClassFileReader::dumpFields(FILE *fp)
{
  fprintf(fp,"#fields_count\n");
  fprintf(fp,"%i\n",fieldCount);

  /* Output to stdout. */
  if (true) {
    printf("#fields_count\n");
    printf("%i\n",fieldCount);
  }

  if (fieldCount > 0) {
    
    fprintf(fp,"#begin_fields\n");

    /* Output to stdout. */
    if (true) printf("#begin_fields\n");

    for (uint16 i = 0; i < fieldCount; i++ ) {

      fprintf(fp,"\t#%i\n",i);

      /* Access flags */
      fprintf(fp,"\taccess_flags 0x%x\n",fieldInfo[i]->getAccessFlags());
      
      /* Name index */
      fprintf(fp,"\tnameIndex %i descIndex %i\n",fieldInfo[i]->getNameIndex(),fieldInfo[i]->getDescIndex());
      
      /* Attributes count */
      fprintf(fp,"\tattributes_count %i\n",fieldInfo[i]->getAttributeCount());

      /* Output to stdout. */
      if (true) {

	printf("\t#%i\n",i);
      
	/* Access flags */
	printf("\taccess_flags 0x%x\n",fieldInfo[i]->getAccessFlags());
	
	/* Name index */
	printf("\tnameIndex %i descIndex %i\n",fieldInfo[i]->getNameIndex(),fieldInfo[i]->getDescIndex());
	
	/* Attributes count */
	printf("\tattributes_count %i\n",fieldInfo[i]->getAttributeCount());

      }	
      
      /*
       * The following attribute_info loop needs to be tested.
       */
      if (fieldInfo[i]->getAttributeCount() > 0) {
	
	fprintf(fp,"\t#begin_attributes\n");

	/* Output to stdout. */
	if (true) printf("\t#being_attributes\n");

	int currentCount = 0;
	const DoublyLinkedList<AttributeInfoItem> &list = fieldInfo[i]->getAttributes();
	for (DoublyLinkedList<AttributeInfoItem>::iterator j = list.begin(); !list.done(j); j = list.advance(j)) {

	  AttributeInfoItem &item = list.get(j);
	  
	  fprintf(fp,"\t\t#%i %s\n",currentCount,item.getName());
	  
	  /* Attribute name index */
	  fprintf(fp,"\t\tnameIndex %i\n",item.getNameIndex());

	  /* Attribute length */
	  fprintf(fp,"\t\tlength %i\n",item.getLength());

	  /* Output to stdout. */
	  if (true) {

	    printf("\t\t#%i %s\n",currentCount,item.getName());
	    
	    /* Attribute name index */
	    printf("\t\tnameIndex %i\n",item.getNameIndex());
	    
	    /* Attribute length */
	    printf("\t\tlength %i\n",item.getLength());

	  }

	  /* Find the correct attribute and generate the corresponding info. */
	  switch (item.getCode()) {

	  case CR_ATTRIBUTE_CONSTANTVALUE: { /* ConstantValue Attribute */

	    AttributeConstantValue *constVal = static_cast<AttributeConstantValue *>(&item);
	    fprintf(fp,"\t\tindex: %i\n",constVal->getIndex());

	    /* Output to stdout. */
	    if (true) printf("\t\tindex: %i\n",constVal->getIndex());

	    break;

	  }

	  default: { /* Ignore everything else. */
	    break;
	  }

	  }
	
	  currentCount++;
  
	}

	fprintf(fp,"\t#end_attribute_info\n");

	/* Output to stdout. */
	if (true) printf("\t#end_attribute_info\n");

      }
    }

    fprintf(fp,"#end_fields\n");

    /* Output to stdout. */
    if (true) printf("#end_fields\n");


  }
  
}

void ClassFileReader::dumpMethods(FILE *fp) {

  /* Total number of bytecodes. */
  int totalBytecodes = 0;

  fprintf(fp,"#methods_count\n");
  fprintf(fp,"%i\n",methodCount);

  /* Output to stdout. */ 
  if (true) {
    printf("#methods_count\n");
    printf("%i\n",methodCount);
  }

  if (methodCount > 0) {

    fprintf(fp,"#begin_methods\n");

    /* Output to stdout. */
    if (true) printf("#begin_methods\n");

    for (uint i = 0; i < methodCount; i++) {

      ConstantUtf8 *method = methodInfo[i]->getName();
      fprintf(fp,"\t#%i %s\n",i,method->getUtfString());
      fprintf(fp,"\taccess_flags %i\n",methodInfo[i]->getAccessFlags());
      fprintf(fp,"\tnameIndex %i descIndex %i\n",methodInfo[i]->getNameIndex(),methodInfo[i]->getDescIndex());
      fprintf(fp,"\tattributes_count %i\n",methodInfo[i]->getAttributeCount());

      /* Output to stdout. */
      if (true) {

	printf("\t#%i %s\n",i,method->getUtfString());
	printf("\taccess_flags %i\n",methodInfo[i]->getAccessFlags());
	printf("\tnameIndex %i descIndex %i\n",methodInfo[i]->getNameIndex(),methodInfo[i]->getDescIndex());
	printf("\tattributes_count %i\n",methodInfo[i]->getAttributeCount());
	
      }

      if (methodInfo[i]->getAttributeCount() > 0) {
	
	fprintf(fp,"\t#begin_attributes\n");

	/* Output to stdout. */
	printf("\t#begin_attributes\n");

	const DoublyLinkedList<AttributeInfoItem> &list = methodInfo[i]->getAttributes();
	int currentCount = 0;
	for (DoublyLinkedList<AttributeInfoItem>::iterator j = list.begin(); !list.done(j); j = list.advance(j)) {
	  
	  AttributeInfoItem &item = list.get(j);
	  
	  fprintf(fp,"\t\t#%i %s\n",currentCount,item.getName());
	  
	  /* Attribute name index */
	  fprintf(fp,"\t\tnameIndex %i\n",item.getNameIndex());

	  /* Attribute length */
	  fprintf(fp,"\t\tlength %i\n",item.getLength());

	  /* Output to stdout. */
	  if (true) {

	    printf("\t\t#%i %s\n",currentCount,item.getName());
	    
	    /* Attribute name index */
	    printf("\t\tnameIndex %i\n",item.getNameIndex());
	    
	    /* Attribute length */
	    printf("\t\tlength %i\n",item.getLength());

	  }
	  
	  /* Get the correct code and generate the corresponding info. */
	  switch(item.getCode()) {

	  case CR_ATTRIBUTE_CODE: { /* Code Attribute */
	    
	    AttributeCode *code = static_cast<AttributeCode *>(&item);

	    /* Max stacks */
	    fprintf(fp,"\t\tmax_stack %i\n",code->getMaxStack());

	    /* Max locals */
	    fprintf(fp,"\t\tmax_locals %i\n",code->getMaxLocals());

	    /* Code length */
	    fprintf(fp,"\t\tcode_length %i\n",code->getCodeLength());

		/* Add to total number of bytecodes. */
		totalBytecodes = totalBytecodes + code->getCodeLength();

	    /* Bytecode */
	    
	    const unsigned char *bytecode = (const unsigned char *)code->getCode();

	    /* Temp way for handling bytecodes. */
	    int k;

	    uint32 codeLength = code->getCodeLength();
	    fprintf(fp,"\t\t#bytecodes ");
	    for ( k = 0; k < (int)codeLength; k++) {
	      fprintf(fp,"%x ",bytecode[k]);
	    }
	    fprintf(fp,"\n");
	    
	    fprintf(fp,"\t\t#begin_bytecodes\n");
	    disassembleBytecodes(fp,bytecode,bytecode+code->getCodeLength(),bytecode,24);
	    fprintf(fp,"\t\t#end_bytecodes\n");

	    /* Exception table length */
	    fprintf(fp,"\t\texception_table_length %i\n",code->getNumExceptions());
	    
	    /* Output to stdout. */
	    if (true) {

	      /* Max stacks */
	      printf("\t\tmax_stack %i\n",code->getMaxStack());
	      
	      /* Max locals */
	      printf("\t\tmax_locals %i\n",code->getMaxLocals());
	      
	      /* Code length */
	      printf("\t\tcode_length %i\n",code->getCodeLength());	    
	      
	      /* Temp way for handling bytecodes. */
	      printf("\t\t#bytecodes ");
	      for ( k = 0; k < (int)codeLength; k++) {
		printf("%x ",bytecode[k]);
	      }
	      printf("\n");

	      /* Bytecode */
	      printf("\t\t#begin_bytecodes\n");
	      disassembleBytecodes(stdout,bytecode,bytecode+code->getCodeLength(),bytecode,24);
	      printf("\t\t#end_bytecodes\n");

	      /* Exception table length */
	      printf("\t\texception_table_length %i\n",code->getNumExceptions());
	      	      
	    }

	    /*
	     * The following Exception_Table loop needs to be tested. 
	     */
	    if (code->getNumExceptions() > 0) {

	      ExceptionItem **exceptionItem = code->getExceptions();

	      fprintf(fp,"\t\t#begin_exception_table\n");
	      
	      /* Output to stdout. */
	      if (true) printf("\t\t#begin_exception_table\n");
	      
	      for ( k = 0; k < (int)code->getNumExceptions(); k++) {


		fprintf(fp,"\t\t\t#%i\n", k);
		
		/* Start/End pc */
		fprintf(fp,"\t\t\tstart_pc %i end_pc %i\n",exceptionItem[k]->getStartPc(),exceptionItem[k]->getEndPc);
		
		/* Handler pc */
		fprintf(fp,"\t\t\thandler_pc %i\n",exceptionItem[k]->getHandlerPc());

		/* Catcher type */
		fprintf(fp,"\t\t\tcatch_type %i\n",exceptionItem[k]->getCatcherIndex());

		/* Output to stdout. */
		if (true) {

		  printf("\t\t\t#%i\n",k);

		  /* Start/End pc */
		  printf("\t\t\tstart_pc %i end_pc %i\n",exceptionItem[k]->getStartPc(),exceptionItem[k]->getEndPc);
		  
		  /* Handler pc */
		  printf("\t\t\thandler_pc %i\n",exceptionItem[k]->getHandlerPc());
		  
		  /* Catcher type */
		  printf("\t\t\tcatch_type %i\n",exceptionItem[k]->getCatcherIndex());

		}

	      }

	      fprintf(fp,"\t\t#end_excpetion_table\n");

	      /* Output to stdout. */
	      if (true) printf("\t\t#end_exception_table\n");

	    }

	    /* Attributes count */
	    fprintf(fp,"\t\tattributes_count %i\n",code->getNumAttributes());

	    /* Output to stdout. */
	    if (true) printf("\t\tattributes_count %i\n",code->getNumAttributes());

	    if (code->getNumAttributes() > 0) {

	      fprintf(fp,"\t\t#begin_attributes\n");

	      /* Output to stdout. */
	      if (true) printf("\t\t#begin_attributes\n");

	      AttributeInfoItem **attributes = code->getAttributes();
	      for (uint16 l = 0; l < code->getNumAttributes(); l++) {

		fprintf(fp,"\t\t\t#%i %s\n",l,attributes[l]->getName());
		
		/* Attribute name index */
		fprintf(fp,"\t\t\tnameIndex %i\n",attributes[l]->getNameIndex());
		
		/* Attribute length */
		fprintf(fp,"\t\t\tlength %i\n",attributes[l]->getLength());
		
		/* Output to stdout. */
		if (true) {
		  
		  printf("\t\t\t#%i %s\n",l,attributes[l]->getName());
		  
		  /* Attribute name index */
		  printf("\t\t\tnameIndex %i\n",attributes[l]->getNameIndex());
		  
		  /* Attribute length */
		  printf("\t\t\tlength %i\n",attributes[l]->getLength());
		  
		}

		/* Get the correct code and generate the corresponding attribute. */
		switch(attributes[l]->getCode()) {
		  
		case CR_ATTRIBUTE_LINENUMBERTABLE: { /* LineNumberTable Attribute */
		  
		  AttributeLineNumberTable *lineNoTable = (AttributeLineNumberTable*)attributes[l];
		  
		  /* Line number table length */
		  fprintf(fp,"\t\t\ttable_length %i\n",lineNoTable->getNumEntries());

		  /* Output to stdout. */
		  if (true) {
		    /* Line number table length */
		    printf("\t\t\ttable_length %i\n",lineNoTable->getNumEntries());
		  }

		  if (lineNoTable->getNumEntries() > 0) {

		    LineNumberEntry *lineNoEntry = lineNoTable->getEntries();

		    fprintf(fp,"\t\t\t#begin_line_number_table\n");

		    /* Output to stdout. */
		    if (true) printf("\t\t\t#begin_line_number_table\n");

		    for(uint16 m = 0; m < lineNoTable->getNumEntries(); m++) {

		      fprintf(fp,"\t\t\t\t#%i\n",m);

		      /* Start pc */
		      fprintf(fp,"\t\t\t\tstart_pc %i\n",lineNoEntry[m].startPc);
		      
		      /* Line number */
		      fprintf(fp,"\t\t\t\tline_number %i\n",lineNoEntry[m].lineNumber);
		      
		      /* Output to stdout. */
		      if (true) {

			printf("\t\t\t\t#%i\n",m);

			/* Start pc */
			printf("\t\t\t\tstart_pc %i\n",lineNoEntry[m].startPc);
			
			/* Line number */
			printf("\t\t\t\tline_number %i\n",lineNoEntry[m].lineNumber);
		      
		      }
		    
		    }

		    fprintf(fp,"\t\t\t#end_line_number_table\n");
		    
		    /* Output to stdout. */
		    if (true) printf("\t\t\t#end_line_number_table\n");		    

		  }
		  
		  break;
		  
		}

		/*
		 * The LocalVariableTable case needs to be tested. 
		 */
		case CR_ATTRIBUTE_LOCALVARIABLETABLE: { /* LocalVariableTable */
		  
		  AttributeLocalVariableTable *localVariableTable = (AttributeLocalVariableTable*)attributes[l];

		  /* Local variable table length */
		  fprintf(fp,"\t\t\ttable_length %i\n",localVariableTable->getNumEntries());

		  /* Output to stdout. */
		  if (true) {
		    /* Local variable table length */
		    printf("\t\t\ttable_length %i\n",localVariableTable->getNumEntries());
		  }

		  if(localVariableTable->getNumEntries() > 0) {

		    fprintf(fp,"\t\t\t#begin_local_variable_table\n");
		    
		    /* Output to stdout. */
		    if (true) printf("\t\t\t#begin_local_variable_table\n");

		    LocalVariableEntry *localVarEntry = localVariableTable->getEntries();
		    
		    for (uint16 m = 0; m < localVariableTable->getNumEntries(); m++) {

		      fprintf(fp,"\t\t\t\t#%i\n",m);

		      /* Start pc */
		      fprintf(fp,"\t\t\t\tstart_pc %i\n",localVarEntry[m].startPc);
		      
		      /* Length */
		      fprintf(fp,"\t\t\t\tlength %i\n",localVarEntry[m].length);

		      /* Name index */
		      fprintf(fp,"\t\t\t\tnameIndex %i\n",localVarEntry[m].nameIndex);

		      /* Description index */
		      fprintf(fp,"\t\t\t\tdescIndex %i\n",localVarEntry[m].descIndex);

		      /* Index */
		      fprintf(fp,"\t\t\t\tindex %i\n",localVarEntry[m].index);

		      /* Output to stdout. */
		      if (true) {

			printf("\t\t\t\t#%i\n",m);
			
			/* Start pc */
			printf("\t\t\t\tstartPc %i\n",localVarEntry[m].startPc);
			
			/* Length */
			printf("\t\t\t\tlength %i\n",localVarEntry[m].length);
			
			/* Name index */
			printf("\t\t\t\tnameIndex %i\n",localVarEntry[m].nameIndex);
			
			/* Description index */
			printf("\t\t\t\tdescIndex %i\n",localVarEntry[m].descIndex);
			
			/* Index */
			printf("\t\t\t\tindex %i\n",localVarEntry[m].index);
			
		      }
		      
		    }

		    fprintf(fp,"\t\t\t#end_local_variable_table\n");

		    /* Output to stdout. */
		    if (true) printf("\t\t\t#end_local_variable_table\n");

		  }

		}
		
		default: { /* Ignore everything else. */
		  break;
		}
		
		}
		
	      }

	      fprintf(fp,"\t\t#end_attributes\n");

	      /* Output to stdout. */
	      if (true) printf("\t\t#end_attributes\n");
	    
	    }

	    break;

	  }
	  
	  /*
	   * The following Exceptions case needs to be tested.
	   */
	  case CR_ATTRIBUTE_EXCEPTIONS: { /* Exceptions Attribute */

	    AttributeExceptions *exceptions = static_cast<AttributeExceptions *>(&item);

      	    /* Number of exceptions */
	    fprintf(fp,"\t\texceptions_count %i\n",exceptions->getNumExceptions());
   
	    /* Output to stdout. */
	    if (true) {
	      /* Number of exceptions */
	      fprintf(fp,"\t\texceptions_count %i\n",exceptions->getNumExceptions());
   	    }	

	    if (exceptions->getNumExceptions() > 0) {

	      fprintf(fp,"\t\t#begin_exception_index_table\n");

	      /* Output to stdout. */
	      if (true) printf("\t\t#begin_exception_index_table\n");

	      uint16 *indices = exceptions->getExcIndices();
	      for (uint16 k = 0; k < exceptions->getNumExceptions(); k++) {
		
		fprintf(fp,"\t\t\t#%i\n",k);

		/* Index */
		fprintf(fp,"\t\t\tindex %i\n",indices[k]);

		/* Output to stdout. */
		if (true) {
		  fprintf(fp,"\t\t\t#%i\n",k);
		  
		  /* Index */
		  fprintf(fp,"\t\t\tindex %i\n",indices[k]);
		}

	      }

	      fprintf(fp,"\t\t#end_exception_index_table\n");

	      /* Output to stdout. */
	      if (true) printf("\t\t#end_exception_index_table\n");

	    }

	    break;

	  }

	  default: { /* Ignore everything else. */
	    break;
	  }

	  }

	  currentCount++;

	}
	
	fprintf(fp,"\t#end_attributes\n");

	/* Output to stdout. */
	printf("\t#end_attributes\n");

      }

    }

    fprintf(fp,"#end_methods\n");
    
    /* Output to stdout. */
    if (true) printf("#end_methods\n");

  }

  printf("TOTAL NUMBER OF BYTECODES: %i", totalBytecodes);

}

void ClassFileReader::dumpAttributes(FILE *fp) {

  fprintf(fp,"#attributes_count\n");
  fprintf(fp,"%i\n",attributeCount);

  /* Output to stdout. */
  if (true) {
    printf("#attributes_count\n");
    printf("%i\n",attributeCount);
  }

  if (attributeCount > 0) {

    fprintf(fp,"#begin_attributes\n");

    /* Output to stdout. */
    if (true) printf("#begin_attributes\n");

    for(uint i = 0; i < attributeCount; i++ ) {
    
      fprintf(fp,"\t#%i %s\n",i,attributeInfo[i]->getName());
      
      /* Attribute name index */
      fprintf(fp,"\tnameIndex %i\n",attributeInfo[i]->getNameIndex());
      
      /* Attribute length */
      fprintf(fp,"\tlength %i\n",attributeInfo[i]->getLength());
      
      /* Output to stdout. */
      if (true) {
	
	printf("\t#%i %s\n",i,attributeInfo[i]->getName());
	
	/* Attribute name index */
	printf("\tnameIndex %i\n",attributeInfo[i]->getNameIndex());
	
	/* Attribute length */
	printf("\tlength %i\n",attributeInfo[i]->getLength());
	
      }
      
      /* Get the correct code and generate the corresponding attribute. */
      switch(attributeInfo[i]->getCode()) {
	
      case CR_ATTRIBUTE_SOURCEFILE: { /* SourceFile Attribute */
	
	AttributeSourceFile *srcFile = (AttributeSourceFile*)attributeInfo[i];

	fprintf(fp,"\tindex %i\n",srcFile->getIndex());

	/* Output to stdout. */
	printf("\tindex %i\n",srcFile->getIndex());

	break;

      }

      default: { /* Ignore everything else. */
	break;
      }
      
      }
    }
  
    fprintf(fp,"#end_attributes\n");

    /* Output to stdout. */
    if (true) printf("#end_attributes\n");

  }
}

#endif

#define CR_BUFFER_SIZE 16184


#define JAVA_MAGIC 0xCAFEBABE

CrError ClassFileReader::readConstantPool()
{
  uint16 cIndex, tIndex;
  uint16 index;
  uint32 low, high;
  int i;
  constantPool[0] = 0;

  for (i = 1; i < constantPoolCount; i++) {
    char tag;

    if (!fr->readU1(&tag, 1)) {
      return crErrorIO;
    }

    switch (tag) {
    case CR_CONSTANT_UTF8: {
      uint16 len;

      if (!fr->readU2(&len, 1)) {
	return crErrorIO;
      }
      
      ConstantUtf8 *utf = new (p) ConstantUtf8(p, len);      
      char *buf = new char[len+1];
      fr->readU1(buf, len);
      buf[len] = 0;
      
      /* Do we intern all strings that we get, or just class names? */
      utf->data = sp.intern(buf);
      delete buf;

      constantPool[i] = utf;
      break;
    }

    case CR_CONSTANT_INTEGER: {
      int32 val;


      if (!fr->readU4((uint32 *) &val, 1)) {
	return crErrorIO;
      }

      constantPool[i] = new (p) ConstantInt(p, val);
      break;
    }

    case CR_CONSTANT_FLOAT: {
      uint32 raw;

      if (!fr->readU4(&raw, 1)) {
	return crErrorIO;
      }

      constantPool[i] = new (p) ConstantFloat(p, raw);
      break;
    }

    case CR_CONSTANT_LONG:
      if (!fr->readU4(&high, 1)) {
	return crErrorIO;
      }

      if (!fr->readU4(&low, 1)) {
	return crErrorIO;
      }

      constantPool[i] = new (p) ConstantLong(p, low, high);
      constantPool[i+1] = 0;
      
      /* Longs and doubles take up two constant-pool entries */
      i++;
      break;
      
    case CR_CONSTANT_DOUBLE: {
      char buf[8];

      if (!fr->readU1(buf, 8)) {
	return crErrorIO;
      }

      constantPool[i] = new (p) ConstantDouble(p, buf);
      constantPool[i+1] = 0;

      /* Longs and doubles take up two constant-pool entries */
      i++;
      break;
    }

    case CR_CONSTANT_CLASS:
      if (!fr->readU2(&index, 1)) {
#ifdef DEBUG
	print(0, "CR_CONSTANT_CLASS: Cannot read index\n");
#endif
	return crErrorIO;
      }

      if (invalidIndex(index)) {
#ifdef DEBUG
	print(0, "CR_CONSTANT_CLASS: Invalid index %d\n", index);
#endif
	return crErrorInvalidConstant;
      }

      constantPool[i] = new (p) ConstantClass(p, constantPool, index);
      break;

    case CR_CONSTANT_STRING:
      if (!fr->readU2(&index, 1)) {
	return crErrorIO;
      }
      
      if (invalidIndex(index)) {
#ifdef DEBUG
	print(0, "CR_CONSTANT_STRING: Invalid index %d\n", index);
#endif
	return crErrorInvalidConstant;
      }

      constantPool[i] = new (p) ConstantString(p, constantPool, index);
      break;


    case CR_CONSTANT_FIELDREF:
      if (!fr->readU2(&cIndex, 1)) {
	return crErrorIO;
      }

      if (!fr->readU2(&tIndex, 1)) {
	return crErrorIO;
      }

      if (invalidIndex(cIndex) || invalidIndex(tIndex)) {
#ifdef DEBUG
	print(0, "CR_CONSTANT_FIELDREF: Invalid indices %d %d\n",
	       cIndex, tIndex);
#endif
	return crErrorInvalidConstant;
      }
	
      constantPool[i] = new (p) ConstantFieldRef(p, constantPool, 
						 cIndex, tIndex);
      break;

    case CR_CONSTANT_METHODREF:
      if (!fr->readU2(&cIndex, 1)) {
	return crErrorIO;
      }

      if (!fr->readU2(&tIndex, 1)) {
	return crErrorIO;
      }

      if (invalidIndex(cIndex) || invalidIndex(tIndex)) {
#ifdef DEBUG
	print(0, "CR_CONSTANT_METHODREF: Invalid indices %d %d\n",
	       cIndex, tIndex);
#endif
	return crErrorInvalidConstant;
      }

      constantPool[i] = new (p) ConstantMethodRef(p, constantPool,
						  cIndex, tIndex);
      break;

    case CR_CONSTANT_INTERFACEMETHODREF:
      if (!fr->readU2(&cIndex, 1)) {
	return crErrorIO;
      }

      if (!fr->readU2(&tIndex, 1)) {
	return crErrorIO;
      }

      if (invalidIndex(cIndex) || invalidIndex(tIndex)) {
#ifdef DEBUG
	print(0, "CR_CONSTANT_INTERFACEMETHODREF: Invalid indices %d %d\n",
	       cIndex, tIndex);
#endif
	return crErrorInvalidConstant;
      }

      constantPool[i] = new (p) ConstantInterfaceMethodRef(p, constantPool, 
							   cIndex, tIndex);
      break;
      
    case CR_CONSTANT_NAMEANDTYPE: {
      uint16 nIndex, dIndex;

      if (!fr->readU2(&nIndex, 1)) {
	return crErrorIO;
      }

      if (!fr->readU2(&dIndex, 1)) {
	return crErrorIO;
      }

      if (invalidIndex(nIndex) || invalidIndex(dIndex)) {
#ifdef DEBUG
	print(0, "CR_CONSTANT_NAMEANDTYPE: Invalid indices %d %d\n",
	       nIndex, dIndex);
#endif

	return crErrorInvalidConstant;
      }

      constantPool[i] = new (p) ConstantNameAndType(p, constantPool, 
						    nIndex, dIndex);
      
      break;
    }

    default:
#ifdef DEBUG
      print(0, "UNKNOWN constant type %d\n", tag);
#endif

      return crErrorUnknownConstantType;
    }
  } 
  
  for (i = 1; i < constantPoolCount; i++)
    if (constantPool[i] && 
	!constantPool[i]->resolveAndValidate()) {
#ifdef DEBUG
      print(0, "Cannot resolveAndValidate entry #%d\n", i);
#endif

      return crErrorInvalidConstant;
    }

  return crErrorNone;
}

CrError ClassFileReader::readInterfaces()
{
  for (int i = 0; i < interfaceCount; i++) {
    uint16 index;

    if (!fr->readU2(&index, 1))
      return crErrorIO;

    if (invalidIndex(index)) 
      return crErrorInvalidInterface;
    
    interfaceInfo[i].item = constantPool[index];
    interfaceInfo[i].index = index;
  }

  return crErrorNone;
}


AttributeInfoItem *ClassFileReader::readAttribute(CrError *status) 
{
  uint16 nameIndex;
  AttributeInfoItem *item;
  ConstantUtf8 *utf8;

  *status = crErrorNone;

  if (!fr->readU2(&nameIndex, 1)) {
    *status = crErrorIO;
    return 0;
  }

  /* Do we want to read the rest here to recover? */
  if (invalidIndex(nameIndex)) {
#ifdef DEBUG
    print(0, "ClassFileReader::readAttribute(): Invalid Name Index (%d)\n",
	   nameIndex);
#endif

    *status = crErrorInvalidConstant;
    return 0;
  }

  if (constantPool[nameIndex]->getType() != CR_CONSTANT_UTF8) {
#ifdef DEBUG
    print(0, "ClassFileReader::readAttribute(): Invalid Type(%d, %d)\n",
	   nameIndex, constantPool[nameIndex]->getType());
#endif

    *status = crErrorInvalidConstant;
    return 0;
  }
  
  utf8 = (ConstantUtf8 *) constantPool[nameIndex];
  

  for (int i = 0; i < numAttrHandlers; i++)
    if ((item = attrHandlers[i]->handle(utf8->getUtfString(), nameIndex, status)) != 0 &&
	*status == crErrorNone)
      return item;
    else if (*status != crErrorNone) {
#ifdef DEBUG
      print(0, "Error trying to read attribute (%s); Current handler (%s)\n",
	     utf8->getUtfString(), attrHandlers[i]->getName());
#endif
      return 0;
    }

#ifdef DEBUG
  print(0, "Cannot find a handler for attribute (%s)\n", 
	 utf8->getUtfString());
#endif

  *status = crErrorInvalidAttribute;

  return 0;
}

#define ALIGN(size,mod) ((size)+(((size)%(mod)) ? ((mod)-((size)%(mod))) : 0))

CrError ClassFileReader::readInfoItems(int count, InfoItem **info, int field)
{
  uint32 offset = 4;  /* Offset of first field is 4 */
  CrError def = (field) ? crErrorInvalidField : crErrorInvalidMethod;

  for (int i = 0; i < count; i++) {
    uint16 aFlags, nameIndex, descIndex, attrCount;
    
    if (!fr->readU2(&aFlags, 1))
      return crErrorIO;

    if (!fr->readU2(&nameIndex, 1))
      return crErrorIO;

    if (invalidIndex(nameIndex)) {
#ifdef DEBUG
      print(0, "(%d): Invalid name index %d\n", i, nameIndex);
#endif

      return def;
    }

    if (!fr->readU2(&descIndex, 1))
      return crErrorIO;

    if (invalidIndex(descIndex)) {
#ifdef DEBUG
      print(0, "(%d): Invalid desc index %d\n", i, descIndex);
#endif

      return def;
    }
    
    if (!fr->readU2(&attrCount, 1))
      return crErrorIO;

    if (constantPool[nameIndex]->getType() != CR_CONSTANT_UTF8 ||
	constantPool[descIndex]->getType() != CR_CONSTANT_UTF8) {
#ifdef DEBUG
      print(0, "Invalid type for indices name (%d, %d) or desc (%d, %d)\n",
	     nameIndex, constantPool[nameIndex]->getType(),
	     descIndex, constantPool[descIndex]->getType());
#endif
	     
      return def;
    }

    if (field) {
      FieldInfo *fInfo;
      ConstantUtf8 *desc = (ConstantUtf8 *) constantPool[descIndex];

      fInfo = new (p) FieldInfo(p, aFlags, 
				(ConstantUtf8 *) constantPool[thisClassIndex],
				(ConstantUtf8 *) constantPool[nameIndex],
				desc, nameIndex, descIndex);
      
      JavaType *type;
      fInfo->getType(type);

      const char *next;
      if (!parseFieldDescriptor(p, desc->getUtfString(), *type, &next) ||
	  *next) {
#ifdef DEBUG
	print(0, "Cannot parse field descriptor for field %s\n", 
	       ((ConstantUtf8 *) constantPool[nameIndex])->getUtfString());
#endif
	return crErrorInvalidField;
      }	

      if (!(aFlags & CR_FIELD_STATIC)) {
	fInfo->pos.offset = offset;
	
	offset += getJavaSize(type);
	offset = ALIGN(offset, CR_ALIGNMENT);

      } else {
	int size = getJavaSize(type);
	
	fInfo->pos.a = objectAddress((obj)(new(p) char[size]));	
      }

      info[i] = (InfoItem *) fInfo;
    } else {
      MethodInfo *mInfo;
      ConstantUtf8 *desc = (ConstantUtf8 *) constantPool[descIndex];

      mInfo = new (p) MethodInfo(p, aFlags, 
				 ((ConstantClass *) constantPool[thisClassIndex])->getUtf(),
				 (ConstantUtf8 *) constantPool[nameIndex],
				 (ConstantUtf8 *) constantPool[descIndex],
				 nameIndex, descIndex);
      
#if 0
      printf("Method %s\n", ((ConstantUtf8 *) constantPool[nameIndex])->getUtfString());
#endif

      if (!mInfo->parseMethodDescriptor(desc->getUtfString())) {
#ifdef DEBUG
	print(0, "Cannot parse method descriptor for field %s\n", 
	       ((ConstantUtf8 *) constantPool[nameIndex])->getUtfString());
#endif
	return crErrorInvalidMethod;
      }	
      
      info[i] = mInfo;
    }

    for (int j = 0; j < attrCount; j++) {
      CrError status;     
      AttributeInfoItem *attr = readAttribute(&status);
      
      if (attr)
	info[i]->addAttribute(*attr);
      else {
#ifdef DEBUG
	print(0, "Cannot add attribute to InfoItem (%d)\n", i);
#endif	
	info[i] = 0;
	return crErrorIO;
      }
    }
  }

  if (field)
    objSize = offset;

  return crErrorNone;
}

CrError ClassFileReader::readMethods()
{
  return readInfoItems(methodCount, (InfoItem **) methodInfo, false);  
}

CrError ClassFileReader::readFields()
{
  return readInfoItems(fieldCount, (InfoItem **) fieldInfo, true);
}


CrError ClassFileReader::readAttributes(AttributeInfoItem **attrInfo,
					int attrCount)
{
  CrError status;

  for (int i = 0; i < attrCount; i++)
    if (!(attrInfo[i] = readAttribute(&status))) 
      return status;

  return crErrorNone;
}


ClassFileReader::ClassFileReader(Pool &pool, StringPool &strPool,
				 const char *fileName, 
				 CrError *status) :
  p(pool), sp(strPool)
{
  if (!fileName) {
    *status = crErrorNone;
    return;
  }

  int st;
  fr = new (p) FileReader(p, fileName, &st);

  if (st != 0) {
    *status = crErrorFileNotFound;
    return;
  }

  uint32 magic;
  
  objSize = 0;
  methodInfo = 0;
  methodCount = 0;
  constantPool = 0, interfaceInfo = 0, fieldInfo = 0, 
    attributeInfo = 0;
  
  constantPoolCount = 0, interfaceCount = 0, fieldCount = 0,
    attributeCount = 0;

  numAttrHandlers = 0;
  attrSize = 10;
  attrHandlers = new (p) AttributeHandler *[attrSize];

  if (!fr) {
    *status = crErrorNoMem;
    return;
  }

  if (!fr->readU4(&magic, 1))  {
    *status = crErrorIO;
    return;
  }
    
  if (magic != JAVA_MAGIC) {
    *status = crErrorInvalidFileType;
    return;
  }

  /* get the version numbers */
  if (!fr->readU2(&minorVersion, 1) || !fr->readU2(&majorVersion, 1)) {
    *status = crErrorIO;
    return;
  }

  /* Constant pool count */
  if (!fr->readU2(&constantPoolCount, 1)) {
    *status = crErrorIO;
    return;
  }

  constantPool = new (p) ConstantPoolItem *[constantPoolCount];  

  /* Parse the constant pool */
  if ((*status = readConstantPool()) != 0)
    return;

  /* Register attribute handlers for all attributes that we can
   * handle 
   */
  addAttrHandler(new (p) AttributeHandlerSourceFile(p, fr, this));
  addAttrHandler(new (p) AttributeHandlerConstantValue(p, fr, this));
  addAttrHandler(new (p) AttributeHandlerCode(p, fr, this));
  addAttrHandler(new (p) AttributeHandlerExceptions(p, fr, this));
  addAttrHandler(new (p) AttributeHandlerLineNumberTable(p, fr, this));
  addAttrHandler(new (p) AttributeHandlerLocalVariableTable(p, fr, this));

  /* This must always be added after everything else, since it handles
   * all attributes that we know nothing about
   */
  addAttrHandler(new (p) AttributeHandlerDummy(p, fr, this));
		 
  /* Access Flags */
  if (!fr->readU2(&accessFlags, 1)) {
    *status = crErrorIO;
    return;
  }

  /* This Class, super Class */
  if (!fr->readU2(&thisClassIndex, 1)) {
    *status = crErrorIO;
    return;
  }

  if (!fr->readU2(&superClassIndex, 1)) {
    *status = crErrorIO;
    return;
  }

  if (invalidIndex(thisClassIndex)) {
    *status = crErrorNoClass;
    return;
  }

  if (superClassIndex > 0 && invalidIndex(superClassIndex)) {
    *status = crErrorNoSuperClass;
    return;
  }

  if (constantPool[thisClassIndex]->getType() != CR_CONSTANT_CLASS) {
    *status = crErrorNoClass;
    return;
  }

  /* If we don't have a super-class, we must be java/lang/Object */
  if (superClassIndex < 1) {
#ifdef NO_NSPR
    if (strcmp(((ConstantClass *)constantPool[thisClassIndex])->getUtf()->getUtfString(), "java/lang/Object") != 0) {
#else
    if (PL_strcmp(((ConstantClass *)constantPool[thisClassIndex])->getUtf()->getUtfString(), "java/lang/Object") != 0) {
#endif
      *status = crErrorNoSuperClass;
      return;
    }
  } else {    
    if (constantPool[superClassIndex]->getType() != CR_CONSTANT_CLASS) {
      *status = crErrorNoClass;
      return;
    }
  }

  /* Interfaces */
  if (!fr->readU2(&interfaceCount, 1)) {
    *status = crErrorIO;
    return;
  }
  
  if (interfaceCount > 0) {
    interfaceInfo = new (p) InterfaceInfo[interfaceCount];

    if ((*status = readInterfaces()) != 0)
      return;

  } else
    interfaceInfo = 0;

  /* Fields */
  if (!fr->readU2(&fieldCount, 1)) {
    *status = crErrorIO;
    return;
  }
  
  if (fieldCount > 0) {
    fieldInfo = new (p) FieldInfo *[fieldCount];
    
    if ((*status = readFields()) != 0)
      return;
  } else {
    fieldInfo = 0;
    objSize = 4;
  }

  /* Methods */
  if (!fr->readU2(&methodCount, 1)) {
    *status = crErrorIO;
    return;
  }
  
  if (methodCount > 0) {
    methodInfo = new (p) MethodInfo *[methodCount];
    memset(methodInfo, 0, sizeof(MethodInfo *)*methodCount);
    
    if ((*status = readMethods()) != 0)
      return;
  } else
    methodInfo = 0;

  /* Attributes */
  if (!fr->readU2(&attributeCount, 1)) {
    attributeCount = 0;
    *status = crErrorIO;
    return;
  }
  
  if (attributeCount > 0) {
    attributeInfo = new (p) AttributeInfoItem *[attributeCount];

    if ((*status = readAttributes(attributeInfo, attributeCount)) != 0)
      return;
  } else
    attributeInfo = 0;

  /* At this point, we must be at the end of the file....*/
#if 1
  char dummy;
  if (fr->readU1(&dummy, 1))
    *status = crErrorSpuriousBytes;
  else {
    /* Success... */
    *status = crErrorNone;
  }
#else
  *status = crErrorNone;
#endif
}

void ClassFileReader::addAttrHandler(AttributeHandler *handler)
{
  if (numAttrHandlers+1 > attrSize) {
    AttributeHandler **oldAttrHandlers = attrHandlers;
    attrSize += 5;
    attrHandlers = new (p) AttributeHandler *[attrSize];
    
    for (int i = 0; i < numAttrHandlers; i++)
      attrHandlers[i] = oldAttrHandlers[i];
  }

  attrHandlers[numAttrHandlers++] = handler;
}

bool ClassFileReader::lookupConstant(const char *name, const char *typeStr,
				     uint8 type, 
				     uint16 &index) const
{
  /* Get interned versions of the name and type string */
  const char *namei = sp.get(name);
  const char *typeStri = sp.get(typeStr);

  for (int i = 1; i < constantPoolCount; i++) {
    if (constantPool[i] && 
	constantPool[i]->getType() == type) {
      switch (type) {
      case CR_CONSTANT_METHODREF:
      case CR_CONSTANT_INTERFACEMETHODREF:
      case CR_CONSTANT_FIELDREF: {
	ConstantNameAndType *ninfo = ((ConstantRef *) constantPool[i])->getTypeInfo();
	ConstantUtf8 *nameUtf = ninfo->getName();
	ConstantUtf8 *typeUtf = ninfo->getDesc();

	if (nameUtf->getUtfString() == namei &&
	    typeUtf->getUtfString() == typeStri) {
	  index = i;
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


