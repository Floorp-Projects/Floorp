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
 
#include "nsFileSpec.h"

#include "nsFileStream.h"
#include "nsDebug.h"
#include "nsEscape.h"

#include "prtypes.h"
#include "plstr.h"
#include "plbase64.h"
#include "prmem.h"

#include <string.h>
#include <stdio.h>

//========================================================================================
//            class nsSimpleCharString
//========================================================================================

//----------------------------------------------------------------------------------------
nsSimpleCharString::nsSimpleCharString()
//----------------------------------------------------------------------------------------
:   mData(nsnull)
{
    
} // nsSimpleCharString::nsSimpleCharString

//----------------------------------------------------------------------------------------
nsSimpleCharString::nsSimpleCharString(const char* inString)
//----------------------------------------------------------------------------------------
:   mData(nsnull)
{
    if (inString)
        CopyFrom(inString, strlen(inString));
} // nsSimpleCharString::nsSimpleCharString

//----------------------------------------------------------------------------------------
nsSimpleCharString::nsSimpleCharString(const nsString& inString)
//----------------------------------------------------------------------------------------
:   mData(nsnull)
{
    *this = inString;
} // nsSimpleCharString::nsSimpleCharString

//----------------------------------------------------------------------------------------
nsSimpleCharString::nsSimpleCharString(const nsSimpleCharString& inOther)
//----------------------------------------------------------------------------------------
{
    mData = inOther.mData;
    AddRefData();
} // nsSimpleCharString::nsSimpleCharString

//----------------------------------------------------------------------------------------
nsSimpleCharString::nsSimpleCharString(const char* inData, PRUint32 inLength)
//----------------------------------------------------------------------------------------
:   mData(nsnull)
{
    CopyFrom(inData, inLength);
} // nsSimpleCharString::nsSimpleCharString

//----------------------------------------------------------------------------------------
nsSimpleCharString::~nsSimpleCharString()
//----------------------------------------------------------------------------------------
{
    ReleaseData();
} // nsSimpleCharString::nsSimpleCharString

//----------------------------------------------------------------------------------------
void nsSimpleCharString::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
    if (inString)
        CopyFrom(inString, strlen(inString));
    else
        SetToEmpty();
} // nsSimpleCharString::operator =

//----------------------------------------------------------------------------------------
void nsSimpleCharString::operator = (const nsString& inString)
//----------------------------------------------------------------------------------------
{
    PRUint32 len = inString.Length();
    ReallocData(len);
    if (!mData)
        return;
    inString.ToCString(mData->mString, len + 1);  
} // nsSimpleCharString::operator =

//----------------------------------------------------------------------------------------
void nsSimpleCharString::operator = (const nsSimpleCharString& inOther)
//----------------------------------------------------------------------------------------
{
    if (mData == inOther.mData)
        return;
    ReleaseData();
    mData = inOther.mData;
    AddRefData();
} // nsSimpleCharString::operator =

//----------------------------------------------------------------------------------------
void nsSimpleCharString::operator += (const char* inOther)
//----------------------------------------------------------------------------------------
{
    if (!inOther)
        return;
    int newLength = Length() + PL_strlen(inOther);
    ReallocData(newLength);
    strcat(mData->mString, inOther);
} // nsSimpleCharString::operator =

//----------------------------------------------------------------------------------------
nsSimpleCharString nsSimpleCharString::operator + (const char* inOther) const
//----------------------------------------------------------------------------------------
{
    nsSimpleCharString result(*this);
    result += inOther;
    return result;
} // nsSimpleCharString::operator =

//----------------------------------------------------------------------------------------
void nsSimpleCharString::Catenate(const char* inString1, const char* inString2)
//----------------------------------------------------------------------------------------
{
    if (!inString2)
    {
        *this += inString1;
        return;
    }
    int newLength = Length() + PL_strlen(inString1) + PL_strlen(inString2);
    ReallocData(newLength);
    strcat(mData->mString, inString1);
    strcat(mData->mString, inString2);
} // nsSimpleCharString::operator =

