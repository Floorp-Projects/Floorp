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
 
#include "nsFileSpec.h"

#include "nsFileStream.h"
#include "nsDebug.h"

#include "prtypes.h"

#include <string.h>
#include <stdio.h>

//========================================================================================
NS_NAMESPACE nsFileSpecHelpers
//========================================================================================
{
    enum
    {    kMaxFilenameLength = 31                // should work on Macintosh, Unix, and Win32.
    ,    kMaxAltDigitLength    = 5
    ,    kMaxCoreLeafNameLength    = (kMaxFilenameLength - (kMaxAltDigitLength + 1))
    };
    NS_NAMESPACE_PROTOTYPE void LeafReplace(
        char*& ioPath,
        char inSeparator,
        const char* inLeafName);
#ifndef XP_MAC
    NS_NAMESPACE_PROTOTYPE void Canonify(char*& ioPath, bool inMakeDirs);
    NS_NAMESPACE_PROTOTYPE void MakeAllDirectories(const char* inPath, int mode);
#endif
    NS_NAMESPACE_PROTOTYPE char* GetLeaf(const char* inPath, char inSeparator); // allocated
    NS_NAMESPACE_PROTOTYPE char* StringDup(const char* inString, int allocLength = 0);
    NS_NAMESPACE_PROTOTYPE char* AllocCat(const char* inString1, const char* inString2);
    NS_NAMESPACE_PROTOTYPE char* StringAssign(char*& ioString, const char* inOther);
    NS_NAMESPACE_PROTOTYPE char* ReallocCat(char*& ioString, const char* inString1);
} NS_NAMESPACE_END


//----------------------------------------------------------------------------------------
char* nsFileSpecHelpers::StringDup(
    const char* inString,
    int allocLength)
//----------------------------------------------------------------------------------------
{
    if (!allocLength && inString)
        allocLength = strlen(inString);
    char* newPath = inString || allocLength ? new char[allocLength + 1] : nsnull;
    if (!newPath)
        return NULL;
    strcpy(newPath, inString);
    return newPath;
} // nsFileSpecHelpers::StringDup

//----------------------------------------------------------------------------------------
char* nsFileSpecHelpers::AllocCat(
    const char* inString1,
    const char* inString2)
//----------------------------------------------------------------------------------------
{
    if (!inString1)
        return inString2 ? StringDup(inString2) : (char*)NULL;
    if (!inString2)
        return StringDup(inString1);
    char* outString = StringDup(inString1, strlen(inString1) + strlen(inString2));
    if (outString)
        strcat(outString, inString2);
    return outString;
} // nsFileSpecHelpers::StringDup

//----------------------------------------------------------------------------------------
char* nsFileSpecHelpers::StringAssign(
    char*& ioString,
    const char* inString2)
//----------------------------------------------------------------------------------------
{
    if (!inString2)
    {
        delete [] ioString;
        ioString = (char*)NULL;
        return ioString;
    }
    if (!ioString || (strlen(inString2) > strlen(ioString)))
    {
        delete [] ioString;
        ioString = StringDup(inString2);
        return ioString;
    }
    strcpy(ioString, inString2);
    return ioString;
} // nsFileSpecHelpers::StringAssign

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::LeafReplace(
    char*& ioPath,
    char inSeparator,
    const char* inLeafName)
//----------------------------------------------------------------------------------------
{
    // Find the existing leaf name
    if (!ioPath)
        return;
    if (!inLeafName)
    {
        *ioPath = '\0';
        return;
    }
    char* lastSeparator = strrchr(ioPath, inSeparator);
    int oldLength = strlen(ioPath);
    *(++lastSeparator) = '\0'; // strip the current leaf name
    int newLength = lastSeparator - ioPath + strlen(inLeafName);
    if (newLength > oldLength)
    {
        char* newPath = StringDup(ioPath, newLength + 1);
        delete [] ioPath;
        ioPath = newPath;
    }
    strcat(ioPath, inLeafName);
} // nsNativeFileSpec::LeafReplace

