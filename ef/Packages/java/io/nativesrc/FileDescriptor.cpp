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
#include <string.h>


#include "java_io_FileDescriptor.h"
#include "prio.h"
#include "prprf.h"

#include "Mapping.h"

/* This is used as a global file-descriptor mapper */

Mapping fdMapping;

extern "C" {

//#ifdef LINUX
#if 0

// 
// This is temporary. I have to find a way to get the static 
// initializers to work !!!!
// 
//  Sorry, Laurent.
//
void _init()
{
	fdMapping.init();
#if 0
	fdMapping.descs[1] = PR_Open("/tmp/stdout", PR_WRONLY | PR_CREATE_FILE | PR_APPEND, 0644);
	fdMapping.descs[2] = fdMapping.descs[1];
#endif
}
#endif

/*
 * Class : java/io/FileDescriptor
 * Method : sync
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_FileDescriptor_sync(Java_java_io_FileDescriptor *fileD)
{
    PRFileDesc *desc = fdMapping.get(fileD->fd);
  
    if (desc) PR_Sync(desc);
}

/*
 * Class : java/io/FileDescriptor
 * Method : initIDs
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_FileDescriptor_initIDs()
{
    
}

} /* extern "C" */

