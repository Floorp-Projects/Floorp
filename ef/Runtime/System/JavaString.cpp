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
#include "plstr.h"
#include "JavaString.h"
#include "ClassCentral.h"
#include "ClassFileSummary.h"

static inline JavaArray *newCharArray(Uint32 length) 
{
    void *mem = malloc(arrayEltsOffset(tkChar) + getTypeKindSize(tkChar)*length);
    return (new (mem) JavaArray(Array::obtain(tkChar), length));
}


/* Return the UTF representation of this string. This routine allocates 
 * enough memory for the conversion; this memory can be freed using 
 * JavaString::freeUtf()
 */
char *JavaString::convertUtf()
{
  /* XXX Fixme For now, we just copy the string over byte by byte... */
  const int16 *chars = getStr();
  char *copy = new char[count+1];

  int32 i;
  for (i = 0; i < count; i++)
    copy[i] = (char) chars[i];

  copy[i] = 0;
  return copy;
}

void JavaString::freeUtf(char *str)
{
  delete [] str;
}

/* Create a new JavaString from a char array that represents the string in UTF-8
 * format.
 */
JavaString::JavaString(const char *str) : JavaObject(*strType)
{
  count = PL_strlen(str);

  offset = 0;

  /* Let's keep the string zero-terminated anyway */
  value = (JavaArray *) newCharArray(count+1);
  int16 *chars = const_cast<int16 *>(getStr());

  for (int32 i = 0; i < count; i++)
    chars[i] = str[i];

  chars[count] = 0;
}

/* print a textual representation of this string */
void JavaString::dump()
{
  const int16 *chars = getStr();

  for (int16 i = 0; i < count; i++)
    putchar(chars[i]);

  putchar('\n');
}

Type *JavaString::strType;

void JavaString::staticInit()
{
  strType = &asType(Standard::get(cString));
}



