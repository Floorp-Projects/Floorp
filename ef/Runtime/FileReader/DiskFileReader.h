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
#ifndef _DISK_FILEREADER_H_
#define _DISK_FILEREADER_H_

#include "FileReader.h"

#define FR_BUFFERSIZE 8192
/* Reads an uncompressed file off the disk */
class DiskFileReader : public FileReader {
public:
  DiskFileReader(Pool &pool, const char *fileName,
		 Int32 bufferSize = FR_BUFFERSIZE);
  
  virtual ~DiskFileReader();

protected:
  virtual bool read1(char *destp, int size);

private:
  Int32 bufferSize;
  char *buffer;
  char *curp, *limit;
  bool eofReached;

  Int32 fillBuf();

#ifdef NO_NSPR
  FILE *fp;
#else
  PRFileDesc *fp;
#endif
};
  
#endif /* _DISK_FILEREADER_H_ */
