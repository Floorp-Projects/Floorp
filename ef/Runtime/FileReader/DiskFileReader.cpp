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

#include "DiskFileReader.h"
#include "ErrorHandling.h"

DiskFileReader::DiskFileReader(Pool &p, const char *fileName, 
			       Int32 _bufferSize) : FileReader(p)
{
  fp = 0;
  buffer = 0;

#ifdef NO_NSPR
  if (!(fp = fopen(fileName, "rb"))) {
#else
  if (!(fp = PR_Open(fileName, PR_RDONLY, 0))) {
#endif
    verifyError(VerifyError::noClassDefFound);
    return;
  }
  
  bufferSize = _bufferSize;
  eofReached = false;

  buffer = new (pool) char[bufferSize];

  if (!fillBuf())
    verifyError(VerifyError::noClassDefFound);
}

/* Fill the buffer, returning number of items read */
Int32 DiskFileReader::fillBuf() 
{
  if (eofReached)
    return 0;

#ifdef NO_NSPR
  Int32 nread = fread(buffer, 1, bufferSize, fp);
#else
  Int32 nread = PR_Read(fp, buffer, bufferSize);
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

bool DiskFileReader::read1(char *destp, int size)
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
  
DiskFileReader::~DiskFileReader()
{
  if (fp)
#ifdef NO_NSPR
    fclose(fp);
#else
    PR_Close(fp);
#endif
}




