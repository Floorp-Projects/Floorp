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
#ifndef _PATH_COMPONENT_H_
#define _PATH_COMPONENT_H_

#include "Pool.h"
#include "FileReader.h"
#include "ZipArchive.h"

enum PathComponentType {
  pathComponentTypeInvalid=0,
  pathComponentTypeDirectory,
  pathComponentTypeZipFile,
  pathComponentTypeJarFile
};

/* A path component is one component in a class path. It could
 * be a directory, a zip file or a jar file.
 */
class PathComponent {
public:
  PathComponent(const char *path, Pool &pool);
  
  /* If the class given by className can be found in the domain of
   * this component, return an object that is capable of reading the
   * file. Otherwise, return NULL. maxFileNameLen, if non-zero, indicates
   * the maximum length of a file on the native opearing system. filenames
   * longer than maxFileNameLen are assumed to be truncated to maxFileNameLen.
   */
  FileReader *getFile(const char *className, Uint32 maxFileNameLen = 0);
  
private:
  /* Pool used to allocate memory */
  Pool &pool; 

  /* Type of path component */
  PathComponentType componentType;

  /* component specific information */
  union {
    ZipArchive *archive;  /* Zip archive reader for zip files */
    const char *dirName;  /* Directory name for directories */
  };
};



#endif /* _PATH_COMPONENT_H_ */
