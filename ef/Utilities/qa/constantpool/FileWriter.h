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




