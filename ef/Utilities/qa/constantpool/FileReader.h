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

#ifndef _CR_FILEREADER_H_
#define _CR_FILEREADER_H_

#define FR_BUFFERSIZE 8192

#include "prtypes.h"

#ifdef NO_NSPR
#include <stdio.h>
#else
#include "prio.h"
#endif

#include "Pool.h"

/* Reads a file, allocating space as neccessary */
class FileReader {
public:
  FileReader(Pool &pool, const char *fileName, int *status,
	     int bufferSize = FR_BUFFERSIZE);

  /* All read routines return True on success, False on failure */

  /* read an unsigned byte */
  int readU1(char *destp, int numItems);

  /* read an unsigned 16-bit quantity */
  int readU2(uint16 *destp, int numItems);

  /* read an unsigned 32-bit quantity */
  int readU4(uint32 *destp, int numItems);

  ~FileReader();

private:
  int bufferSize;
  char *buffer;
  char *curp, *limit;
  bool eofReached;

  int fillBuf();
  int read1(char *destp, int size);
  Pool &pool;

#ifdef NO_NSPR
  FILE *fp;
#else
  PRFileDesc *fp;
#endif
};

#endif /* _CR_FILEREADER_H_ */

