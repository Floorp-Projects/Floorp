/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef _NATIVE_DEFS_H_
#define _NATIVE_DEFS_H_

#include "JavaObject.h"

/* Define an array of a generated primitive type */
#define ARRAY_OF_PRIMITIVETYPE(X) \
struct ArrayOf_##X : public JavaArray { \
  ArrayOf_##X(const Type &type, uint32 length) : JavaArray(type, length) { } \
  X elements[1];\
}

ARRAY_OF_PRIMITIVETYPE(char);
ARRAY_OF_PRIMITIVETYPE(uint8);
ARRAY_OF_PRIMITIVETYPE(int16);
ARRAY_OF_PRIMITIVETYPE(uint16);
ARRAY_OF_PRIMITIVETYPE(int32);
ARRAY_OF_PRIMITIVETYPE(uint32);
ARRAY_OF_PRIMITIVETYPE(int64);
ARRAY_OF_PRIMITIVETYPE(uint64);
ARRAY_OF_PRIMITIVETYPE(float);
ARRAY_OF_PRIMITIVETYPE(double);

/* Define an array of a generated class type */
#define ARRAY_OF(X) \
struct ArrayOf_##X : public JavaArray {\
  ArrayOf_##X(const Type &type, uint32 length) : JavaArray(type, length) { }\
  X *elements[1];\
}

#endif /* _NATIVE_DEFS_H_ */
