/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _IdlVariable_h__
#define _IdlVariable_h__

#if defined(XP_UNIX) || defined (XP_MAC)
#include <stddef.h>
#endif

#include "IdlObject.h"

enum Type {
  TYPE_BOOLEAN = 1,
  TYPE_FLOAT,
  TYPE_DOUBLE,
  TYPE_LONG,
  TYPE_SHORT,
  TYPE_ULONG,
  TYPE_USHORT,
  TYPE_CHAR,
  TYPE_INT,
  TYPE_UINT,
  TYPE_STRING,
  TYPE_OBJECT,
  TYPE_XPIDL_OBJECT,
  TYPE_FUNC,
  TYPE_VOID
};

class IdlVariable : public IdlObject {
private:
  Type mType;
  char *mTypeName;
  union {
    unsigned long vLong;
    char vChar;
    char *vString;
    double vDouble;
    void *vObject;
  } mValue;

public:
                  IdlVariable();
                  ~IdlVariable();

  void            SetType(Type aType);
  Type            GetType();
  void            SetTypeName(char *aTypeName);
  char*           GetTypeName(void);
  void            GetTypeAsString(char *aString, size_t aStringSize);
  
  void            SetValue(unsigned long aValue);
  void            SetValue(char aValue);
  void            SetValue(char *aValue);
  void            SetValue(double aValue);
  void            SetValue(void *aValue);

  unsigned long   GetLongValue();
  char            GetCharValue();
  char*           GetStringValue();
  double          GetDoubleValue();
  void*           GetObjectValue();

private:
  void            DeleteStringType();
};

#endif

