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
#ifndef nsIFileStream_h___
#define nsIFileStream_h___

#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "prio.h"

class nsFileSpec;

/* a6cf90e8-15b3-11d2-932e-00805f8add32 */
#define NS_IFILE_IID \
{ 0xa6cf90e8, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

//========================================================================================
class nsIFile
// Represents a file, and supports Open.
//========================================================================================
: public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IFILE_IID; return iid; }
	NS_IMETHOD                         Open(
                                           const nsFileSpec& inFile,
                                           int nsprMode,
                                           PRIntn accessMode) = 0;
                                           // Note: Open() is only needed after
                                           // an explicit Close().  All file streams
                                           // are automatically opened on construction.
    NS_IMETHOD                         GetIsOpen(PRBool* outOpen) = 0;

}; // class nsIFile

/* a6cf90e8-15b3-11d2-932e-00805f8add32 */
#define NS_IRANDOMACCESS_IID \
{  0xa6cf90eb, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

//========================================================================================
class nsIRandomAccessStore
// Supports Seek, Tell etc.
//========================================================================================
: public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IRANDOMACCESS_IID; return iid; }
    NS_IMETHOD                         Seek(PRSeekWhence whence, PRInt32 offset) = 0;
    NS_IMETHOD                         Tell(PRIntn* outWhere) = 0;

/* "PROTECTED" */
    NS_IMETHOD                         GetAtEOF(PRBool* outAtEOF) = 0;
    NS_IMETHOD                         SetAtEOF(PRBool inAtEOF) = 0;
}; // class nsIRandomAccessStore

/* a6cf90e6-15b3-11d2-932e-00805f8add32 */
#define NS_IFILEINPUTSTREAM_IID \
{ 0xa6cf90e6, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }
    
//========================================================================================
class nsIFileInputStream
// These are additional file-specific methods that files have, above what
// nsIInputStream supports.  The current implementation supports both
// interfaces.
//========================================================================================
: public nsIInputStream
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IFILEINPUTSTREAM_IID; return iid; }
}; // class nsIFileInputStream

/* a6cf90e7-15b3-11d2-932e-00805f8add32 */
#define NS_IFILEOUTPUTSTREAM_IID \
{ 0xa6cf90e7, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

//========================================================================================
class nsIFileOutputStream
// These are additional file-specific methods that files have, above what
// nsIOutputStream supports.  The current implementation supports both
// interfaces.
//========================================================================================
: public nsIOutputStream
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IFILEOUTPUTSTREAM_IID; return iid; }
}; // class nsIFileOutputStream

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewTypicalInputFileStream(
    nsISupports** aStreamResult,
    const nsFileSpec& inFile
    /*Default nsprMode == PR_RDONLY*/
    /*Default accessmode = 0700 (octal)*/);
// Factory method to get an nsInputStream from a file, using most common options

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewOutputConsoleStream(
    nsISupports** aStreamResult);
    // Factory method to get an nsOutputStream to the console.

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewTypicalOutputFileStream(
    nsISupports** aStreamResult, // will implement all the above interfaces
    const nsFileSpec& inFile
    /*default nsprMode= (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE)*/
    /*Default accessMode= 0700 (octal)*/);
    // Factory method to get an nsOutputStream to a file - most common case.

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewTypicalIOFileStream(
    nsISupports** aStreamResult, // will implement all the above interfaces
    const nsFileSpec& inFile
    /*default nsprMode = (PR_RDWR | PR_CREATE_FILE)*/
    /*Default accessMode = 0700 (octal)*/);
    // Factory method to get an object that implements both nsIInputStream
    // and nsIOutputStream, associated with a single file.

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewIOFileStream(
    nsISupports** aStreamResult, // will implement all the above interfaces
    const nsFileSpec& inFile,
    PRInt32 nsprMode,
    PRInt32 accessMode);
    // Factory method to get an object that implements both nsIInputStream
    // and nsIOutputStream, associated with a single file.

#endif /* nsIFileStream_h___ */
