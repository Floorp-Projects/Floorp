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

#ifndef _CR_FILEWRITER_H_
#define _CR_FILEWRITER_H_

#include "prtypes.h"
#include <stdio.h>

/* Writes a file. */
class FileWriter {
public:
  FileWriter(char *classfile);
  
  /* All write routines return True on success, False on failure */
  
  /* write an unsigned byte */
  int writeU1(char *destp, int numItems);
  
  /* write an unsigned 16-bit quantity */
  int writeU2(uint16 *destp, int numItems);
  
  /* write an unsigned 32-bit quantity */
  int writeU4(uint32 *destp, int numItems);
  
  ~FileWriter();
  
private:
  int write1(char *destp, int size);
  FILE *fp;
};

#endif /* _CR_FILEWRITER_H_ */




