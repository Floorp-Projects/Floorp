/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 
//    This file is included by nsFileSpec.cpp, and includes the Unix-specific
//    implementations.

#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include "nsError.h"

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::Canonify(char*& ioPath, PRBool inMakeDirs)
// Canonify, make absolute, and check whether directories exist
//----------------------------------------------------------------------------------------
{
    if (!ioPath)
        return;
    if (inMakeDirs)
    {
        const mode_t mode = 0700;
        nsFileSpecHelpers::MakeAllDirectories(ioPath, mode);
    }
    char buffer[MAXPATHLEN];
    errno = 0;
    *buffer = '\0';
    char* canonicalPath = realpath(ioPath, buffer);
    if (!canonicalPath)
    {
        // Linux's realpath() is pathetically buggy.  If the reason for the nil
        // result is just that the leaf does not exist, strip the leaf off,
        // process that, and then add the leaf back.
        char* allButLeaf = nsFileSpecHelpers::StringDup(ioPath);
        if (!allButLeaf)
            return;
        char* lastSeparator = strrchr(allButLeaf, '/');
        if (lastSeparator)
        {
            *lastSeparator = '\0';
            canonicalPath = realpath(allButLeaf, buffer);
            strcat(buffer, "/");
            // Add back the leaf
            strcat(buffer, ++lastSeparator);
        }
        delete [] allButLeaf;
    }
    if (!canonicalPath && *ioPath != '/' && !inMakeDirs)
    {
        // Well, if it's a relative path, hack it ourselves.
        canonicalPath = realpath(".", buffer);
        if (canonicalPath)
        {
            strcat(canonicalPath, "/");
            strcat(canonicalPath, ioPath);
        }
    }
    if (canonicalPath)
        nsFileSpecHelpers::StringAssign(ioPath, canonicalPath);
} // nsFileSpecHelpers::Canonify

//----------------------------------------------------------------------------------------
void nsFileSpec::SetLeafName(const char* inLeafName)
//----------------------------------------------------------------------------------------
{
    nsFileSpecHelpers::LeafReplace(mPath, '/', inLeafName);
} // nsFileSpec::SetLeafName

//----------------------------------------------------------------------------------------
char* nsFileSpec::GetLeafName() const
//----------------------------------------------------------------------------------------
{
    return nsFileSpecHelpers::GetLeaf(mPath, '/');
} // nsFileSpec::GetLeafName

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::Exists() const
//----------------------------------------------------------------------------------------
{
    struct stat st;
    return 0 == stat(mPath, &st); 
} // nsFileSpec::Exists

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsFile() const
//----------------------------------------------------------------------------------------
{
    struct stat st;
    return 0 == stat(mPath, &st) && S_ISREG(st.st_mode); 
} // nsFileSpec::IsFile

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsDirectory() const
//----------------------------------------------------------------------------------------
{
    struct stat st;
    return 0 == stat(mPath, &st) && S_ISDIR(st.st_mode); 
} // nsFileSpec::IsDirectory

//----------------------------------------------------------------------------------------
void nsFileSpec::GetParent(nsFileSpec& outSpec) const
//----------------------------------------------------------------------------------------
{
    nsFileSpecHelpers::StringAssign(outSpec.mPath, mPath);
    char* cp = strrchr(outSpec.mPath, '/');
    if (cp)
        *cp = '\0';
} // nsFileSpec::GetParent

//----------------------------------------------------------------------------------------
void nsFileSpec::operator += (const char* inRelativePath)
//----------------------------------------------------------------------------------------
{
    if (!inRelativePath || !mPath)
        return;
    
    char endChar = mPath[strlen(mPath) - 1];
    if (endChar == '/')
        nsFileSpecHelpers::ReallocCat(mPath, "x");
    else
        nsFileSpecHelpers::ReallocCat(mPath, "/x");
    SetLeafName(inRelativePath);
} // nsFileSpec::operator +=

//----------------------------------------------------------------------------------------
void nsFileSpec::CreateDirectory(int mode)
//----------------------------------------------------------------------------------------
{
    // Note that mPath is canonical!
    mkdir(mPath, mode);
} // nsFileSpec::CreateDirectory

//----------------------------------------------------------------------------------------
void nsFileSpec::Delete(PRBool inRecursive)
// To check if this worked, call Exists() afterwards, see?
//----------------------------------------------------------------------------------------
{
    if (IsDirectory())
    {
        if (inRecursive)
        {
            for (nsDirectoryIterator i(*this); i.Exists(); i++)
            {
                nsFileSpec& child = (nsFileSpec&)i;
                child.Delete(inRecursive);
            }        
        }
        rmdir(mPath);
    }
    else
        remove(mPath);
} // nsFileSpec::Delete



//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Rename(const char* inNewName)
//----------------------------------------------------------------------------------------
{
    // This function should not be used to move a file on disk. 
    if (strchr(inNewName, '/')) 
        return NS_FILE_FAILURE;

    if (PR_Rename(mPath, inNewName) != 0)
    {
        return NS_FILE_FAILURE;
    }
    SetLeafName(inNewName);
    return NS_OK;
} // nsFileSpec::Rename

