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

#ifndef _C_UTILS_H_
#define _C_UTILS_H_

#include "Pool.h"

/* This file is intended to be a repository for C/C++ utils that
 * are reimplemented because the standard C routines are unsuitable
 * for some reason (not thread-safe, not available on all platforms)
 * and NSPR does not have them.
 */

/* Performs the work done by the standard C routine strtok; like
 * strtok, it mangles the original string. Use it as follows:
 *   const char *stringToBeBrokenUp;
 *   Strtok tok(stringToBeBrokenUp);
 *   for (const char *token = tok.get(pattern); token; 
 *        token = tok.get(pattern))
 *      use tok;
 */
class Strtok {
public:
  /* string is a sequence of zero or more text tokens separated
   * by spans of one or more characters in a pattern string.
   */
  Strtok(char *string) : string(string) { } 

  /* returns a pointer to the first character of the next
   * "token" in the string separated by one of the characters
   * in pattern, NULL if there are no nore tokens.
   */
  char *get(const char *pattern);

private:
  /* The candidate string */
  char *string;
  
};

/* duplicate the string, using the Pool provided to alloc memory */
char *dupString(const char *str, Pool &pool);

/* Append t to s, whose initial size (not length) is strSize. Grows s if 
 * necessary; on success, returns the updated s and sets strSize to
 * the new size of the string.
 */
char *append(char *&s, Uint32 &strSize, const char *t);

/* Class that houses a file descriptor and closes the file on exit */
#include "prio.h"

class FileDesc {
private:
  PRFileDesc *fp;

public:
  FileDesc(const char *fileName, PRWord flags, PRWord mode) {
    fp = PR_Open(fileName, flags, mode);
  }

  void close() { if (fp) PR_Close(fp); fp = 0; }
  ~FileDesc() { close();  }

  operator PRFileDesc * (void) { return fp; }
};

#endif /* _C_UTILS_H_ */

