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

FileReader::FileReader(Pool &p, const char *fileName, int *status,
		       int _bufferSize) : pool(p)
{
  fp = 0;
  buffer = 0;

  *status = 0;

#ifdef NO_NSPR
  if (!(fp = fopen(fileName, "rb"))) {
#else
  if (!(fp = PR_Open(fileName, PR_RDONLY, 0))) {
#endif
    *status = 1;
    return;
  }
  
  bufferSize = _bufferSize;
  eofReached = false;

  buffer = new (pool) char[bufferSize];

  if (!fillBuf())
    *status = 1;

}

/* Fill the buffer, returning number of items read */
int FileReader::fillBuf() 
{
  if (eofReached)
    return 0;

#ifdef NO_NSPR
  int nread = fread(buffer, 1, bufferSize, fp);
#else
  int nread = PR_Read(fp, buffer, bufferSize);
#endif
  
  if (nread < bufferSize) {
    eofReached = true;
    if (fp)
#ifdef NO_NSPR
      fclose(fp);
#else
      PR_Close(fp);
#endif
    fp = 0;
  }

  curp = buffer;
  limit = buffer+nread;

  return nread;
}

int FileReader::read1(char *destp, int size)
{
  while (curp+size > limit) {
    int nread = (limit-curp);
    memcpy(destp, curp, nread);
    size -= nread;
    destp += nread;

    if (fillBuf() <= 0)
      return false;
  }
  
  memcpy(destp, curp, size);
  curp += size;
  return true;
}
  
int FileReader::readU1(char *destp, int numItems)
{
  return read1(destp, numItems);
}

#ifdef IS_LITTLE_ENDIAN
#define flipU2(x) (((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8)
#else
#define flipU2(x) (x)
#endif

int FileReader::readU2(uint16 *destp, int numItems)
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

int FileReader::readU4(uint32 *destp, int numItems)
{
  if (!read1((char *) destp, numItems*4))
    return false;

#ifdef IS_LITTLE_ENDIAN
  for (numItems--; numItems >= 0; numItems--)
    destp[numItems] = flipU4(destp[numItems]);
#endif  

  return true;
}

FileReader::~FileReader()
{
  if (fp)
#ifdef NO_NSPR
    fclose(fp);
#else
    PR_Close(fp);
#endif
}




