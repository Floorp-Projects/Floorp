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
#ifndef __nsINetFile_h
#define __nsINetFile_h
#include "nsISupports.h"
#include "nsString.h"
#include "prio.h"

// These defines are for temporary use until we get a real dir manager.
#define USER_DIR_TOK "%USER%"
#define CACHE_DIR_TOK "%CACHE_D%"
#define DEF_DIR_TOK "%DEF_D%"

#define COOKIE_FILE_TOK "%COOKIE_F%"
#ifdef XP_PC
#define COOKIE_FILE "cookies.txt"
#else
#define COOKIE_FILE "cookies"
#endif

#ifdef CookieManagement
#define COOKIE_PERMISSION_FILE_TOK "%COOKIE_PERMISSION_F%"
#ifdef XP_PC
#define COOKIE_PERMISSION_FILE "cookperm.txt"
#else
#define COOKIE_PERMISSION_FILE "cookperm"
#endif
#endif

#ifdef SingleSignon
#define SIGNON_FILE_TOK "%SIGNON_F%"
#ifdef XP_PC
#define SIGNON_FILE "signons.txt"
#else
#define SIGNON_FILE "signons"
#endif
#endif

#define CACHE_DB_F_TOK "%CACHE_DB_F%"
#define CACHE_DB_FILE "fat.db"

// {9E04ADC2-2B1D-11d2-B6EB-00805F8A2676}
#define NS_INETFILE_IID   \
{ 0x9e04adc2, 0x2b1d, 0x11d2, \
    {0xb6, 0xeb, 0x00, 0x80, 0x5f, 0x8a, 0x26, 0x76} }

// Represents a file for use with nsINetFile routines.
typedef struct _nsFile {
    PRFileDesc *fd;
} nsFile;

// Represents a directory for use with nsINetFile routines.
typedef struct _nsDir {
    PRDir *dir;
} nsDir;

// Open flags. Used with nsINetFile::OpenFile.
typedef enum {
    nsRead = 1,
    nsWrite = 2,
    nsReadWrite = 3,
    nsReadBinary = 5,
    nsWriteBinary = 6,
    nsReadWriteBinary = 7,
    nsOverWrite = 8
} nsFileMode;

class nsINetFile: public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_INETFILE_IID; return iid; }

    /*
     * File methods
     */

    // Convert a generic file names into a platform file path
    // Currently generic file location information is prepended to the 
    // begining of aName. For example, if I wanted to open the cookie file,
    // I'd pass in "%USER%\cookies.txt . aName is platform specific.
    NS_IMETHOD GetFilePath(const char *aName, char **aRes) = 0;
    NS_IMETHOD GetTemporaryFilePath(const char *aName, char **aRes) = 0;
    NS_IMETHOD GetUniqueFilePath(const char *aName, char **aRes) = 0;
    NS_IMETHOD GetCacheFileName(char *aDirTok, char **aRes) = 0;

    // Open a file
    NS_IMETHOD OpenFile(const char *aPath, nsFileMode aMode,
                        nsFile** aRes) = 0;
    
    // Close a file
    NS_IMETHOD CloseFile(nsFile* aFile) = 0;

    // Read a file
    NS_IMETHOD FileRead(nsFile *aFile, char **aBuf, 
                        PRInt32 *aBuflen,
                        PRInt32 *aBytesRead) = 0;

    NS_IMETHOD FileReadLine(nsFile *aFile, char **aBuf,
                        PRInt32 *aBuflen,
                        PRInt32 *aBytesRead) = 0;

    // Write a file
    NS_IMETHOD FileWrite(nsFile *aFile, const char *aBuf, 
                         PRInt32 *aLen,
                         PRInt32 *aBytesWritten) = 0;

    // Sync a file with disk
    NS_IMETHOD FileSync(nsFile *aFile) = 0;

    // Remove a file
    NS_IMETHOD FileRemove(const char *aPath) = 0;

    // Rename a file
    NS_IMETHOD FileRename(const char *aPathOld, const char *aPathNew) = 0;

    /*
     * Directory Methods
     */

    // Open a directory
    NS_IMETHOD OpenDir(const char *aPath, nsDir** aRes) = 0;

    // Close a directory
    NS_IMETHOD CloseDir(nsDir *aDir) = 0;

    // Create a directory
    NS_IMETHOD CreateDir(const char *aPath, PRBool aRecurse) = 0;

    // Assocaite a token with a directory.
    NS_IMETHOD SetDirectory(const char *aToken, const char *aDir) = 0;

    // Associate a filename with a token, and optionally a dir token.
    NS_IMETHOD SetFileAssoc(const char *aToken, const char *aFile, const char *aDirToken) = 0;

};

/**
 * Create an instance of the INetFile
 *
 */
extern "C" NS_NET nsresult NS_NewINetFile(nsINetFile** aInstancePtrResult,
                                             nsISupports* aOuter);


extern "C" NS_NET nsresult NS_InitINetFile(void);

extern "C" NS_NET nsresult NS_ShutdownINetFile();

#endif /*  __nsINetFile_h */