//----------------------------------------------------------------------------------------
void nsSimpleCharString::CopyFrom(const char* inData, PRUint32 inLength)
//----------------------------------------------------------------------------------------
{
    if (!inData)
        return;
    ReallocData(inLength);
    if (!mData)
        return;
    memcpy(mData->mString, inData, inLength);
    mData->mString[inLength] = '\0';
} // nsSimpleCharString::CopyFrom

//----------------------------------------------------------------------------------------
void nsSimpleCharString::SetToEmpty()
//----------------------------------------------------------------------------------------
{
    ReleaseData();
} // nsSimpleCharString::SetToEmpty

//----------------------------------------------------------------------------------------
void nsSimpleCharString::Unescape()
//----------------------------------------------------------------------------------------
{
    if (!mData)
        return;
    nsUnescape(mData->mString);
    mData->mLength = strlen(mData->mString);       
} // nsSimpleCharString::Unescape


//----------------------------------------------------------------------------------------
void nsSimpleCharString::AddRefData()
//----------------------------------------------------------------------------------------
{
    if (mData)
        ++mData->mRefCount;
} // nsSimpleCharString::AddRefData

//----------------------------------------------------------------------------------------
void nsSimpleCharString::ReleaseData()
//----------------------------------------------------------------------------------------
{
    if (!mData)
        return;
    NS_ASSERTION(mData->mRefCount > 0, "String deleted too many times!");
    if (--mData->mRefCount == 0)
        PR_Free(mData);
    mData = nsnull;
} // nsSimpleCharString::ReleaseData

//----------------------------------------------------------------------------------------
void nsSimpleCharString::ReallocData(PRUint32 inLength)
// Reallocate mData to a new length.  Since this presumably precedes a change to the string,
// we want to detach ourselves if the data is shared by another string, even if the length
// requested would not otherwise require a reallocation.
//----------------------------------------------------------------------------------------
{
    // We always allocate for a string of at least kMinStringSize, to prevent a lot of small
    // reallocations.
    const PRUint32 kMinStringSize = 127;

    if (mData)
    {
	    // Re
	    NS_ASSERTION(mData->mRefCount > 0, "String deleted too many times!");
	    if (mData->mRefCount == 1)
	    {
	        // We are the sole owner, so just change its length, if necessary.
	        if (inLength > mData->mLength && inLength > kMinStringSize)
		        mData = (Data*)PR_Realloc(mData, inLength + sizeof(Data));
	        mData->mLength = inLength;
	        return;
	    }
    }
    PRUint32 allocLength = inLength;
    if (allocLength < kMinStringSize)
        allocLength = kMinStringSize;
	if (allocLength < Length())	// make sure we're not truncating the result.
		allocLength = Length();
    Data* newData = (Data*)PR_Malloc(allocLength + sizeof(Data));
    // If data was already allocated when we get to here, then we are cloning the data
    // from a shared pointer.
    if (mData)
    {
        memcpy(newData, mData, sizeof(Data) + Length());
        mData->mRefCount--; // Say goodbye
    }
    else
        newData->mString[0] = '\0';

    mData = newData;
    mData->mRefCount = 1;
    mData->mLength = inLength;
} // nsSimpleCharString::ReleaseData

//========================================================================================
NS_NAMESPACE nsFileSpecHelpers
//========================================================================================
{
    enum
    {    kMaxFilenameLength = 31                // should work on Macintosh, Unix, and Win32.
    ,    kMaxAltDigitLength    = 5
    ,    kMaxCoreLeafNameLength    = (kMaxFilenameLength - (kMaxAltDigitLength + 1))
    };
#ifndef XP_MAC
    NS_NAMESPACE_PROTOTYPE void Canonify(nsSimpleCharString& ioPath, PRBool inMakeDirs);
    NS_NAMESPACE_PROTOTYPE void MakeAllDirectories(const char* inPath, int mode);
#endif
#ifdef XP_PC
    NS_NAMESPACE_PROTOTYPE void NativeToUnix(nsSimpleCharString& ioPath);
    NS_NAMESPACE_PROTOTYPE void UnixToNative(nsSimpleCharString& ioPath);
#endif
} NS_NAMESPACE_END

