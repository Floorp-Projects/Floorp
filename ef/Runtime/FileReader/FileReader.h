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
#ifndef _FILEREADER_H_
#define _FILEREADER_H_

#include "prtypes.h"
#include "Pool.h"

#ifdef NO_NSPR
#include <stdio.h>
#else
#include "prio.h"
#endif

/* Reads a file, allocating space as neccessary */
class FileReader {
public:
  FileReader(Pool &pool): pool(pool) { } 
  
  /* All read routines return True on success, False on failure */

  /* read an unsigned byte */
  bool readU1(char *destp, Int32 numItems);

  /* read numItems unsigned 16-bit quantities, flipping bytes if neccessary.
   * Returns true on success, false if unable to read numItems.
   */
  bool readU2(Uint16 *destp, Int32 numItems);

  /* read an unsigned 32-bit quantity, flipping bytes if neccessary */
  bool readU4(Uint32 *destp, Int32 numItems);
  
  virtual ~FileReader() { } 
  
protected:
  /* Yes this is an abstract class */
  virtual bool read1(char *destp, int size) = 0;
  Pool &pool;
};


  
#endif /* _FILEREADER_H_ */

