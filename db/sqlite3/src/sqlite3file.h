//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla History System
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


/**
 * This file collects some internal sqlite data structures that are exported
 * for use by the storage asynchronous IO system. Using all real versions of
 * the internal sqlite header files became complicated because they depend
 * on one another and they have a lot of extra stuff we don't care about.
 *
 * THESE DECLARATIONS MUST BE KEPT IN SYNC with the internal versions, so if
 * you upgrade sqlite, be sure to update these.
 */

extern "C" {

struct ThreadData;

/* FROM sqliteInt.h
** ----------------
** Some compilers do not support the "long long" datatype.  So we have
** to do a typedef that for 64-bit integers that depends on what compiler
** is being used.
*/
#if defined(_MSC_VER) || defined(__BORLANDC__)
  typedef __int64 sqlite_int64;
  typedef unsigned __int64 sqlite_uint64;
#else
  typedef long long int sqlite_int64;
  typedef unsigned long long int sqlite_uint64;
#endif

/* FROM sqliteInt.h
** ----------------
** Call this to check for out of memory errors when returning.
** See definition in util.c
*/
struct sqlite3;
int sqlite3ApiExit(sqlite3 *db, int);


/*
** Forward declarations
*/
typedef struct OsFile OsFile;
typedef struct IoMethod IoMethod;

/* FROM os.h
** ---------
** An instance of the following structure contains pointers to all
** methods on an OsFile object.
*/
struct IoMethod {
  int (*xClose)(OsFile**);
  int (*xOpenDirectory)(OsFile*, const char*);
  int (*xRead)(OsFile*, void*, int amt);
  int (*xWrite)(OsFile*, const void*, int amt);
  int (*xSeek)(OsFile*, sqlite_int64 offset);
  int (*xTruncate)(OsFile*, sqlite_int64 size);
  int (*xSync)(OsFile*, int);
  void (*xSetFullSync)(OsFile *id, int setting);
  int (*xFileHandle)(OsFile *id);
  int (*xFileSize)(OsFile*, sqlite_int64 *pSize);
  int (*xLock)(OsFile*, int);
  int (*xUnlock)(OsFile*, int);
  int (*xLockState)(OsFile *id);
  int (*xCheckReservedLock)(OsFile *id);
};

/* FROM os.h
** ---------
** The OsFile object describes an open disk file in an OS-dependent way.
** The version of OsFile defined here is a generic version.  Each OS
** implementation defines its own subclass of this structure that contains
** additional information needed to handle file I/O.  But the pMethod
** entry (pointing to the virtual function table) always occurs first
** so that we can always find the appropriate methods.
*/
struct OsFile {
  IoMethod const *pMethod;
};

/* FROM os.h
** ---------
** When redefinable I/O is enabled, a single global instance of the
** following structure holds pointers to the routines that SQLite 
** uses to talk with the underlying operating system.  Modify this
** structure (before using any SQLite API!) to accomodate perculiar
** operating system interfaces or behaviors.
*/
struct sqlite3OsVtbl {
  int (*xOpenReadWrite)(const char*, OsFile**, int*);
  int (*xOpenExclusive)(const char*, OsFile**, int);
  int (*xOpenReadOnly)(const char*, OsFile**);

  int (*xDelete)(const char*);
  int (*xFileExists)(const char*);
  char *(*xFullPathname)(const char*);
  int (*xIsDirWritable)(char*);
  int (*xSyncDirectory)(const char*);
  int (*xTempFileName)(char*);

  int (*xRandomSeed)(char*);
  int (*xSleep)(int ms);
  int (*xCurrentTime)(double*);

  void (*xEnterMutex)(void);
  void (*xLeaveMutex)(void);
  int (*xInMutex)(int);
  ThreadData *(*xThreadSpecificData)(int);

  void *(*xMalloc)(int);
  void *(*xRealloc)(void *, int);
  void (*xFree)(void *);
  int (*xAllocationSize)(void *);
};

/* FROM os.h
** ---------
** Macro used to comment out routines that do not exists when there is
** no disk I/O 
*/
#ifdef SQLITE_OMIT_DISKIO
# define IF_DISKIO(X)  0
#else
# define IF_DISKIO(X)  X
#endif

#ifdef _SQLITE_OS_C_
  /*
  ** The os.c file implements the global virtual function table.
  */
  struct sqlite3OsVtbl sqlite3Os = {
    IF_DISKIO( sqlite3OsOpenReadWrite ),
    IF_DISKIO( sqlite3OsOpenExclusive ),
    IF_DISKIO( sqlite3OsOpenReadOnly ),
    IF_DISKIO( sqlite3OsDelete ),
    IF_DISKIO( sqlite3OsFileExists ),
    IF_DISKIO( sqlite3OsFullPathname ),
    IF_DISKIO( sqlite3OsIsDirWritable ),
    IF_DISKIO( sqlite3OsSyncDirectory ),
    IF_DISKIO( sqlite3OsTempFileName ),
    sqlite3OsRandomSeed,
    sqlite3OsSleep,
    sqlite3OsCurrentTime,
    sqlite3OsEnterMutex,
    sqlite3OsLeaveMutex,
    sqlite3OsInMutex,
    sqlite3OsThreadSpecificData,
    sqlite3OsMalloc,
    sqlite3OsRealloc,
    sqlite3OsFree,
    sqlite3OsAllocationSize
  };
#else
  /*
  ** Files other than os.c just reference the global virtual function table. 
  */
  extern struct sqlite3OsVtbl sqlite3Os;
#endif /* _SQLITE_OS_C_ */


/* FROM os.h
** ---------
** This additional API routine is available with redefinable I/O
*/
struct sqlite3OsVtbl *sqlite3_os_switch(void);

} // extern "C"
