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
#include "PathComponent.h"

#include "prio.h"
#include "plstr.h"
#include "prprf.h"

#include "CUtils.h"
#include "StringUtils.h"

#include "DiskFileReader.h"
#include "ZipFileReader.h"

/* return a path component object that can handle the component given
 * by path
 */
PathComponent::PathComponent(const char *path, Pool &pool) : pool(pool)
{
  PRFileInfo info;

  if (PR_GetFileInfo(path, &info) < 0)
    componentType = pathComponentTypeInvalid;
  else {
    switch (info.type) {
    case PR_FILE_DIRECTORY:
      componentType = pathComponentTypeDirectory;
      dirName = dupString(path, pool);
      break;

    case PR_FILE_FILE: {
      bool status;
      ZipArchive *zarch = new (pool) ZipArchive(path, pool, status);

      if (status) {
	archive = zarch;
	componentType = pathComponentTypeZipFile;
      } else {
	zarch->ZipArchive::~ZipArchive();
	componentType = pathComponentTypeInvalid;
      }

      break;
    }

    default:
      componentType = pathComponentTypeInvalid;
      break;
    }
  }
}


FileReader *PathComponent::getFile(const char *className, 
				   Uint32 maxFileNameLen)
{
  if (componentType == pathComponentTypeInvalid)
    return 0;

  switch (componentType) {
  case pathComponentTypeZipFile: {
    Int32 len = PL_strlen(className)+10;
    TemporaryBuffer tempBuf(len);
    char *fileName = tempBuf;
    
    PR_snprintf(fileName, len, "%s.class", className);    
    const DirectoryEntry *entry = archive->lookup(fileName);

    if (!entry)
      return 0;

    return new (pool) ZipFileReader(pool, *archive, entry);
  }

  case pathComponentTypeDirectory: {
    Int32 fileLen = PL_strlen(className) + PL_strlen(dirName)+10;
    TemporaryBuffer fileBuf(fileLen);
    char *fileName = fileBuf;
    PR_snprintf(fileName, fileLen, "%s/%s.class", dirName, className);

    /* Normal file names on the MAC are truncated to 31 characters */
    if (maxFileNameLen > 0) {
      char *classFileName = PL_strrchr(fileName, '/');

      if (classFileName == NULL)
	classFileName = fileName;
      else
	classFileName += 1;

      Uint32 fileNameLen = PL_strlen(classFileName);

      if (fileNameLen > maxFileNameLen) {
#ifdef XP_MAC
	classFileName[maxFileNameLen] = 'É';
	classFileName[maxFileNameLen + 1] = 0;
#else
	assert(false);
#endif
      }
    }

    PRFileInfo info;
    if (PR_GetFileInfo(fileName, &info) < 0)
      return 0;

    return new DiskFileReader(pool, fileName);
  }
  
  default:
    return 0;
  }
}



