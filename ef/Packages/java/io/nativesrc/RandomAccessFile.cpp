/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "java_io_RandomAccessFile.h"
#include "java_io_FileDescriptor.h"
#include "java_lang_String.h"

#include "prio.h"
#include "prprf.h"
#include "Mapping.h"

#include "JavaString.h"
#include "JavaVM.h"

extern Mapping fdMapping;

extern "C" {

/*
 * Class : java/io/RandomAccessFile
 * Method : initIDs
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_RandomAccessFile_initIDs()
{
//    PR_fprintf(PR_STDERR, "Warning: RandomAccessFile::initIDs() not implemented\n");
}

/*
 * Class : java/io/RandomAccessFile
 * Method : open
 * Signature : (Ljava/lang/String;Z)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_RandomAccessFile_open(Java_java_io_RandomAccessFile *raFile, Java_java_lang_String *str, uint32 /* bool */ w)
{
    char *name = ((JavaString *) str)->convertUtf();
    
    PRFileDesc *desc = PR_Open(name, (w) ? PR_RDWR|PR_CREATE_FILE : PR_RDONLY, 0);
    
    JavaString::freeUtf(name);

    if (!desc) 
		sysThrowNamedException("java/io/IOException");
	    
    Int32 fd = fdMapping.add(desc);
    raFile->fd->fd = fd;
}

}