//----------------------------------------------------------------------------------------
nsresult ns_file_convert_result(PRInt32 nativeErr)
//----------------------------------------------------------------------------------------
{
    return nativeErr ?
        NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES,((nativeErr)&0xFFFF))
        : NS_OK;
}

//----------------------------------------------------------------------------------------
void nsSimpleCharString::LeafReplace(char inSeparator, const char* inLeafName)
//----------------------------------------------------------------------------------------
{
    // Find the existing leaf name
    if (IsEmpty())
        return;
    if (!inLeafName)
    {
        SetToEmpty();
        return;
    }
    char* chars = mData->mString;
    char* lastSeparator = strrchr(chars, inSeparator);
    int oldLength = Length();
    PRBool trailingSeparator = (lastSeparator + 1 == chars + oldLength);
    if (trailingSeparator)
    {
		char savedCh = *lastSeparator;
		char *savedLastSeparator = lastSeparator;
        *lastSeparator = '\0';
        lastSeparator = strrchr(chars, inSeparator);
		*savedLastSeparator = savedCh;
    }
    if (lastSeparator)
        lastSeparator++; // point at the trailing string
    else
        lastSeparator = chars; // the full monty

	PRUint32 savedLastSeparatorOffset = (lastSeparator - chars);
    int newLength = (lastSeparator - chars) + strlen(inLeafName) + (trailingSeparator != 0);
    ReallocData(newLength);

    chars = mData->mString; // it might have moved.
    chars[savedLastSeparatorOffset] = '\0'; // strip the current leaf name

    strcat(chars, inLeafName);
    if (trailingSeparator)
    {
        // If the original ended in a slash, then the new one should, too.
        char sepStr[2] = "/";
        *sepStr = inSeparator;
        strcat(chars, sepStr);
    }
} // nsSimpleCharString::LeafReplace

//----------------------------------------------------------------------------------------
char* nsSimpleCharString::GetLeaf(char inSeparator) const
// Returns a pointer to an allocated string representing the leaf.
//----------------------------------------------------------------------------------------
{
    if (IsEmpty())
        return nsnull;

    char* chars = mData->mString;
    const char* lastSeparator = strrchr(chars, inSeparator);
    
    // If there was no separator, then return a copy of our path.
    if (!lastSeparator)
        return PL_strdup(*this);

    // So there's at least one separator.  What's just after it?
    // If the separator was not the last character, return the trailing string.
    const char* leafPointer = lastSeparator + 1;
    if (*leafPointer)
        return PL_strdup(leafPointer);

    // So now, separator was the last character. Poke in a null instead.
    *(char*)lastSeparator = '\0'; // Should use const_cast, but Unix has old compiler.
    leafPointer = strrchr(chars, inSeparator);
    char* result = leafPointer ? PL_strdup(++leafPointer) : PL_strdup(chars);
    // Restore the poked null before returning.
    *(char*)lastSeparator = inSeparator;
#ifdef XP_PC
	// If it's a drive letter use the colon notation.
	if (!leafPointer && result[2] == 0 && result[1] == '|')
	    result[1] = ':';
#endif
    return result;
} // nsSimpleCharString::GetLeaf

