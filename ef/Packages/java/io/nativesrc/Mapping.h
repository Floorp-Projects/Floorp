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
#ifndef _FD_MAPPING_H_
#define _FD_MAPPING_H_

#include <string.h>

/* The current JDK Java source uses an int32 to represent a file-descriptor.
 * NSPR uses pointers to PRFileDesc's. So we need a mapping from one to the
 * other.
 */
struct Mapping {
  PRFileDesc **descs;
  int32 numDescs;
  
  /* Initialize the mapping mechanism and the values of STDIN, STDOUT
   * and STDERR.
   */
  Mapping() {init(); }
  
  void init() {
    numDescs = 10;
    descs = new PRFileDesc * [numDescs];
    memset(descs, 0, numDescs * sizeof(PRFileDesc *));
    descs[0] = PR_STDIN;
    descs[1] = PR_STDOUT;
    descs[2] = PR_STDERR;
	}

  /* Return true if the given fd is valid. */
  bool valid(int fd) { return (fd < numDescs && descs[fd] != 0); }

  /* Add a new file descriptor and return the corresponding fd */
  int add(PRFileDesc *desc) {
    int32 i;
    for (i = 0; i < numDescs; i++)
      if (!descs[i]) {
	descs[i] = desc;
	return i;
      } else if (descs[i] == desc) /* Already added by someone else */
	return i;

    PRFileDesc **temp = descs;
    descs = new PRFileDesc * [numDescs = numDescs+10];
    memset(descs, 0, sizeof(PRFileDesc *)*numDescs);
    for (int32 j = 0; j < numDescs; j++)
      descs[j] = temp[j];

    delete [] temp;
    
    descs[i] = desc;
    return i;
  }

  PRFileDesc *get(int32 fd) { assert(fd < numDescs); return descs[fd]; }

  void close(int32 fd) { 
    assert(fd < numDescs); 

    if (valid(fd))
      PR_Close(descs[fd]);

    descs[fd] = 0;
  }

  ~Mapping() {
    delete [] descs;
  }
};


#endif /* _FD_MAPPING_H_ */
