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
#include "java_io_FileInputStream.h"
#include "java_io_FileDescriptor.h"

#include "prio.h"
#include "prprf.h"
#include "Mapping.h"

#include "ErrorHandling.h"

#include "JavaString.h"
#include "SysCallsRuntime.h"

extern Mapping fdMapping;

extern "C" {

inline PRFileDesc *java_io_FileInputStream_GetDesc(Java_java_io_FileInputStream *str) 
{
    PRFileDesc *desc = fdMapping.get(str->fd->fd);
    
    if (!desc)
		sysThrowNamedException("java/io/IOException");

    return desc;
}

inline int64 java_io_FileInputStream_Read(Java_java_io_FileInputStream *str,
					  char *buf, int32 len)
{
    PRFileDesc *desc = java_io_FileInputStream_GetDesc(str);
    
    int nRead = PR_Read(desc, buf, len);

    if (nRead == 0)
	return -1; /* EOF */
    else if (nRead < 0)
		sysThrowNamedException("java/io/IOException");
    return nRead;
}

/*
 * Class : java/io/FileInputStream
 * Method : open
 * Signature : (Ljava/lang/String;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_FileInputStream_open(Java_java_io_FileInputStream *stream, 
					   Java_java_lang_String *str)
{
    char *name = ((JavaString *) str)->convertUtf();
    
    PRFileDesc *desc = PR_Open(name, PR_RDONLY, 0);
    
    JavaString::freeUtf(name);

    if (!desc) 
		sysThrowNamedException("java/io/IOException");
	    
    Int32 fd = fdMapping.add(desc);
    stream->fd->fd = fd;
}


/*
 * Class : java/io/FileInputStream
 * Method : read
 * Signature : ()I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_io_FileInputStream_read(Java_java_io_FileInputStream *str)
{
    char c;
    Int32 nRead = (int32) java_io_FileInputStream_Read(str, &c, 1);
    assert(nRead == 1);
    return (Int32) c;
}


/*
 * Class : java/io/FileInputStream
 * Method : readBytes
 * Signature : ([BII)I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_io_FileInputStream_readBytes(Java_java_io_FileInputStream *str, 
						ArrayOf_uint8 *arr, 
						int32 offset, 
						int32 len)
{
    if (offset < 0 || offset >= len || (Uint32)len > (arr->length - offset))
	runtimeError(RuntimeError::illegalAccess);

    char *startp = (char *) arr->elements + offset;
    int32 nRead = (int32) java_io_FileInputStream_Read(str, startp, len);
  
    return nRead;
}


/*
 * Class : java/io/FileInputStream
 * Method : skip
 * Signature : (J)J
 */
NS_EXPORT NS_NATIVECALL(int64)
Netscape_Java_java_io_FileInputStream_skip(Java_java_io_FileInputStream *str,
					   int64 nBytesToSkip)
{
    if (nBytesToSkip < 0)
		sysThrowNamedException("java/io/IOException");
	else if (nBytesToSkip == 0)
		return 0;
	
    char *buf = new char[(int32) nBytesToSkip];

    if (!buf)
	return 0;

    int32 nRead = (int32) java_io_FileInputStream_Read(str, buf, (int32) nBytesToSkip);
    
    return nRead;
}


/*
 * Class : java/io/FileInputStream
 * Method : available
 * Signature : ()I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_io_FileInputStream_available(Java_java_io_FileInputStream *str)
{
    PRFileDesc *desc = java_io_FileInputStream_GetDesc(str);
    
    return PR_Available(desc);
}


/*
 * Class : java/io/FileInputStream
 * Method : close
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_FileInputStream_close(Java_java_io_FileInputStream *str)
{
    fdMapping.close(str->fd->fd);
}

/*
 * Class : java/io/FileInputStream
 * Method : initIDs
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_FileInputStream_initIDs()
{
    /* Currently empty, since all our initialization is done statically (see Mapping.h) */
}


} /* extern "C" */