#if defined(XP_UNIX) || defined(XP_PC)

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::MakeAllDirectories(const char* inPath, int mode)
// Make the path a valid one by creating all the intermediate directories.  Does NOT
// make the leaf into a directory.  This should be a unix path.
//----------------------------------------------------------------------------------------
{
	if (!inPath)
		return;
		
    char* pathCopy = PL_strdup( inPath );
    if (!pathCopy)
    	return;

	const char kSeparator = '/'; // I repeat: this should be a unix-style path.
    const int kSkipFirst = 1;

#ifdef XP_PC
    // Either this is a relative path, or we ensure that it has
    // a drive letter specifier.
    NS_ASSERTION( pathCopy[0] != '/' || pathCopy[2] == '|', "No drive letter!" );
#endif
    char* currentStart = pathCopy;
    char* currentEnd = strchr(currentStart + kSkipFirst, kSeparator);
    if (currentEnd)
    {
        nsFileSpec spec;
		*currentEnd = '\0';
        
#ifdef XP_PC
        /* 
           if we have a drive letter path, we must make sure that the inital path has a '/' on it, or
           Canonify will turn "/c|" into a path relative to the running executable.
        */
        if (pathCopy[0] == '/' && pathCopy[2] == '|')
        {
            char* startDir = (char*)PR_Malloc(strlen(pathCopy) + 2);
            strcpy(startDir, pathCopy);
            strcat(startDir, "/");

            spec = nsFilePath(startDir, PR_FALSE);
		    
            PR_Free(startDir);
        }
        else
        {
            spec = nsFilePath(pathCopy, PR_FALSE);

        }
#else
        spec = nsFilePath(pathCopy, PR_FALSE);
#endif        
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
            
            *currentEnd = '\0';

			spec += currentStart; // "lengthen" the path, adding the next node.
		} while (currentEnd);
    }
	delete [] pathCopy;
} // nsFileSpecHelpers::MakeAllDirectories

#endif // XP_PC || XP_UNIX

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
nsFileURL::nsFileURL(const char* inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
{
    if (!inString)
    	return;
    NS_ASSERTION(strstr(inString, kFileURLPrefix) == inString, "Not a URL!");
    // Make canonical and absolute.
	nsFilePath path(inString + kFileURLPrefixLength, inCreateDirs);
	*this = path;
} // nsFileURL::nsFileURL
#endif

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsString& inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
{
    const nsAutoCString aString(inString);
    const char* aCString = (const char*) aString;
    if (!inString.Length())
    	return;
    NS_ASSERTION(strstr(aCString, kFileURLPrefix) == aCString, "Not a URL!");
    // Make canonical and absolute.
	nsFilePath path(aCString + kFileURLPrefixLength, inCreateDirs);
	*this = path;
} // nsFileURL::nsFileURL
#endif

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
:    mURL(inOther.mURL)
#ifdef XP_MAC
,    mFileSpec(inOther.GetFileSpec())
#endif
{
} // nsFileURL::nsFileURL

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
    *this = inOther;
} // nsFileURL::nsFileURL
#endif

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
    *this = inOther;
} // nsFileURL::nsFileURL
#endif

//----------------------------------------------------------------------------------------
nsFileURL::~nsFileURL()
//----------------------------------------------------------------------------------------
{
}

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
    mURL = inString;
    NS_ASSERTION(strstr(inString, kFileURLPrefix) == inString, "Not a URL!");
} // nsFileURL::operator =
#endif

//----------------------------------------------------------------------------------------
void nsFileURL::operator +=(const char* inRelativeUnixPath)
//----------------------------------------------------------------------------------------
{
    char* escapedPath = nsEscape(inRelativeUnixPath, url_Path);
    mURL += escapedPath;
    delete [] escapedPath;
#ifdef XP_MAC
    mFileSpec += inRelativeUnixPath;
#endif
} // nsFileURL::operator +=

//----------------------------------------------------------------------------------------
nsFileURL nsFileURL::operator +(const char* inRelativeUnixPath) const
//----------------------------------------------------------------------------------------
{
   nsFileURL result(*this);
   result += inRelativeUnixPath;
   return result;
}  // nsFileURL::operator +

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
    mURL = inOther.mURL;
#ifdef XP_MAC
    mFileSpec = inOther.GetFileSpec();
