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

#include "java_io_File.h"
#include "java_lang_String.h"

#include "prio.h"
#include "prprf.h"

#include "JavaString.h"
#include "JavaVM.h"
#include "SysCallsRuntime.h"

extern "C" {

/* Implementation of the native code in java/io/File */

static bool getFileInfo(Java_java_io_File *file, PRFileInfo &info) 
{
    JavaString *str = (JavaString *) file->path;
	if (str == NULL)
		sysThrowNullPointerException();

    char *filePath = str->convertUtf();
    
    bool ret = (PR_GetFileInfo(filePath, &info) == PR_SUCCESS);
    JavaString::freeUtf(filePath);
    
    return ret;
}

/* Given a File object, return its file name. The null-terminated
 * string returned by this function must be freed using JavaString::freeUtf()
 */
static char *getFilePath(Java_java_io_File *file)
{
    JavaString *str = (JavaString *) file->path;
	
	if (str == NULL)
		sysThrowNullPointerException();

	char *filePath = str->convertUtf();
	return filePath;
}


/*
 * Class : java/io/File
 * Method : exists0
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_io_File_exists0(Java_java_io_File *file)
{
    PRFileInfo info;

    return getFileInfo(file, info);
}


/*
 * Class : java/io/File
 * Method : canWrite0
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_io_File_canWrite0(Java_java_io_File *file)
{
    /* Open the file for writing; if it succeeds, we can write to the file. */
    char *filePath = getFilePath(file);

    PRFileDesc *fd = PR_Open(filePath, PR_WRONLY, 00644);
    
    JavaString::freeUtf(filePath);

    if (!fd)
	return false;
    
    PR_Close(fd);
    return true;
}



/*
 * Class : java/io/File
 * Method : canRead0
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_io_File_canRead0(Java_java_io_File *file)
{
    /* Open the file for reading; if it succeeds, we can write to the file. */
    char *filePath = getFilePath(file);

    PRFileDesc *fd = PR_Open(filePath, PR_RDONLY, 00644);

    JavaString::freeUtf(filePath);

    if (!fd)
	return false;

    PR_Close(fd);
    return true;
}



/*
 * Class : java/io/File
 * Method : isFile0
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_io_File_isFile0(Java_java_io_File *file)
{
    PRFileInfo fileInfo;
    
    if (!getFileInfo(file, fileInfo))
	return false;
  
    return (fileInfo.type == PR_FILE_FILE);
}



/*
 * Class : java/io/File
 * Method : isDirectory0
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_io_File_isDirectory0(Java_java_io_File *file)
{
    PRFileInfo fileInfo;
    
    if (!getFileInfo(file, fileInfo))
	return false;
    
    return (fileInfo.type == PR_FILE_DIRECTORY);
}



/*
 * Class : java/io/File
 * Method : lastModified0
 * Signature : ()J
 */
NS_EXPORT NS_NATIVECALL(int64)
Netscape_Java_java_io_File_lastModified0(Java_java_io_File *)
{
    PR_fprintf(PR_STDERR, "Warning: File::lastModified0() not implemented\n");
    return 0;
}



/*
 * Class : java/io/File
 * Method : length0
 * Signature : ()J
 */
NS_EXPORT NS_NATIVECALL(int64)
Netscape_Java_java_io_File_length0(Java_java_io_File *file)
{
    PRFileInfo fileInfo;
    
    if (!getFileInfo(file, fileInfo))
	return 0;
  
    return ((Int64) fileInfo.size);
}



/*
 * Class : java/io/File
 * Method : mkdir0
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_io_File_mkdir0(Java_java_io_File *file)
{
    char *dirName = getFilePath(file);
    bool ret = true;

    if (PR_MkDir(dirName, 00755) == PR_FAILURE)
	ret = false;
    
    JavaString::freeUtf(dirName);

    return ret;
}



/*
 * Class : java/io/File
 * Method : renameTo0
 * Signature : (Ljava/io/File;)Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_io_File_renameTo0(Java_java_io_File *from, 
				     Java_java_io_File *to)
{
    char *fromName = getFilePath(from);
    char *toName = getFilePath(to);
    
    bool ret = (PR_Rename(fromName, toName) == PR_SUCCESS);

    JavaString::freeUtf(fromName);
    JavaString::freeUtf(toName);

    return ret;
}



/*
 * Class : java/io/File
 * Method : delete0
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_io_File_delete0(Java_java_io_File *file)
{
    char *name = getFilePath(file);
    
    bool ret = (PR_Delete(name) == PR_SUCCESS);

    JavaString::freeUtf(name);
    return ret;
}



/*
 * Class : java/io/File
 * Method : rmdir0
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_io_File_rmdir0(Java_java_io_File *file)
{
    char *name = getFilePath(file);
    
    bool ret = (PR_RmDir(name) == PR_SUCCESS); 

    JavaString::freeUtf(name);
    return ret;
}



/*
 * Class : java/io/File
 * Method : list0
 * Signature : ()[Ljava/lang/String;
 */
NS_EXPORT NS_NATIVECALL(ArrayOf_Java_java_lang_String *)
Netscape_Java_java_io_File_list0(Java_java_io_File *dir)
{
    char *name = getFilePath(dir);

    PRFileInfo info;
	if (!getFileInfo(dir, info))
		return 0;
    
	PRDir* d = PR_OpenDir(name);
	PRDirEntry* e;
	Uint32 count = 0;
	while ((e = PR_ReadDir(d, PR_SKIP_BOTH)) != NULL)
		count++;
	PR_CloseDir(d);
	d = PR_OpenDir(name);

	JavaArray *arr = (JavaArray *) sysNewObjectArray(&VM::getStandardClass(cString), count);
	ArrayOf_Java_java_lang_String *stringArray = (ArrayOf_Java_java_lang_String *) arr;
	for (Uint32 i = 0; i < count; i++)
		stringArray->elements[i] = (Java_java_lang_String*) JavaString::make(PR_ReadDir(d, PR_SKIP_BOTH)->name);
	
	PR_CloseDir(d);
	return stringArray;
}



/*
 * Class : java/io/File
 * Method : canonPath
 * Signature : (Ljava/lang/String;)Ljava/lang/String;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_String *)
Netscape_Java_java_io_File_canonPath(Java_java_io_File *, Java_java_lang_String *)
{
    printf("File::canonPath() not yet implemented\n");
    return 0;
}



/*
 * Class : java/io/File
 * Method : isAbsolute
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32 /* bool */)
Netscape_Java_java_io_File_isAbsolute(Java_java_io_File *file)
{
  JavaString* jspath = (JavaString*)file->path;
  const uint16* path = jspath->getStr();
  char first = (char) path[0];
  
  // Shouldn't this be done in NSPR ? 

#if defined(XP_UNIX)
  Class &clazz = const_cast<Class&>(file->getClass());
  Field &fld = clazz.getField("pathSeparatorChar");
  int16 pathSeparatorChar = fld.getChar(NULL); // pathSeparatorChar is a static field
  if (first == pathSeparatorChar) 
    return 1;
  else return 0;
#elif defined(XP_PC)
  if (first == '/' || first == '\\') 
    return 1;
  char second = (char) path[1];
  if (((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')) 
      && (second == ':'))
    return 1;
  return 0;
#else  
  printf("File::isAbsolute() not yet implemented on this platform\n");  
#endif
  return 0;
}

/*
 * Class : java/io/File
 * Method : initIDs
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_io_File_initIDs()
{
    /* Currently empty, since all our initialization is done statically (see Mapping.h) */
}


} /* extern "C" */