//----------------------------------------------------------------------------------------
char* nsFileSpecHelpers::GetLeaf(const char* inPath, char inSeparator)
// Returns a pointer to an allocated string representing the leaf.
//----------------------------------------------------------------------------------------
{
    if (!inPath)
        return NULL;
    char* lastSeparator = strrchr(inPath, inSeparator);
    if (lastSeparator)
        return StringDup(++lastSeparator);
    return StringDup(inPath);
} // nsNativeFileSpec::GetLeaf

#if defined(XP_UNIX) || defined(XP_PC)

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::MakeAllDirectories(const char* inPath, int mode)
// Make the path a valid one by creating all the intermediate directories.  Does NOT
// make the leaf into a directory.  This should be a unix path.
//----------------------------------------------------------------------------------------
{
	if (!inPath)
		return;
		
    char* pathCopy = nsFileSpecHelpers::StringDup( inPath );
    if (!pathCopy)
    	return;

	const char kSeparator = '/'; // I repeat: this should be a unix-style path.
    const int kSkipFirst = 1;

#ifdef XP_WIN
    NS_ASSERTION( pathCopy[2] == '|', "No drive letter!" );
#endif
    char* currentStart = pathCopy;
    char* currentEnd = strchr(currentStart + kSkipFirst, kSeparator);
    if (currentEnd)
    {
		*currentEnd = '\0';
		nsNativeFileSpec spec(nsFilePath(pathCopy, false));
		do
		{
			// If the node doesn't exist, and it is not the initial node in a full path,
			// then make a directory (We cannot make the initial (volume) node).
			if (!spec.Exists() && *currentStart != kSeparator)
				spec.CreateDirectory(mode);
			if (!spec.Exists())
			{
				NS_ASSERTION(spec.Exists(), "Could not create the directory?");
				break;
			}
			currentStart = ++currentEnd;
			currentEnd = strchr(currentStart, kSeparator);
			if (!currentEnd)
				break;
			spec += currentStart; // "lengthen" the path, adding the next node.
		} while (currentEnd);
    }
	delete [] pathCopy;
} // nsFileSpecHelpers::MakeAllDirectories

#endif // XP_PC || XP_UNIX

//----------------------------------------------------------------------------------------
char* nsFileSpecHelpers::ReallocCat(char*& ioString, const char* inString1)
//----------------------------------------------------------------------------------------
{
    char* newString = AllocCat(ioString, inString1);
    delete [] ioString;
    ioString = newString;
    return ioString;
} // nsNativeFileSpec::ReallocCat

#if defined(XP_PC)
#include "windows/nsFileSpecWin.cpp" // Windows-specific implementations
#elif defined(XP_MAC)
#include "nsFileSpecMac.cpp" // Macintosh-specific implementations
#elif defined(XP_UNIX)
#include "unix/nsFileSpecUnix.cpp" // Unix-specific implementations
#endif

//========================================================================================
//                                nsFileURL implementation
//========================================================================================

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const char* inString, bool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mURL(nsnull)
{
    if (!inString)
    	return;
    NS_ASSERTION(strstr(inString, kFileURLPrefix) == inString, "Not a URL!");
    // Make canonical and absolute.
	nsFilePath path(inString + kFileURLPrefixLength, inCreateDirs);
	*this = path;
} // nsFileURL::nsFileURL
#endif

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
:    mURL(nsFileSpecHelpers::StringDup(inOther.mURL))
#ifdef XP_MAC
,    mNativeFileSpec(inOther.GetNativeSpec())
#endif
{
} // nsFileURL::nsFileURL

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
:    mURL(nsFileSpecHelpers::AllocCat(kFileURLPrefix, (const char*)inOther))
#ifdef XP_MAC
,    mNativeFileSpec(inOther.GetNativeSpec())
#endif
{
} // nsFileURL::nsFileURL

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsNativeFileSpec& inOther)
:    mURL(nsFileSpecHelpers::AllocCat(kFileURLPrefix, (const char*)nsFilePath(inOther)))
#ifdef XP_MAC
,    mNativeFileSpec(inOther)
#endif
//----------------------------------------------------------------------------------------
{
} // nsFileURL::nsFileURL

