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
#include <stdlib.h>
#include <string.h>

#include "plstr.h"
#include "prprf.h"

#include "ZipArchive.h"
#include "CUtils.h"
#include "StringUtils.h"
#include "LogModule.h"

UT_DEFINE_LOG_MODULE(ZipArchive);

/* Arcane macros and stuff; borrowed from sun_java source */

#define HUGEP 

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Comparision function for sorting and searching */
static int direlcmp(const void *d1, const void *d2)
{
  return PL_strcmp(((DirectoryEntry *)d1)->fn, ((DirectoryEntry *)d2)->fn);
}

/* Public functions */
ZipArchive::ZipArchive(const char *archiveName, Pool &pool,
		       bool &status) : pool(pool), strm(0), dir(0)
{
  status = true;

  fp = PR_Open(archiveName, PR_RDONLY, 0);
  
  if (!fp || !initReader()) {
    status = false;
    return;
  }

}


ZipArchive::~ZipArchive()
{
  if (fp)
    PR_Close(fp);
}

const DirectoryEntry *ZipArchive::lookup(const char *fileName)
{
  // IMPLEMENT
  PR_ASSERT(0);
  return NULL;
}

bool ZipArchive::get(const DirectoryEntry *dp, char *&buf, Int32 &len)
{
  // IMPLEMENT
  return false;
}
  
Uint32 ZipArchive::getNumElements(const char *fn_suffix)
{
  int suf_len = strlen(fn_suffix);
  Uint32 nelems;
  Uint32 i;
  
  for (i=0, nelems=0; i < nel; i++) {
    char *fn = dir[i].fn;
    int fn_len = strlen(fn);
    if (PL_strncasecmp(&fn[fn_len - suf_len], fn_suffix, suf_len) == 0) {
      nelems++;
    }
  }
  return nelems;
}
  
Uint32 ZipArchive::listElements(const char *fn_suffix, char **buf)
{
  int suf_len = strlen(fn_suffix);
  Uint32 nelems;
  Uint32 i;
  buf = new (pool) char *[nel];
  
  for (i=0, nelems=0; i < nel; i++) {
    char *fn = dir[i].fn;
    int fn_len = strlen(fn);
    if ((PL_strncasecmp(&fn[fn_len - suf_len], fn_suffix, suf_len) == 0)) {
      buf[nelems] = dupString(fn, pool);
      nelems++;
    }
  }
  return nelems;
}

/* Private Functions */
/*
 * Initialize zip file reader, read in central directory and construct the
 * lookup table for locating zip file members.
 */
bool ZipArchive::initReader()
{
  // IMPLEMENT
  PR_ASSERT(0);
  return false;
}

/*
 * Find end of central directory (END) header in zip file and set the file
 * pointer to start of header. Return FALSE if the END header could not be
 * found, otherwise return TRUE.
 */
bool ZipArchive::findEnd()
{
  // IMPLEMENT
  PR_ASSERT(0);
  return false;
}

/*
 * Read exactly 'len' bytes of data into 'buf'. Return true if all bytes
 * could be read, otherwise return false.
 */
bool ZipArchive::readFully(void *buf, Int32 len)
{
  char *bp = (char *) buf;

  while (len > 0) {
    Int32 n = PR_Read(fp, bp, len);

    if (n <= 0) 
      return false;
    
    bp += n;
    len -= n;
  }

  return true;
}

/* Fully inflate the zip archive into buffer buf, whose length is len. 
 * Return true on success, false on failure.
 */
bool ZipArchive::inflateFully(Uint32 size, void *buf, Uint32 len, 
			      char **msg)
{
    // IMPLEMENT
    PR_ASSERT(0);
    return false;
}