#endif
} // nsFileURL::operator =

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
    mURL = kFileURLPrefix;
	char* original = (char*)(const char*)inOther; // we shall modify, but restore.
#ifdef XP_PC
	// because we don't want to escape the '|' character, change it to a letter.
    NS_ASSERTION(original[2] == '|', "No drive letter part!");
    original[2] = 'x';
    char* escapedPath = nsEscape(original, url_Path);
    original[2] = '|'; // restore it
    escapedPath[2] = '|';
#else
    char* escapedPath = nsEscape(original, url_Path);
#endif
    if (escapedPath)
	    mURL += escapedPath;
    delete [] escapedPath;
} // nsFileURL::operator =
#endif

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
    *this = nsFilePath(inOther);
	if (mURL[mURL.Length() - 1] != '/' && inOther.IsDirectory())
	    mURL += "/";
} // nsFileURL::operator =
#endif

//----------------------------------------------------------------------------------------
nsOutputStream& operator << (nsOutputStream& s, const nsFileURL& url)
//----------------------------------------------------------------------------------------
{
    return (s << url.mURL);
}

//========================================================================================
//                                nsFilePath implementation
//========================================================================================

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
    : mPath(inPath.mPath)
#ifdef XP_MAC
    , mFileSpec(inPath.mFileSpec)
#endif
{
}

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const char* inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mPath(inString)
{
    NS_ASSERTION(strstr(inString, kFileURLPrefix) != inString, "URL passed as path");

#ifdef XP_PC
    nsFileSpecHelpers::UnixToNative(mPath);
#endif
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, inCreateDirs);
#ifdef XP_PC
    NS_ASSERTION( mPath[1] == ':', "unexpected canonical path" );
    nsFileSpecHelpers::NativeToUnix(mPath);
#endif
}
#endif

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsString& inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mPath(inString)
{
    NS_ASSERTION(strstr((const char*)mPath, kFileURLPrefix) != (const char*)mPath, "URL passed as path");

#ifdef XP_PC
    nsFileSpecHelpers::UnixToNative(mPath);
#endif
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, inCreateDirs);
#ifdef XP_PC
    NS_ASSERTION( mPath[1] == ':', "unexpected canonical path" );
    nsFileSpecHelpers::NativeToUnix(mPath);
#endif
}
#endif

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
    mPath = (const char*)inOther.mURL + kFileURLPrefixLength;
    mPath.Unescape();
}
#endif

#ifdef XP_UNIX
//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFileSpec& inOther)
//----------------------------------------------------------------------------------------
:    mPath(inOther.mPath)
{
}
#endif // XP_UNIX

//----------------------------------------------------------------------------------------
nsFilePath::~nsFilePath()
//----------------------------------------------------------------------------------------
{
}

#ifdef XP_UNIX
//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
    mPath = inOther.mPath;
}
#endif // XP_UNIX

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
    NS_ASSERTION(strstr(inString, kFileURLPrefix) != inString, "URL passed as path");
#ifdef XP_MAC
    mFileSpec = inString;
	mPath = (const char*)nsFilePath(mFileSpec);
#else
	mPath = inString;
#ifdef XP_PC
	nsFileSpecHelpers::UnixToNative(mPath);
#endif
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, PR_FALSE /* XXX? */);
#ifdef XP_PC
	nsFileSpecHelpers::NativeToUnix(mPath);
#endif
#endif // XP_MAC
}

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
    mPath = (const char*)nsFilePath(inOther);
}
#endif

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
    mPath = inOther.mPath;
#ifdef XP_MAC
    mFileSpec = inOther.GetFileSpec();
#endif
}

//----------------------------------------------------------------------------------------
void nsFilePath::operator +=(const char* inRelativeUnixPath)
//----------------------------------------------------------------------------------------
{
    char* escapedPath = nsEscape(inRelativeUnixPath, url_Path);
    mPath += escapedPath;
    delete [] escapedPath;
#ifdef XP_MAC
    mFileSpec += inRelativeUnixPath;
#endif
} // nsFilePath::operator +=