//----------------------------------------------------------------------------------------
nsFileURL::~nsFileURL()
//----------------------------------------------------------------------------------------
{
    delete [] mURL;
}

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
    nsFileSpecHelpers::StringAssign(mURL, inString);
    NS_ASSERTION(strstr(inString, kFileURLPrefix) == inString, "Not a URL!");
#ifdef XP_MAC
    mNativeFileSpec = inString + kFileURLPrefixLength;
#endif
} // nsFileURL::operator =

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
    mURL = nsFileSpecHelpers::StringAssign(mURL, inOther.mURL);
#ifdef XP_MAC
    mNativeFileSpec = inOther.GetNativeSpec();
#endif
} // nsFileURL::operator =

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
    delete [] mURL;
    mURL = nsFileSpecHelpers::AllocCat(kFileURLPrefix, (const char*)inOther);
#ifdef XP_MAC
    mNativeFileSpec  = inOther.GetNativeSpec();
#endif
} // nsFileURL::operator =

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsNativeFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
    delete [] mURL;
    mURL = nsFileSpecHelpers::AllocCat(kFileURLPrefix, (const char*)nsFilePath(inOther));
#ifdef XP_MAC
    mNativeFileSpec  = inOther;
#endif
} // nsFileURL::operator =

//----------------------------------------------------------------------------------------
nsOutputFileStream& operator << (nsOutputFileStream& s, const nsFileURL& url)
//----------------------------------------------------------------------------------------
{
    return (s << url.mURL);
}

//========================================================================================
//                                nsFilePath implementation
//========================================================================================

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const char* inString, bool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mPath(nsFileSpecHelpers::StringDup(inString))
{
    NS_ASSERTION(strstr(inString, kFileURLPrefix) != inString, "URL passed as path");
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, inCreateDirs);
}
#endif

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
:    mPath(nsFileSpecHelpers::StringDup(inOther.mURL + kFileURLPrefixLength))
#ifdef XP_MAC
,    mNativeFileSpec(inOther.GetNativeSpec())
#endif
{
}

#ifdef XP_UNIX
//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsNativeFileSpec& inOther)
//----------------------------------------------------------------------------------------
:    mPath(nsFileSpecHelpers::StringDup(inOther.mPath))
{
}
#endif // XP_UNIX

//----------------------------------------------------------------------------------------
nsFilePath::~nsFilePath()
//----------------------------------------------------------------------------------------
{
    delete [] mPath;
}

#ifdef XP_UNIX
//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsNativeFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
    mPath = nsFileSpecHelpers::StringAssign(mPath, inOther.mPath);
}
#endif // XP_UNIX

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
    NS_ASSERTION(strstr(inString, kFileURLPrefix) != inString, "URL passed as path");
#ifdef XP_MAC
    mNativeFileSpec = inString;
	nsFileSpecHelpers::StringAssign(mPath, (const char*)nsFilePath(mNativeFileSpec));
#else
    nsFileSpecHelpers::StringAssign(mPath, inString);
    nsFileSpecHelpers::Canonify(mPath, true);
#endif
}

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
    nsFileSpecHelpers::StringAssign(mPath, (const char*)nsFilePath(inOther));
#ifdef XP_MAC
    mNativeFileSpec = inOther.GetNativeSpec();
#endif
}

//========================================================================================
//                                nsNativeFileSpec implementation
//========================================================================================

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec()
//----------------------------------------------------------------------------------------
:    mPath(NULL)
{
}
#endif

