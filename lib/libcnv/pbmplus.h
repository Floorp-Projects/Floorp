/* pbmplus.h - header file for PBM, PGM, PPM, and PNM
**
** Copyright (C) 1988, 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#ifndef _PBMPLUS_H_
#define _PBMPLUS_H_

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>

#define SYSV
#if 0//MFJ
#if defined(USG) || defined(SVR4)
#define SYSV
#endif
#if ! ( defined(BSD) || defined(SYSV) || defined(MSDOS) )
/* CONFIGURE: If your system is >= 4.2BSD, set the BSD option; if you're a
** System V site, set the SYSV option; and if you're IBM-compatible, set
** MSDOS.  If your compiler is ANSI C, you're probably better off setting
** SYSV - all it affects is string handling.
*/
#define BSD
/* #define SYSV */
/* #define MSDOS */
#endif
#endif

/* CONFIGURE: If you want to enable writing "raw" files, set this option.
** "Raw" files are smaller, and much faster to read and write, but you
** must have a filesystem that allows all 256 ASCII characters to be read
** and written.  You will no longer be able to mail P?M files without 
** using uuencode or the equivalent, or running the files through pnmnoraw.
** Note that reading "raw" files works whether writing is enabled or not.
*/
#define PBMPLUS_RAWBITS

/* CONFIGURE: PGM can store gray values as either bytes or shorts.  For most
** applications, bytes will be big enough, and the memory savings can be
** substantial.  However, if you need more than 8 bits of grayscale resolution,
** then define this symbol.
*/
/* #define PGM_BIGGRAYS */

/* CONFIGURE: Normally, PPM handles a pixel as a struct of three grays.
** If grays are stored in bytes, that's 24 bits per color pixel; if
** grays are stored as shorts, that's 48 bits per color pixel.  PPM
** can also be configured to pack the three grays into a single longword,
** 10 bits each, 30 bits per pixel.
**
** If you have configured PGM with the PGM_BIGGRAYS option, AND you don't
** need more than 10 bits for each color component, AND you care more about
** memory use than speed, then this option might be a win.  Under these
** circumstances it will make some of the programs use 1.5 times less space,
** but all of the programs will run about 1.4 times slower.
**
** If you are not using PGM_BIGGRAYS, then this option is useless -- it
** doesn't save any space, but it still slows things down.
*/
/* #define PPM_PACKCOLORS */

/* CONFIGURE: uncomment this to enable debugging checks. */
/* #define DEBUG */

#ifdef SYSV

#include <string.h>
#define index(s,c) strchr(s,c)
#define rindex(s,c) strrchr(s,c)
#define srandom(s) srand(s)
#define random rand
#define bzero(dst,len) memset(dst,0,len)
#define bcopy(src,dst,len) memcpy(dst,src,len)
#define bcmp memcmp
/*extern void srand();JUDGE*/
/*extern int16 rand();JUDGE*/

#else /*SYSV*/

#include <strings.h>
extern void srandom();
extern int32 random();

#endif /*SYSV*/

/*extern int16 atoi();JUDGE*/
/*extern void exit();JUDGE*/
/*extern int32 time();JUDGE*/
/*extern int16 write();JUDGE*/

/* CONFIGURE: On some systems, malloc.h doesn't declare these, so we have
** to do it.  On other systems, for example HP/UX, it declares them
** incompatibly.  And some systems, for example Dynix, don't have a
** malloc.h at all.  A sad situation.  If you have compilation problems
** that point here, feel free to tweak or remove these declarations.
*/
#include <malloc.h>
#if 0
/*JUDGE*/
extern char* malloc();
extern char* realloc();
extern char* calloc();
#endif
/* CONFIGURE: Some systems don't have vfprintf(), which we need for the
** error-reporting routines.  If you compile and get a link error about
** this routine, uncomment the first define, which gives you a vfprintf
** that uses the theoretically non-portable but fairly common routine
** _doprnt().  If you then get a link error about _doprnt, or
** message-printing doesn't look like it's working, try the second
** define instead.
*/
/* #define NEED_VFPRINTF1 */
/* #define NEED_VFPRINTF2 */

/* End of configurable definitions. */


#undef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#undef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#undef abs
#define abs(a) ((a) >= 0 ? (a) : -(a))
#undef odd
#define odd(n) ((n) & 1)


/* Definitions to make PBMPLUS work with either ANSI C or C Classic. */

#define ARGS(alist) alist


/* Initialization. */

void pm_init ARGS(( int16* argcP, char* argv[] ));


/* Variable-sized arrays definitions. */

char** pm_allocarray ARGS(( int16 cols, int16 rows, int16 size ));
char* pm_allocrow ARGS(( int16 cols, int16 size ));
void pm_freearray ARGS(( char** its, int16 rows ));
void pm_freerow ARGS(( char* itrow ));


/* Case-insensitive keyword matcher. */

int16 pm_keymatch ARGS(( char* str, char* keyword, int16 minchars ));


/* Log base two hacks. */

int16 pm_maxvaltobits ARGS(( int16 maxval ));
int16 pm_bitstomaxval ARGS(( int16 bits ));


/* Error handling definitions. */

void pm_message ARGS(( char*, ... ));
void pm_error ARGS(( char*, ... ));			/* doesn't return */
void pm_perror ARGS(( char* reason ));			/* doesn't return */
void pm_usage ARGS(( char* usage ));			/* doesn't return */


/* File open/close that handles "-" as stdin and checks errors. */

FILE* pm_openr ARGS(( char* name ));
FILE* pm_openw ARGS(( char* name ));
void pm_close ARGS(( FILE* f ));


/* Endian I/O. */

int16 pm_readbigshort ARGS(( FILE* in, int16* sP ));
int16 pm_writebigshort ARGS(( FILE* out, int16 s ));
int16 pm_readbiglong ARGS(( FILE* in, int32* lP ));
int16 pm_writebiglong ARGS(( FILE* out, int32 l ));
int16 pm_readlittleshort ARGS(( FILE* in, int16* sP ));
int16 pm_writelittleshort ARGS(( FILE* out, int16 s ));
int16 pm_readlittlelong ARGS(( FILE* in, int32* lP ));
int16 pm_writelittlelong ARGS(( FILE* out, int32 l ));


#endif /*_PBMPLUS_H_*/