//----------------------------------------------------------------------------------------
nsFilePath nsFilePath::operator +(const char* inRelativeUnixPath) const
//----------------------------------------------------------------------------------------
{
   nsFilePath result(*this);
   result += inRelativeUnixPath;
   return result;
}  // nsFilePath::operator +

//========================================================================================
//                                nsFileSpec implementation
//========================================================================================

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec()
//----------------------------------------------------------------------------------------
:    mError(NS_OK)
{
}
#endif

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsPersistentFileDescriptor& inDescriptor)
//----------------------------------------------------------------------------------------
{
	*this = inDescriptor;
}

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsFileURL& inURL)
//----------------------------------------------------------------------------------------
{
    *this = nsFilePath(inURL); // convert to unix path first
}

//----------------------------------------------------------------------------------------
void nsFileSpec::MakeUnique(const char* inSuggestedLeafName)
//----------------------------------------------------------------------------------------
{
    if (inSuggestedLeafName && *inSuggestedLeafName)
        SetLeafName(inSuggestedLeafName);

    MakeUnique();
} // nsFileSpec::MakeUnique

//----------------------------------------------------------------------------------------
void nsFileSpec::MakeUnique()
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
        suffix = PL_strdup(lastDot); // include '.'
        *lastDot = '\0'; // strip suffix and dot.
    }
    const int kMaxRootLength
        = nsFileSpecHelpers::kMaxCoreLeafNameLength - strlen(suffix) - 1;
    if ((int)strlen(leafName) > (int)kMaxRootLength)
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
} // nsFileSpec::MakeUnique

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsFileURL& inURL)
//----------------------------------------------------------------------------------------
{
    *this = nsFilePath(inURL); // convert to unix path first
}

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsPersistentFileDescriptor& inDescriptor)
//----------------------------------------------------------------------------------------
{

	nsSimpleCharString data;
	PRInt32 dataSize;
    inDescriptor.GetData(data, dataSize);
    
#ifdef XP_MAC
    char* decodedData = PL_Base64Decode((const char*)data, (int)dataSize, nsnull);
    // Cast to an alias record and resolve.
	AliasHandle aliasH = nsnull;
	mError = NS_FILE_RESULT(PtrToHand(decodedData, &(Handle)aliasH, (dataSize * 3) / 4));
	PR_Free(decodedData);
	if (NS_SUCCEEDED(mError))
		return; // not enough memory?

	Boolean changed;
	mError = NS_FILE_RESULT(::ResolveAlias(nsnull, aliasH, &mSpec, &changed));
	DisposeHandle((Handle) aliasH);
	mPath.SetToEmpty();
#else
    mPath = data;
    mError = NS_OK;
#endif
}

//========================================================================================
//                                UNIX & WIN nsFileSpec implementation
//========================================================================================

#ifdef XP_UNIX
//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
:    mPath((const char*)inPath)
,    mError(NS_OK)
{
}
#endif // XP_UNIX

#ifdef XP_UNIX
//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
    mPath = (const char*)inPath;
    mError = NS_OK;
}
#endif //XP_UNIX

#if defined(XP_UNIX) || defined(XP_PC)
//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
:    mPath(inSpec.mPath)
,    mError(NS_OK)
{
}
#endif //XP_UNIX

#if defined(XP_UNIX) || defined(XP_PC)
//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const char* inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mPath(inString)
,    mError(NS_OK)
{
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, inCreateDirs);
}
#endif //XP_UNIX,PC

#if defined(XP_UNIX) || defined(XP_PC)
//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsString& inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mPath(inString)
,    mError(NS_OK)
{
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, inCreateDirs);
}
#endif //XP_UNIX,PC

//----------------------------------------------------------------------------------------
nsFileSpec::~nsFileSpec()
//----------------------------------------------------------------------------------------
{
}

