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
 
//	First checked in on 98/12/08 by John R. McMullen.
//  Since nsFileStream.h is entirely templates, common code (such as open())
//  which does not actually depend on the charT, can be placed here.

#include "nsFileStream.h"

#include <string.h>
#include <stdio.h>

#ifdef XP_MAC
#include <Errors.h>
#endif

//========================================================================================
//          nsBasicFileStream
//========================================================================================

//----------------------------------------------------------------------------------------
nsBasicFileStream::nsBasicFileStream()
//----------------------------------------------------------------------------------------
    :    mFileDesc(0)
    ,    mNSPRMode(0)
    ,    mFailed(false)
    ,    mEOF(false)
{
}

//----------------------------------------------------------------------------------------
nsBasicFileStream::nsBasicFileStream(
    const nsFilePath& inFile, 
    int nsprMode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
    :    mFileDesc(0)
    ,    mNSPRMode(0)
    ,    mFailed(false)
    ,    mEOF(false)
{
    open(inFile, nsprMode, accessMode);
}

//----------------------------------------------------------------------------------------
nsBasicFileStream::nsBasicFileStream(PRFileDesc* desc, int nsprMode)
//----------------------------------------------------------------------------------------
    :    mFileDesc(desc)
    ,    mNSPRMode(nsprMode)
    ,    mFailed(false)
    ,    mEOF(false)
{
}

//----------------------------------------------------------------------------------------
nsBasicFileStream::~nsBasicFileStream()
//----------------------------------------------------------------------------------------
{
	close();
}

//----------------------------------------------------------------------------------------
void nsBasicFileStream::open(
	const nsFilePath& inFile,
    int nsprMode,
    PRIntn accessMode)
//----------------------------------------------------------------------------------------
{
    if (mFileDesc)
        return;
        
    const int nspr_modes[]={
        PR_WRONLY | PR_CREATE_FILE,
        PR_WRONLY | PR_CREATE_FILE | PR_APPEND,
        PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
        PR_RDONLY,
        PR_RDONLY | PR_APPEND,
        PR_RDWR | PR_CREATE_FILE,
        PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE,
//      "wb",
//      "ab", 
//      "wb",
//      "rb",
//      "r+b",
//      "w+b",
        0 };
    const int* currentLegalMode = nspr_modes;
    while (*currentLegalMode && nsprMode != *currentLegalMode)
        ++currentLegalMode;
    if (!*currentLegalMode) 
        return;

#ifdef XP_MAC
     // Use the file spec to open the file, because one path can be common to
     // several files on the Macintosh (you can have several volumes with the
     // same name, see).
    mFileDesc = 0;
    if (inFile.GetNativeSpec().Error() != noErr)
        return;
    OSErr err = noErr;
#if DEBUG
	const OSType kCreator = 'CWIE';
#else
    const OSType kCreator = 'MOSS';
#endif
	nsNativeFileSpec nativeSpec = inFile.GetNativeSpec();
    FSSpec* spec = (FSSpec*)nativeSpec;
    if (nsprMode & PR_CREATE_FILE)
    	err = FSpCreate(spec, kCreator, 'TEXT', 0);
    if (err == dupFNErr)
    	err = noErr;
    if (err != noErr)
       return;
    
    SInt8 perm;
    if (nsprMode & PR_RDWR)
       perm = fsRdWrPerm;
    else if (nsprMode & PR_WRONLY)
       perm = fsWrPerm;
    else
       perm = fsRdPerm;

    short refnum;
    err = FSpOpenDF(spec, perm, &refnum);

    if (err == noErr && (nsprMode & PR_TRUNCATE))
    	err = SetEOF(refnum, 0);
    if (err == noErr && (nsprMode & PR_APPEND))
    	err = SetFPos(refnum, fsFromLEOF, 0);
    if (err != noErr)
       return;

    if ((mFileDesc = PR_ImportFile(refnum)) == 0)
    	return;
#else
	//	Platforms other than Macintosh...
    if ((mFileDesc = PR_Open(inFile, nsprMode, accessMode)) == 0)
    	return;
#endif
     mNSPRMode = nsprMode;
} // nsFileStreamHelpers::open

//----------------------------------------------------------------------------------------
void nsBasicFileStream::close()
// Must precede the destructor because both are inline.
//----------------------------------------------------------------------------------------
{
    if (mFileDesc==PR_STDIN || mFileDesc==PR_STDOUT || mFileDesc==PR_STDERR || mFileDesc == 0) 
       return;
    if (PR_Close(mFileDesc) == PR_SUCCESS)
    	mFileDesc = 0;
} // nsBasicFileStream::close

//----------------------------------------------------------------------------------------
void nsBasicFileStream::seek(PRSeekWhence whence, PRInt32 offset)
// Must precede the destructor because both are inline.
//----------------------------------------------------------------------------------------
{
    if (mFileDesc==PR_STDIN || mFileDesc==PR_STDOUT || mFileDesc==PR_STDERR || mFileDesc == 0) 
       return;
    mFailed = false; // reset on a seek.
    mEOF = false; // reset on a seek.
    PRInt32 position = PR_Seek(mFileDesc, 0, PR_SEEK_CUR);
    PRInt32 available = PR_Available(mFileDesc);
    PRInt32 fileSize = position + available;
    PRInt32 newPosition;
    switch (whence)
    {
    	case PR_SEEK_CUR: newPosition = position + offset; break;
    	case PR_SEEK_SET: newPosition = offset; break;
    	case PR_SEEK_END: newPosition = fileSize + offset; break;
    }
    if (newPosition < 0)
    {
    	newPosition = 0;
    	mFailed = true;
    }
    else if (newPosition >= fileSize)
    {
    	newPosition = fileSize;
    	mEOF = true;
    }
    if (PR_Seek(mFileDesc, newPosition, PR_SEEK_SET) < 0)
    	mFailed = true;
} // nsBasicFileStream::seek

//----------------------------------------------------------------------------------------
PRIntn nsBasicFileStream::tell() const
// Must precede the destructor because both are inline.
//----------------------------------------------------------------------------------------
{
    if (mFileDesc==PR_STDIN || mFileDesc==PR_STDOUT || mFileDesc==PR_STDERR || mFileDesc == 0) 
       return -1;
    return PR_Seek(mFileDesc, 0, PR_SEEK_CUR);
} // nsBasicFileStream::tell

//========================================================================================
//          nsInputFileStream
//========================================================================================

//----------------------------------------------------------------------------------------
nsInputFileStream::nsInputFileStream(istream* stream)
//----------------------------------------------------------------------------------------
#ifndef NS_USE_PR_STDIO
    : nsBasicFileStream(0, kDefaultMode)
    , mStdStream(stream)
#else
    : nsBasicFileStream(PR_STDIN, kDefaultMode)
    , mStdStream(0)
#endif
{
}

//----------------------------------------------------------------------------------------
void nsInputFileStream::get(char& c)
//----------------------------------------------------------------------------------------
{
    read(&c, sizeof(char));
}

//----------------------------------------------------------------------------------------
bool nsInputFileStream::readline(char* s, PRInt32 n)
// This will truncate if the buffer is too small.  Result will always be null-terminated.
//----------------------------------------------------------------------------------------
{
    bool bufferLargeEnough = true; // result
    if (!s || !n)
        return true;
    PRIntn position = tell();
    if (position < 0)
        return false;
    PRInt32 bytesRead = read(s, n - 1);
    if (mFailed)
        return false;
    s[bytesRead] = '\0'; // always terminate at the end of the buffer
    char* tp = strpbrk(s, "\n\r");
    if (tp)
    {
        char ch = *tp;
        *tp++ = '\0'; // terminate at the newline, then skip past it
        if ((ch == '\n' && *tp == '\r') || (ch == '\r' && *tp == '\n'))
            tp++; // possibly a pair.
        bytesRead = (tp - s);
    }
    else if (!eof())
        bufferLargeEnough = false;
    position += bytesRead;
    seek(position);
    return bufferLargeEnough;
} // nsInputFileStream::getline

//----------------------------------------------------------------------------------------
PRInt32 nsInputFileStream::read(void* s, PRInt32 n)
//----------------------------------------------------------------------------------------
{
#ifndef NS_USE_PR_STDIO
    // Calling PR_Read on stdin is sure suicide on Macintosh.
    if (mStdStream)
    {
    	mStdStream->read((char*)s, n);
    	return n;
    }
#endif
    if (!mFileDesc || mFailed)
        return -1;
    PRInt32 bytesRead = PR_Read(mFileDesc, s, n);
    if (bytesRead < 0)
        mFailed = true;
    else if (bytesRead < n)
        mEOF = true;
    return bytesRead;
}

//----------------------------------------------------------------------------------------
nsInputFileStream& nsInputFileStream::operator >> (char& c)
//----------------------------------------------------------------------------------------
{
	get(c);
	return *this;
}

//========================================================================================
//          nsOutputFileStream
//========================================================================================

//----------------------------------------------------------------------------------------
nsOutputFileStream::nsOutputFileStream(ostream* stream)
//----------------------------------------------------------------------------------------
#ifndef NS_USE_PR_STDIO
    : nsBasicFileStream(0, kDefaultMode)
    , mStdStream(stream)
#else
    : nsBasicFileStream(PR_STDOUT, kDefaultMode)
    , mStdStream(0)
#endif
{
}

//----------------------------------------------------------------------------------------
void nsOutputFileStream::put(char c)
//----------------------------------------------------------------------------------------
{
    write(&c, sizeof(c));
}

//----------------------------------------------------------------------------------------
PRInt32 nsOutputFileStream::write(const void* s, PRInt32 n)
//----------------------------------------------------------------------------------------
{
#ifndef NS_USE_PR_STDIO
    // Calling PR_Write on stdout is sure suicide.
    if (mStdStream)
    {
    	mStdStream->write((const char*)s, n);
        return n;
    }
#endif
    if (!mFileDesc || mFailed)
       return -1;
    PRInt32 bytesWrit = PR_Write(mFileDesc, s, n);
    if (bytesWrit != n)
        mFailed = true;
    return bytesWrit;
}

//----------------------------------------------------------------------------------------
nsOutputFileStream& nsOutputFileStream::operator << (char c)
//----------------------------------------------------------------------------------------
{
	put(c);
	return *this;
}

//----------------------------------------------------------------------------------------
nsOutputFileStream& nsOutputFileStream::operator << (const char* s)
//----------------------------------------------------------------------------------------
{
	write(s, strlen(s));
	return *this;
}

//----------------------------------------------------------------------------------------
nsOutputFileStream& nsOutputFileStream::operator << (short val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%d", val);
	return *this << buf;
}

//----------------------------------------------------------------------------------------
nsOutputFileStream& nsOutputFileStream::operator << (unsigned short val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%ud", val);
	return *this << buf;
}

//----------------------------------------------------------------------------------------
nsOutputFileStream& nsOutputFileStream::operator << (long val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%ld", val);
	return *this << buf;
}

//----------------------------------------------------------------------------------------
nsOutputFileStream& nsOutputFileStream::operator << (unsigned long val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%uld", val);
	return *this << buf;
}

//----------------------------------------------------------------------------------------
void nsOutputFileStream::flush()
// Must precede the destructor because both are inline.
//----------------------------------------------------------------------------------------
{
#ifndef NS_USE_PR_STDIO
    if (mStdStream)
    {
        mStdStream->flush();
        return;
    }
#endif
    if (mFileDesc == 0) 
        return;
    bool itFailed = PR_Sync(mFileDesc) != PR_SUCCESS;
#ifdef XP_MAC
    // On unix, it seems to fail always.
    if (itFailed)
    	mFailed = true;
#endif
} // nsOutputFileStream::flush

//========================================================================================
//        Manipulators
//========================================================================================

//----------------------------------------------------------------------------------------
nsOutputFileStream& nsEndl(nsOutputFileStream& os)
//----------------------------------------------------------------------------------------
{
#ifndef NS_USE_PR_STDIO
    // Calling PR_Write on stdout is sure suicide on Macintosh.
    ostream* stream = os.GetStandardStream();
    if (stream)
    {
        *stream << std::endl;
        return os;
    }
#endif
     os.put('\n');
     os.flush();
     return os;
}
