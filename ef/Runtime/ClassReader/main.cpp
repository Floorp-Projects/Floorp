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
/* Test program for ClassReader */

#include "ClassReader.h"
#include "ErrorHandling.h"

#include "nspr.h"

void goAway(const char *name, VerifyError err)
{
  printf("Exception %s thrown; cause %d\n", name, err.cause);
  exit(1);
}

int main(int /* argc */, char **argv)
{
  Pool pool;
  StringPool sp(pool);

  PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 1);

  try {
    ClassFileReader reader(pool, sp, argv[1]);
#ifdef DEBUG
    reader.dump();
#endif    
  } catch (VerifyError  err) {
    goAway("VerifyError", err);
  } 

  printf("Read Class File\n");
  return 0;
}
