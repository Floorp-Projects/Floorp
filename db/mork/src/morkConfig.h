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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

#ifndef _MORKCONFIG_
#define _MORKCONFIG_ 1

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// { %%%%% begin debug mode options in Mork %%%%%
#define MORK_DEBUG 1
// } %%%%% end debug mode options in Mork %%%%%

#ifdef MORK_DEBUG
#define MORK_MAX_CODE_COMPILE 1
#endif

// { %%%%% begin platform defs peculiar to Mork %%%%%
#ifdef XP_MAC
#define MORK_MAC 1
#endif

#ifdef XP_OS2
#define MORK_OS2 1
#endif

#ifdef XP_PC
#define MORK_WIN 1
#define XP_WIN 1
#endif

#ifdef XP_UNIX
#define MORK_UNIX 1
#endif
// } %%%%% end platform defs peculiar to Mork %%%%%

#if defined (MORK_WIN) || defined(MORK_UNIX) 
#include "stdio.h" 
#include "ctype.h" 
#include "errno.h" 
#include "string.h" 
#include "memory.h" 
#include "nsDebug.h" 

#define MORK_ISPRINT(c) isprint(c) 

#define MORK_FILETELL(file) ftell(file) 
#define MORK_FILESEEK(file, where, how) fseek(file, where, how) 
#define MORK_FILEREAD(outbuf, insize, file) fread(outbuf, insize, 1, file) 
#define MORK_FILEFLUSH(file) fflush(file) 
#define MORK_FILECLOSE(file) fclose(file) 
#endif /*MORK_WIN*/

#ifdef MORK_MAC 
#include "xp_file.h" 
#include "ctype.h" 

#define MORK_ISPRINT(c) isprint(c) 

#define MORK_FILETELL(file) XP_FileTell(file) 
#define MORK_FILESEEK(file, where, how) XP_FileSeek(file, where, how) 
#define MORK_FILEREAD(outbuf, insize, file) XP_FileRead(outbuf, insize, file) 
#define MORK_FILEFLUSH(file) XP_FileFlush(file) 
#define MORK_FILECLOSE(file) XP_FileClose(file) 
#endif /*MORK_MAC*/

/* ===== ===== ===== ===== line characters ===== ===== ===== ===== */
#define mork_kCR '\015'
#define mork_kLF '\012'
#define mork_kVTAB '\013'
#define mork_kFF '\014'
#define mork_kTAB '\011'
#define mork_kCRLF "\015\012"     /* A CR LF equivalent string */

#ifdef MORK_MAC
#  define mork_kNewline             "\015"
#  define mork_kNewlineSize 1
#else
#  if defined(MORK_WIN) || defined(MORK_OS2)
#    define mork_kNewline           "\015\012"
#    define mork_kNewlineSize       2
#  else
#    ifdef MORK_UNIX
#      define mork_kNewline         "\012"
#      define mork_kNewlineSize     1
#    endif /* MORK_UNIX */
#  endif /* MORK_WIN */
#endif /* MORK_MAC */

// { %%%%% begin assertion macro %%%%%
extern void mork_assertion_signal(const char* inMessage);
#define MORK_ASSERTION_SIGNAL(Y) mork_assertion_signal(Y)
#define MORK_ASSERT(X) if (!(X)) MORK_ASSERTION_SIGNAL(#X)
// } %%%%% end assertion macro %%%%%

#define MORK_LIB(return) return /*API return declaration*/
#define MORK_LIB_IMPL(return) return /*implementation return declaration*/

// { %%%%% begin standard c utility methods %%%%%
/*define MORK_USE_XP_STDLIB 1*/

#ifdef MORK_MAC 
#define MORK_PROVIDE_STDLIB 1
#endif /*MORK_MAC*/

#ifdef MORK_WIN 
#define MORK_USE_C_STDLIB 1
#endif /*MORK_WIN*/

#ifdef MORK_USE_C_STDLIB
#define MORK_MEMCMP(src1,src2,size)  memcmp(src1,src2,size)
#define MORK_MEMCPY(dest,src,size)   memcpy(dest,src,size)
#define MORK_MEMMOVE(dest,src,size)  memmove(dest,src,size)
#define MORK_MEMSET(dest,byte,size)  memset(dest,byte,size)
#define MORK_STRCPY(dest,src)        strcpy(dest,src)
#define MORK_STRCMP(one,two)         strcmp(one,two)
#define MORK_STRNCMP(one,two,length) strncmp(one,two,length)
#define MORK_STRLEN(string)          strlen(string)
#endif /*MORK_USE_C_STDLIB*/

#ifdef MORK_PROVIDE_STDLIB
MORK_LIB(mork_i4) mork_memcmp(const void* a, const void* b, mork_size inSize);
MORK_LIB(void) mork_memcpy(void* dst, const void* src, mork_size inSize);
MORK_LIB(void) mork_memmove(void* dst, const void* src, mork_size inSize);
MORK_LIB(void) mork_memset(void* dst, int inByte, mork_size inSize);
MORK_LIB(void) mork_strcpy(void* dst, const void* src);
MORK_LIB(mork_i4) mork_strcmp(const void* a, const void* b);
MORK_LIB(mork_i4) mork_strncmp(const void* a, const void* b, mork_size inSize);
MORK_LIB(mork_size) mork_strlen(const void* inString);

#define MORK_MEMCMP(src1,src2,size)  mork_memcmp(src1,src2,size)
#define MORK_MEMCPY(dest,src,size)   mork_memcpy(dest,src,size)
#define MORK_MEMMOVE(dest,src,size)  mork_memmove(dest,src,size)
#define MORK_MEMSET(dest,byte,size)  mork_memset(dest,byte,size)
#define MORK_STRCPY(dest,src)        mork_strcpy(dest,src)
#define MORK_STRCMP(one,two)         mork_strcmp(one,two)
#define MORK_STRNCMP(one,two,length) mork_strncmp(one,two,length)
#define MORK_STRLEN(string)          mork_strlen(string)
#endif /*MORK_PROVIDE_STDLIB*/

#ifdef MORK_USE_XP_STDLIB
#define MORK_MEMCMP(src1,src2,size)  XP_MEMCMP(src1,src2,size)
#define MORK_MEMCPY(dest,src,size)   XP_MEMCPY(dest,src,size)
#define MORK_MEMMOVE(dest,src,size)  XP_MEMMOVE(dest,src,size)
#define MORK_MEMSET(dest,byte,size)  XP_MEMSET(dest,byte,size)
#define MORK_STRCPY(dest,src)        XP_STRCPY(dest,src)
#define MORK_STRCMP(one,two)         XP_STRCMP(one,two)
#define MORK_STRNCMP(one,two,length) XP_STRNCMP(one,two,length)
#define MORK_STRLEN(string)          XP_STRLEN(string)
#endif /*MORK_USE_XP_STDLIB*/


// } %%%%% end standard c utility methods %%%%%

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKCONFIG_ */
