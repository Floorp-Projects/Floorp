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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsNewsSummarySpec.h"
#include "plstr.h"
#include "nsString.h"

nsNewsSummarySpec::nsNewsSummarySpec()
{
}

nsNewsSummarySpec::nsNewsSummarySpec(const char *folderPath)
  : nsFileSpec(folderPath)
{
	CreateSummaryFileName();
}

nsNewsSummarySpec::nsNewsSummarySpec(const nsFileSpec& inFolderPath)
: nsFileSpec(inFolderPath)
{
	CreateSummaryFileName();
}

nsNewsSummarySpec::nsNewsSummarySpec(const nsFilePath &inFolderPath) : nsFileSpec(inFolderPath)
{
	CreateSummaryFileName();
}

void nsNewsSummarySpec::SetFolderName(const char *folderPath)
{
	*this = folderPath;
}

void nsNewsSummarySpec::CreateSummaryFileName()
{
	char *leafName = GetLeafName();

#if defined(XP_WIN16) || defined(XP_OS2)
    const PRUint32 MAX_LEN = 8;
#elif defined(XP_MAC)
    const PRUint32 MAX_LEN = 25;
#else
    const PRUint32 MAX_LEN = 55;
#endif
    // Given a name, use either that name, if it fits on our
    // filesystem, or a hashified version of it, if the name is too
    // long to fit.
    char hashedname[MAX_LEN + 1];
    PRBool needshash = PL_strlen(leafName) > MAX_LEN;
#if defined(XP_WIN16)  || defined(XP_OS2)
    if (!needshash) {
      needshash = PL_strchr(leafName, '.') != NULL ||
        PL_strchr(leafName, ':') != NULL;
    }
#endif
    PL_strncpy(hashedname, leafName, MAX_LEN + 1);
    if (needshash) {
      PR_snprintf(hashedname + MAX_LEN - 8, 9, "%08lx",
                  (unsigned long) StringHash(leafName));
    }
    
	nsAutoString fullLeafName(hashedname, MAX_LEN + 1, eOneByte);

	// Append .msf (message summary file) 
	fullLeafName += ".msf";	

	SetLeafName(fullLeafName.GetBuffer());
	PL_strfree(leafName);
}

/* this used to be XP_StringHash2 from xp_hash.c */
/* phong's linear congruential hash  */
PRUint32
nsNewsSummarySpec::StringHash(const char *ubuf)
{
  unsigned char * buf = (unsigned char*) ubuf;
  PRUint32 h=1;
  while(*buf)
    {
      h = 0x63c63cd9*h + 0x9c39c33d + (int32)*buf;
      buf++;
    }
  return h;
}
