/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 
//	This file is included by nsFileSpec.cp, and includes the Windows-specific
//	implementations.

#include <sys/stat.h>
#include <direct.h>
#include <stdlib.h>
#include "prio.h"

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::Canonify(char*& ioPath, bool inMakeDirs)
// Canonify, make absolute, and check whether directories exist. This
// takes a (possibly relative) native path and converts it into a
// fully qualified native path.
//----------------------------------------------------------------------------------------
{
  if (!ioPath)
    return;
  if (inMakeDirs) {
    const int mode = 0700;
    char* unixStylePath = nsFileSpecHelpers::StringDup(ioPath);
    nsFileSpecHelpers::NativeToUnix(unixStylePath);
    nsFileSpecHelpers::MakeAllDirectories(unixStylePath, mode);
    delete[] unixStylePath;
  }
  char buffer[_MAX_PATH];
  errno = 0;
  *buffer = '\0';
  char* canonicalPath = _fullpath(buffer, ioPath, _MAX_PATH);

  NS_ASSERTION( canonicalPath[0] != '\0', "Uh oh...couldn't convert" );
  if (canonicalPath[0] == '\0')
    return;

  nsFileSpecHelpers::StringAssign(ioPath, canonicalPath);
}

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::UnixToNative(char*& ioPath)
// This just does string manipulation.  It doesn't check reality, or canonify, or
// anything
//----------------------------------------------------------------------------------------
{
	// Allow for relative or absolute.  We can do this in place, because the
	// native path is never longer.
	
	if (!ioPath || !*ioPath)
		return;
		
	char* src = ioPath;
	if (*ioPath == '/')
    {
      // Strip initial slash for an absolute path
      src++;
    }
		
	// Convert the vertical slash to a colon
	char* cp = src + 1;
	
	// If it was an absolute path, check for the drive letter
	if (*ioPath == '/' && strstr(cp, "|/") == cp)
    *cp = ':';
	
	// Convert '/' to '\'.
	while (*++cp)
    {
      if (*cp == '/')
        *cp = '\\';
    }

	if (*ioPath == '/') {
    for (cp = ioPath; *cp; ++cp)
      *cp = *(cp + 1);
  }
}

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::NativeToUnix(char*& ioPath)
// This just does string manipulation.  It doesn't check reality, or canonify, or
// anything.  The unix path is longer, so we can't do it in place.
//----------------------------------------------------------------------------------------
{
	if (!ioPath || !*ioPath)
		return;
		
	// Convert the drive-letter separator, if present
	char* temp = nsFileSpecHelpers::StringDup("/", 1 + strlen(ioPath));

	char* cp = ioPath + 1;
	if (strstr(cp, ":\\") == cp) {
		*cp = '|';    // absolute path
  }
  else {
    *temp = '\0'; // relative path
  }
	
	// Convert '\' to '/'
	for (; *cp; cp++)
    {
      if (*cp == '\\')
        *cp = '/';
    }

	// Add the slash in front.
	strcat(temp, ioPath);
	StringAssign(ioPath, temp);
	delete [] temp;
}

//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec(const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
:	mPath(NULL)
{
	*this = inPath;
}

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator = (const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
	nsFileSpecHelpers::StringAssign(mPath, (const char*)inPath);
	nsFileSpecHelpers::UnixToNative(mPath);
} // nsNativeFileSpec::operator =

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsNativeFileSpec& inSpec)
//----------------------------------------------------------------------------------------
:	mPath(NULL)
{
	*this = inSpec;
} // nsFilePath::nsFilePath

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsNativeFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	nsFileSpecHelpers::StringAssign(mPath, inSpec.mPath);
	nsFileSpecHelpers::NativeToUnix(mPath);
} // nsFilePath::operator =

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::SetLeafName(const char* inLeafName)
//----------------------------------------------------------------------------------------
{
	nsFileSpecHelpers::LeafReplace(mPath, '\\', inLeafName);
} // nsNativeFileSpec::SetLeafName

