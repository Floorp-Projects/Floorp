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

/* xp_file_mac.h
 * Mac-only xp_file functions
 */

#pragma once
 
#include "client.h"
#include "xp_file.h"
 
XP_BEGIN_PROTOS
char* xp_FileName( const char* name, XP_FileType type, char* buf, char* configBuf );
char * xp_FilePlatformName(const char * name, char* path);
char * xp_TempName(XP_FileType type, const char * prefix, char* buf, char* buf2, unsigned int *count);

/* general Name->FSSpec conversion */
OSErr XP_FileSpec(const char *name, XP_FileType type, FSSpec* spec);

int xp_MacToUnixErr( OSErr err );

/* This function is only implemented on the mac (because it can only be
   implemented efficiently there.)  Implementing on the other platforms
   to just return some magic value would lead to weird, rare bugs in the
   cache cleanup code; so simply don't call this on the other platforms.
 */
extern int XP_FileNumberOfFilesInDirectory(const char * dir_name,
										   XP_FileType type);

int XP_FileDuplicateResourceFork( const char* oldFilePath, XP_FileType oldType,
									const char* newFilePath, XP_FileType newType );
XP_END_PROTOS