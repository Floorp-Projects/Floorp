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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FileReader.h"

bool FileReader::readU1(char *destp, int numItems)
{
  return read1(destp, numItems);
}

#ifdef IS_LITTLE_ENDIAN
#define flipU2(x) (((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8)
#else
#define flipU2(x) (x)
#endif

bool FileReader::readU2(Uint16 *destp, int numItems)
{
  if (!read1((char *) destp, numItems*2))
    return false;

#ifdef IS_LITTLE_ENDIAN
  for (numItems--; numItems >= 0; numItems--)
    destp[numItems] = flipU2(destp[numItems]);
#endif  

  return true;
}

#ifdef IS_LITTLE_ENDIAN
#define flipU4(x) (((x) >> 24)               |\
		   ((((x) <<  8) >> 24) <<  8) |\
		   ((((x) << 16) >> 24) << 16) |\
		   ((x) << 24))
#else
#define flipU4(x) (x)
#endif

bool FileReader::readU4(Uint32 *destp, int numItems)
{
  if (!read1((char *) destp, numItems*4))
    return false;

#ifdef IS_LITTLE_ENDIAN
  for (numItems--; numItems >= 0; numItems--)
    destp[numItems] = flipU4(destp[numItems]);
#endif  

  return true;
}




