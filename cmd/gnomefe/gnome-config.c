/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include <nspr.h>
#include <pwd.h>
#include <sys/types.h>

char *fe_GetConfigDir(void)
{
  char *result, *home;
  
  home = getenv("HOME");
  if(!home) {
    struct passwd *pw = getpwuid(getuid());

    home = pw ? pw->pw_dir : "/";
  }

  result = PR_smprintf("%s/%s", home, MOZ_USER_DIR);
  return result;
}

char *fe_GetConfigDirFilename(char *filename)
{
  return fe_GetConfigDirFilenameWithPrefix("", filename);
}

char *fe_GetConfigDirFilenameWithPrefix(char *prefix, char *filename)
{
  return PR_smprintf("%s%s/%s", prefix, fe_GetConfigDir(), filename);
}
