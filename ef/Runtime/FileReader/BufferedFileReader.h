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
#ifndef _BUFFERED_FILEREADER_H_
#define _BUFFERED_FILEREADER_H_

#include "FileReader.h"

#include "prio.h"

/* Reads a file, allocating space as neccessary */
class BufferedFileReader : public FileReader {
public:
    BufferedFileReader(char *buf, Int32 len, Pool &pool) : 
	FileReader(pool), buffer(buf), curp(buf), limit(buffer+len) { }
    
    virtual ~BufferedFileReader() { } 

private:
    char *buffer;
    char *curp;
    char *limit;
    
protected:
    virtual bool read1(char *destp, int size);

};


  
#endif /* _BUFFERED_FILEREADER_H_ */