#if defined(XP_UNIX) || defined(XP_PC)
//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
    mPath = inSpec.mPath;
    mError = inSpec.Error();
}
#endif //XP_UNIX


#if defined(XP_UNIX) || defined(XP_PC)
//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
    mPath = inString;
    // Make canonical and absolute.
    nsFileSpecHelpers::Canonify(mPath, PR_FALSE /* XXX? */);
    mError = NS_OK;
}
#endif //XP_UNIX

#if (defined(XP_UNIX) || defined(XP_PC))
//----------------------------------------------------------------------------------------
nsOutputStream& operator << (nsOutputStream& s, const nsFileSpec& spec)
//----------------------------------------------------------------------------------------
{
#ifdef NS_DEBUG
    static PRBool warnedOnce = PR_FALSE;
    if (!warnedOnce)
    {
        NS_WARNING("This is for debugging only.  Do not call this in shipped version!");
        warnedOnce = PR_TRUE;
    }
#endif // NS_DEBUG
    return (s << spec.GetCString());
}
#endif // DEBUG ONLY!

//----------------------------------------------------------------------------------------
nsFileSpec nsFileSpec::operator + (const char* inRelativePath) const
//----------------------------------------------------------------------------------------
{
    nsFileSpec result = *this;
    result += inRelativePath;
    return result;
} // nsFileSpec::operator +

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::operator == (const nsFileSpec& inOther) const
//----------------------------------------------------------------------------------------
{

#ifdef XP_MAC
    if ( inOther.mSpec.vRefNum == mSpec.vRefNum &&
        inOther.mSpec.parID   == mSpec.parID &&
        EqualString(inOther.mSpec.name, mSpec.name, false, true))
        return PR_TRUE;
#else
    PRBool amEmpty = mPath.IsEmpty();
    PRBool heEmpty = inOther.mPath.IsEmpty();
    if (amEmpty) // we're the same if he's empty...
        return heEmpty;
    if (heEmpty) // ('cuz I'm not...)
        return PR_FALSE;
	#if defined(XP_PC)
	   // windows does not care about case.
	   if (_stricmp(mPath, inOther.mPath ) == 0)
	       return PR_TRUE;
	#else
	   if (strcmp(mPath, inOther.mPath ) == 0)
	       return PR_TRUE;
	#endif
#endif
   return PR_FALSE;
}

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::operator != (const nsFileSpec& inOther) const
//----------------------------------------------------------------------------------------
{
    return (! (*this == inOther) );
}

#ifndef XP_MAC
//----------------------------------------------------------------------------------------
const char* nsFileSpec::GetCString() const
// This is the only automatic conversion to const char*
// that is provided, and it allows the
// path to be "passed" to NSPR file routines.  This practice
// is VERY EVIL and should only be used to support legacy
// code.  Using it guarantees bugs on Macintosh. The path is NOT allocated, so do
// not even think of deleting (or freeing) it.
//----------------------------------------------------------------------------------------
{
    return mPath;
}
#endif

//========================================================================================
//	class nsPersistentFileDescriptor
//========================================================================================

//----------------------------------------------------------------------------------------
nsPersistentFileDescriptor::nsPersistentFileDescriptor(const nsPersistentFileDescriptor& inDesc)
//----------------------------------------------------------------------------------------
    : mDescriptorString(inDesc.mDescriptorString)
{
} // nsPersistentFileDescriptor::nsPersistentFileDescriptor

//----------------------------------------------------------------------------------------
void nsPersistentFileDescriptor::operator = (const nsPersistentFileDescriptor& inDesc)
//----------------------------------------------------------------------------------------
{
    mDescriptorString = inDesc.mDescriptorString;
} // nsPersistentFileDescriptor::operator =

//----------------------------------------------------------------------------------------
nsPersistentFileDescriptor::nsPersistentFileDescriptor(const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	*this = inSpec;
} // nsPersistentFileDescriptor::nsPersistentFileDescriptor