//----------------------------------------------------------------------------------------
char* nsNativeFileSpec::GetLeafName() const
//----------------------------------------------------------------------------------------
{
	return nsFileSpecHelpers::GetLeaf(mPath, '\\');
} // nsNativeFileSpec::GetLeafName

//----------------------------------------------------------------------------------------
bool nsNativeFileSpec::Exists() const
//----------------------------------------------------------------------------------------
{
	struct stat st;
	return 0 == stat(mPath, &st); 
} // nsNativeFileSpec::Exists

//----------------------------------------------------------------------------------------
bool nsNativeFileSpec::IsFile() const
//----------------------------------------------------------------------------------------
{
  struct stat st;
  return 0 == stat(mPath, &st) && (_S_IFREG & st.st_mode); 
} // nsNativeFileSpec::IsFile

//----------------------------------------------------------------------------------------
bool nsNativeFileSpec::IsDirectory() const
//----------------------------------------------------------------------------------------
{
	struct stat st;
	return 0 == stat(mPath, &st) && (_S_IFDIR & st.st_mode); 
} // nsNativeFileSpec::IsDirectory

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::GetParent(nsNativeFileSpec& outSpec) const
//----------------------------------------------------------------------------------------
{
	nsFileSpecHelpers::StringAssign(outSpec.mPath, mPath);
	char* cp = strrchr(outSpec.mPath, '\\');
	if (cp)
		*cp = '\0';
} // nsNativeFileSpec::GetParent

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator += (const char* inRelativePath)
//----------------------------------------------------------------------------------------
{
	if (!inRelativePath || !mPath)
		return;
	
	if (mPath[strlen(mPath) - 1] != '\\')
		char* newPath = nsFileSpecHelpers::ReallocCat(mPath, "\\");
	SetLeafName(inRelativePath);
} // nsNativeFileSpec::operator +=

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::CreateDirectory(int /*mode*/)
//----------------------------------------------------------------------------------------
{
	// Note that mPath is canonical!
	mkdir(mPath);
} // nsNativeFileSpec::CreateDirectory

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::Delete(bool inRecursive)
//----------------------------------------------------------------------------------------
{
  if (IsDirectory())
    {
	    if (inRecursive)
        {
          for (nsDirectoryIterator i(*this); i; i++)
            {
              nsNativeFileSpec& child = (nsNativeFileSpec&)i;
              child.Delete(inRecursive);
            }		
        }
	    rmdir(mPath);
    }
	else
    {
      remove(mPath);
    }
} // nsNativeFileSpec::Delete

//========================================================================================
//								nsDirectoryIterator
//========================================================================================

//----------------------------------------------------------------------------------------
nsDirectoryIterator::nsDirectoryIterator(
	const nsNativeFileSpec& inDirectory
,	int inIterateDirection)
//----------------------------------------------------------------------------------------
	: mCurrent(inDirectory)
	, mDir(nsnull)
	, mExists(false)
{
    mDir = PR_OpenDir(inDirectory);
	mCurrent += "dummy";
    ++(*this);
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
nsDirectoryIterator::~nsDirectoryIterator()
//----------------------------------------------------------------------------------------
{
    if (mDir)
	    PR_CloseDir(mDir);
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator ++ ()
//----------------------------------------------------------------------------------------
{
	mExists = false;
	if (!mDir)
		return *this;
  PRDirEntry* entry = PR_ReadDir(mDir, PR_SKIP_BOTH); // Ignore '.' && '..'
	if (entry)
    {
      mExists = true;
      mCurrent.SetLeafName(entry->name);
    }
	return *this;
} // nsDirectoryIterator::operator ++

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator -- ()
//----------------------------------------------------------------------------------------
{
	return ++(*this); // can't do it backwards.
} // nsDirectoryIterator::operator --