//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec(const nsFileURL& inURL)
//----------------------------------------------------------------------------------------
#ifndef XP_MAC
:    mPath(NULL)
#endif
{
    *this = nsFilePath(inURL); // convert to unix path first
}

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::MakeUnique(const char* inSuggestedLeafName)
//----------------------------------------------------------------------------------------
{
    if (inSuggestedLeafName && *inSuggestedLeafName)
        SetLeafName(inSuggestedLeafName);

    MakeUnique();
} // nsNativeFileSpec::MakeUnique

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::MakeUnique()
//----------------------------------------------------------------------------------------
{
    if (!Exists())
        return;

    char* leafName = GetLeafName();
    if (!leafName)
        return;

    char* lastDot = strrchr(leafName, '.');
    char* suffix = "";
    if (lastDot)
    {
        suffix = nsFileSpecHelpers::StringDup(lastDot); // include '.'
        *lastDot = '\0'; // strip suffix and dot.
    }
    const int kMaxRootLength
        = nsFileSpecHelpers::kMaxCoreLeafNameLength - strlen(suffix) - 1;
    if (strlen(leafName) > kMaxRootLength)
        leafName[kMaxRootLength] = '\0';
    for (short index = 1; index < 1000 && Exists(); index++)
    {
        // start with "Picture-1.jpg" after "Picture.jpg" exists
        char newName[nsFileSpecHelpers::kMaxFilenameLength + 1];
        sprintf(newName, "%s-%d%s", leafName, index, suffix);
        SetLeafName(newName);
    }
    if (*suffix)
        delete [] suffix;
    delete [] leafName;
} // nsNativeFileSpec::MakeUnique

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator = (const nsFileURL& inURL)
//----------------------------------------------------------------------------------------
{
    *this = nsFilePath(inURL); // convert to unix path first
}

//========================================================================================
//                                UNIX & WIN nsNativeFileSpec implementation
//========================================================================================

#ifdef XP_UNIX
//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec(const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
:    mPath(nsFileSpecHelpers::StringDup((const char*)inPath))
{
}
#endif // XP_UNIX

#ifdef XP_UNIX
//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator = (const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
    nsFileSpecHelpers::StringAssign(mPath, (const char*)inPath);
}
#endif //XP_UNIX

#if defined(XP_UNIX) || defined(XP_PC)
//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec(const nsNativeFileSpec& inSpec)
//----------------------------------------------------------------------------------------
:    mPath(nsFileSpecHelpers::StringDup(inSpec.mPath))
{
}
#endif //XP_UNIX

#if defined(XP_UNIX) || defined(XP_PC)
//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec(const char* inString, bool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mPath(nsFileSpecHelpers::StringDup(inString))
{
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, inCreateDirs);
}
#endif //XP_UNIX,PC

//----------------------------------------------------------------------------------------
nsNativeFileSpec::~nsNativeFileSpec()
//----------------------------------------------------------------------------------------
{
#ifndef XP_MAC
    delete [] mPath;
#endif
}

#if defined(XP_UNIX) || defined(XP_PC)
//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator = (const nsNativeFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
    mPath = nsFileSpecHelpers::StringAssign(mPath, inSpec.mPath);
}
#endif //XP_UNIX


#if defined(XP_UNIX) || defined(XP_PC)
//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
    mPath = nsFileSpecHelpers::StringAssign(mPath, inString);
}
#endif //XP_UNIX

#if (defined(XP_UNIX) || defined(XP_PC))
//----------------------------------------------------------------------------------------
nsOutputFileStream& operator << (nsOutputFileStream& s, const nsNativeFileSpec& spec)
//----------------------------------------------------------------------------------------
{
    return (s << (const char*)spec.mPath);
}
#endif // DEBUG && XP_UNIX

//----------------------------------------------------------------------------------------
nsNativeFileSpec nsNativeFileSpec::operator + (const char* inRelativePath) const
//----------------------------------------------------------------------------------------
{
    nsNativeFileSpec result = *this;
    result += inRelativePath;
    return result;
} // nsNativeFileSpec::operator +