//----------------------------------------------------------------------------------------
void nsPersistentFileDescriptor::operator = (const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
#ifdef XP_MAC
    if (inSpec.Error())
    	return;
	AliasHandle	aliasH;
	OSErr err = NewAlias(nil, inSpec.GetFSSpecPtr(), &aliasH);
	if (err != noErr)
		return;

	PRUint32 bytes = GetHandleSize((Handle) aliasH);
	HLock((Handle) aliasH);
	char* buf = PL_Base64Encode((const char*)*aliasH, bytes, nsnull);
	DisposeHandle((Handle) aliasH);

    mDescriptorString = buf;
    PR_Free(buf);
#else
    mDescriptorString = inSpec.GetCString();
#endif // XP_MAC
} // nsPersistentFileDescriptor::operator =

//----------------------------------------------------------------------------------------
nsPersistentFileDescriptor::~nsPersistentFileDescriptor()
//----------------------------------------------------------------------------------------
{
} // nsPersistentFileDescriptor::~nsPersistentFileDescriptor

//----------------------------------------------------------------------------------------
void nsPersistentFileDescriptor::GetData(nsSimpleCharString& outData, PRInt32& outSize) const
//----------------------------------------------------------------------------------------
{
    outSize = mDescriptorString.Length();
    outData = mDescriptorString;
}

//----------------------------------------------------------------------------------------
void nsPersistentFileDescriptor::SetData(const nsSimpleCharString& inData, PRInt32 inSize)
//----------------------------------------------------------------------------------------
{
	mDescriptorString.CopyFrom((const char*)inData, inSize);
}

#define MAX_PERSISTENT_DATA_SIZE 1000

//----------------------------------------------------------------------------------------
nsresult nsPersistentFileDescriptor::Read(nsIInputStream* aStream)
//----------------------------------------------------------------------------------------
{
    nsInputStream inputStream(aStream);
    inputStream >> *this;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
nsresult nsPersistentFileDescriptor::Write(nsIOutputStream* aStream)
//----------------------------------------------------------------------------------------
{
    nsOutputStream outputStream(aStream);
    outputStream << *this;
    return NS_OK;
}

//----------------------------------------------------------------------------------------
nsInputStream& operator >> (nsInputStream& s, nsPersistentFileDescriptor& d)
// reads the data from a file
//----------------------------------------------------------------------------------------
{
	char bigBuffer[MAX_PERSISTENT_DATA_SIZE + 1];
	// The first 8 bytes of the data should be a hex version of the data size to follow.
	PRInt32 bytesRead = 8;
	bytesRead = s.read(bigBuffer, bytesRead);
	if (bytesRead != 8)
		return s;
	bigBuffer[8] = '\0';
	sscanf(bigBuffer, "%x", (PRUint32*)&bytesRead);
	if (bytesRead > MAX_PERSISTENT_DATA_SIZE)
		return s; // preposterous.
	// Now we know how many bytes to read, do it.
	s.read(bigBuffer, bytesRead);
	d.SetData(bigBuffer, bytesRead);
	return s;
}

//----------------------------------------------------------------------------------------
nsOutputStream& operator << (nsOutputStream& s, const nsPersistentFileDescriptor& d)
// writes the data to a file
//----------------------------------------------------------------------------------------
{
	char littleBuf[9];
	PRInt32 dataSize;
	nsSimpleCharString data;
	d.GetData(data, dataSize);
	// First write (in hex) the length of the data to follow.  Exactly 8 bytes
	sprintf(littleBuf, "%0.8x", dataSize);
	s << littleBuf;
	// Now write the data itself
	s << (const char*)data;
	return s;
}

//========================================================================================
//	class nsAutoCString
//========================================================================================

//----------------------------------------------------------------------------------------
nsAutoCString::~nsAutoCString()
//----------------------------------------------------------------------------------------
{
	delete [] (char*)mCString;
}
