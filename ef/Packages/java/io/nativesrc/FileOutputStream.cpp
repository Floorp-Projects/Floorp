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
#include "java_io_FileOutputStream.h"

#include "java_io_FileDescriptor.h"
#include "java_lang_String.h"

#include "prio.h"
#include "prprf.h"

#include "ErrorHandling.h"
#include "Mapping.h"
#include "JavaString.h"
#include "SysCallsRuntime.h"

extern Mapping fdMapping;

extern "C" {
inline PRFileDesc *java_io_FileOutputStream_GetDesc(Java_java_io_FileOutputStream *str) 
{
    PRFileDesc *desc = fdMapping.get(str->fd->fd);
    
    if (!desc)
	runtimeError(RuntimeError::IOError);

    return desc;
}

inline int64 java_io_FileOutputStream_Write(Java_java_io_FileOutputStream *str,
					    char *buf, int32 len)
{
    PRFileDesc *desc = java_io_FileOutputStream_GetDesc(str);
    
    int nWritten = PR_Write(desc, buf, len);

    if (nWritten < 0)
	runtimeError(RuntimeError::IOError);

    return nWritten;
}

/*
 * Class : java/io/FileOutputStream
 * Method : open
 * Signature : (Ljava/lang/String;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_FileOutputStream_open(Java_java_io_FileOutputStream *stream, 
					    Java_java_lang_String *string)
{
    char *name = ((JavaString *) string)->convertUtf();

    PRFileDesc *desc = PR_Open(name, PR_WRONLY | PR_CREATE_FILE, 0);
    
    JavaString::freeUtf(name);

    if (!desc)
      sysThrowNamedException("java/io/IOException");
    
    int32 fd = fdMapping.add(desc);
    stream->fd->fd = fd;

}

/*
 * Class : java/io/FileOutputStream
 * Method : openAppend
 * Signature : (Ljava/lang/String;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_FileOutputStream_openAppend(Java_java_io_FileOutputStream *stream, Java_java_lang_String *string)
{
    char *name = ((JavaString *) string)->convertUtf();

    PRFileDesc *desc = PR_Open(name, PR_APPEND, 0);

    JavaString::freeUtf(name);

    if (!desc)
	  sysThrowNamedException("java/io/IOException");
    
    int32 fd = fdMapping.add(desc);
    stream->fd->fd = fd;
}


/*
 * Class : java/io/FileOutputStream
 * Method : write
 * Signature : (I)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_FileOutputStream_write(Java_java_io_FileOutputStream *str, 
					     int32 byte)
{
  java_io_FileOutputStream_Write(str, (char *) &byte, 1);
}


/*
 * Class : java/io/FileOutputStream
 * Method : writeBytes
 * Signature : ([BII)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_FileOutputStream_writeBytes(Java_java_io_FileOutputStream *str, 
						  ArrayOf_uint8 *arr, 
						  int32 offset, int32 len)
{
  if (offset < 0 || offset >= len || (Uint32)len > (arr->length - offset))
    runtimeError(RuntimeError::illegalAccess);

  char *startp = (char *) arr->elements + offset;
  java_io_FileOutputStream_Write(str, startp, len);
  
}


/*
 * Class : java/io/FileOutputStream
 * Method : close
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_FileOutputStream_close(Java_java_io_FileOutputStream *str)
{
  fdMapping.close(str->fd->fd);
}

/*
 * Class : java/io/FileOutputStream
 * Method : initIDs
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_FileOutputStream_initIDs()
{
    /* Currently empty, since all our initialization is done statically (see Mapping.h) */
}

} /* extern "C" */

