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
#ifndef _CR_INFOITEM_H_
#define _CR_INFOITEM_H_

#include "ConstantPool.h"
#include "DoublyLinkedList.h"

#define CR_FIELD_VOLATILE          0x0040
#define CR_FIELD_STATIC            0x0008
#define CR_FIELD_PUBLIC            0x0001
#define CR_FIELD_PROTECTED         0x0004
#define CR_FIELD_PRIVATE           0x0002
#define CR_FIELD_FINAL             0x0010
#define CR_FIELD_TRANSIENT         0x0080

#define CR_METHOD_STATIC           0x0008
#define CR_METHOD_NATIVE           0x0100
#define CR_METHOD_ABSTRACT         0x0400
#define CR_METHOD_PRIVATE          0x0002
#define CR_METHOD_PUBLIC           0x0001
#define CR_METHOD_PROTECTED        0x0004
#define CR_METHOD_FINAL            0x0010
#define CR_METHOD_SYNCHRONIZED     0x0020


/* Base class for types FieldInfo and MethodInfo */
class InfoItem {
public:
  InfoItem(Pool &p, Uint16 aflags, ConstantUtf8 *classInfo, 
	   ConstantUtf8 *nameInfo, ConstantUtf8 *descInfo);

    
  Uint16 getAccessFlags() const {
    return accessFlags;
  }

  ConstantUtf8 *getName() const {
    return name;
  }

  /* Returns a class that points to the string representing the 
   * type descriptor of the field/method
   */
  ConstantUtf8 *getDescriptor() const {
    return descriptor;
  }

  /* returns the name of the class that this field/method is a member of */
  ConstantUtf8 *getClassName() const {
    return className;
  }

  int getAttributeCount() const {
    return attrCount;
  }

  const DoublyLinkedList <AttributeInfoItem> &getAttributes() const {
    return attributes;
  }

    
  /* Looks up an attribute by attribute-name */
  AttributeInfoItem *getAttribute(const char *_name) const;
  
  /* Look up a known attribute by code name */
  AttributeInfoItem *getAttribute(const Uint32 code) const;

#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif  
  
  bool addAttribute(AttributeInfoItem &attribute);

protected: 
  Pool &p;

private:
  Uint16 accessFlags;
  ConstantUtf8 *name, *descriptor, *className;
  
  int attrCount;
  DoublyLinkedList<AttributeInfoItem> attributes;
  
};

/* A FieldInfo represents one field in a class or interface */
class FieldInfo: public InfoItem {
friend class ClassFileReader;
  
public:
  FieldInfo(Pool &p, Uint16 aflags, ConstantUtf8 *classInfo,
	    ConstantUtf8 *nameInfo, ConstantUtf8 *descInfo) :
  InfoItem(p, aflags, classInfo, nameInfo, descInfo) {}
  
  void getInfo(const char *&sig,
	       bool &isVolatile, bool &isConstant, bool &isStatic) const; 


#ifdef DEBUG
  virtual void dump(int nIndents) const;
#endif  
  
};

/* A MethodInfo represents one method in a class or interface */
class MethodInfo: public InfoItem {
public:
  MethodInfo(Pool &p, Uint16 aflags, ConstantUtf8 *classInfo, 
	     ConstantUtf8 *nameInfo, ConstantUtf8 *descInfo) :
    InfoItem(p, aflags, classInfo, nameInfo, descInfo)
  { } 
  
  /* Get the signature of the method and a few of its flags */
  void getInfo(const char *&sig,
	       bool &isAbstract, bool &isStatic,
	       bool &isFinal, bool &isSynchronized,
	       bool &isNative) const;
  bool isNative() const { return (getAccessFlags() & CR_METHOD_NATIVE) != 0; };
};


#endif /* _CR_INFOITEM_H_ */

