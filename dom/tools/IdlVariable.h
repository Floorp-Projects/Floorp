/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _IdlVariable_h__
#define _IdlVariable_h__

#if defined(XP_UNIX) || defined (XP_MAC) || defined (XP_BEOS)
#include <stddef.h>
#endif

#include "IdlObject.h"

enum Type {
  TYPE_BOOLEAN = 1,
  TYPE_FLOAT,
  TYPE_DOUBLE,
  TYPE_LONG,
  TYPE_LONG_LONG,
  TYPE_SHORT,
  TYPE_ULONG,
  TYPE_ULONG_LONG,
  TYPE_USHORT,
  TYPE_CHAR,
  TYPE_INT,
  TYPE_UINT,
  TYPE_STRING,
  TYPE_OBJECT,
  TYPE_XPIDL_OBJECT,
  TYPE_FUNC,
  TYPE_VOID,
  TYPE_JSVAL,
  TYPE_UNKNOWN
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

