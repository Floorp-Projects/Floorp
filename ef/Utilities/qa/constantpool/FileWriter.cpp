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

#include "FileWriter.h"

FileWriter::FileWriter(char *classfile)
{
  fp = fopen(classfile, "wb");
}

int FileWriter::write1(char *destp, int numItems)
{
  fwrite(destp,1,numItems,fp);
  return true;
}

int FileWriter::writeU1(char *destp, int numItems)
{
  return write1(destp, numItems);
}

#ifdef IS_LITTLE_ENDIAN
#define flipU2(x) (((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8)
#else
#define flipU2(x) (x)
#endif

int FileWriter::writeU2(uint16 *destp, int numItems)
{
  
#ifdef IS_LITTLE_ENDIAN
  for (int i = 0; i < numItems; i++)
    destp[i] = flipU2(destp[i]);
#endif  
  
  if (!write1((char *) destp, numItems*2))
    return false;
  
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
  
int FileWriter::writeU4(uint32 *destp, int numItems) {
  
#ifdef IS_LITTLE_ENDIAN
  for (int i = 0; i < numItems; i++)
    destp[i] = flipU4(destp[i]);
#endif  
  
  if (!write1((char *) destp, numItems*4))
    return false;
  
  return true;
}

FileWriter::~FileWriter()
{
  if (fp)
    fclose(fp);
}