static int CrudeFileCopy(const char* in, const char* out)
{
	struct stat in_stat;
	int stat_result = -1;

	char	buf [1024];
	FILE	*ifp, *ofp;
	int	rbytes, wbytes;

	if (!in || !out)
		return -1;

	stat_result = stat (in, &in_stat);

	ifp = fopen (in, "r");
	if (!ifp) 
	{
		return -1;
	}

	ofp = fopen (out, "w");
	if (!ofp)
	{
		fclose (ifp);
		return -1;
	}

	while ((rbytes = fread (buf, 1, sizeof(buf), ifp)) > 0)
	{
		while (rbytes > 0)
		{
			if ( (wbytes = fwrite (buf, 1, rbytes, ofp)) < 0 )
			{
				fclose (ofp);
				fclose (ifp);
				unlink(out);
				return -1;
			}
			rbytes -= wbytes;
		}
	}
	fclose (ofp);
	fclose (ifp);

	if (stat_result == 0)
		{
		chmod (out, in_stat.st_mode & 0777);
		}

	return 0;
} // nsFileSpec::Rename

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Copy(const nsFileSpec& inParentDirectory) const
//----------------------------------------------------------------------------------------
{
    // We can only copy into a directory, and (for now) can not copy entire directories
    nsresult result = NS_FILE_FAILURE;

    if (inParentDirectory.IsDirectory() && (! IsDirectory() ) )
    {
        char *leafname = GetLeafName();
        char* destPath = nsFileSpecHelpers::StringDup(inParentDirectory,  ( strlen(inParentDirectory) + 1 + strlen(leafname) ) );
        strcat(destPath, "/");
        strcat(destPath, leafname);
        delete [] leafname;

        result = NS_FILE_RESULT(CrudeFileCopy(*this, destPath));
        
        delete [] destPath;
    }
    return result;
} // nsFileSpec::Copy

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Move(const nsFileSpec& inNewParentDirectory) const
//----------------------------------------------------------------------------------------
{
    // We can only copy into a directory, and (for now) can not copy entire directories
    nsresult result = NS_FILE_FAILURE;

    if (inNewParentDirectory.IsDirectory() && (! IsDirectory() ) )
    {
        char *leafname = GetLeafName();
        char* destPath
            = nsFileSpecHelpers::StringDup(
                inNewParentDirectory,
                strlen(inNewParentDirectory) + 1 + strlen(leafname));
        strcat(destPath, "/");
        strcat(destPath, leafname);
        delete [] leafname;

        result = NS_FILE_RESULT(CrudeFileCopy(*this, destPath));
        if (result == NS_OK)
	{
    // cast to fix const-ness
		((nsFileSpec*)this)->Delete(PR_FALSE);
	}
	delete [] destPath;
    }
    return result;
} 

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Execute(const char* inArgs ) const
//----------------------------------------------------------------------------------------
{
    nsresult result = NS_FILE_FAILURE;
    
    if (! IsDirectory())
    {
        char* fileNameWithArgs
	        = nsFileSpecHelpers::StringDup(mPath, strlen(mPath) + 1 + strlen(inArgs));
        strcat(fileNameWithArgs, " ");
        strcat(fileNameWithArgs, inArgs);

        result = NS_FILE_RESULT(system(fileNameWithArgs));
        delete [] fileNameWithArgs; 
    } 

    return result;

} // nsFileSpec::Execute

//========================================================================================
//                                nsDirectoryIterator
//========================================================================================

//----------------------------------------------------------------------------------------
nsDirectoryIterator::nsDirectoryIterator(
    const nsFileSpec& inDirectory
,    int inIterateDirection)
//----------------------------------------------------------------------------------------
    : mCurrent(inDirectory)
    , mDir(nsnull)
    , mExists(PR_FALSE)
{
    mCurrent += "sysygy"; // prepare the path for SetLeafName
    mDir = opendir((const char*)nsFilePath(inDirectory));
    ++(*this);
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
nsDirectoryIterator::~nsDirectoryIterator()
//----------------------------------------------------------------------------------------
{
    if (mDir)
        closedir(mDir);
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator ++ ()
//----------------------------------------------------------------------------------------
{
    mExists = PR_FALSE;
    if (!mDir)
        return *this;
    char* dot    = ".";
    char* dotdot = "..";
    struct dirent* entry = readdir(mDir);
    if (entry && strcmp(entry->d_name, dot) == 0)
        entry = readdir(mDir);
    if (entry && strcmp(entry->d_name, dotdot) == 0)
        entry = readdir(mDir);
    if (entry)
    {
        mExists = PR_TRUE;
        mCurrent.SetLeafName(entry->d_name);
    }
    return *this;
} // nsDirectoryIterator::operator ++

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator -- ()
//----------------------------------------------------------------------------------------
{
    return ++(*this); // can't do it backwards.
} // nsDirectoryIterator::operator --
