/* -*- Mode: C; indent-tabs-mode: nil; -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the TripleDB database library.
 *
 * The Initial Developer of the Original Code is
 * Geocast Network Systems.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terry Weissman <terry@mozilla.org>
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

#ifndef _TDBtypes_h_
#define _TDBtypes_h_ 1

/* These are private, internal type definitions, */

#include <string.h>
#include "tdbapi.h"

#include "db/db.h"

#ifdef TDB_USE_NSPR
#include "pratom.h"
#include "prcvar.h"
#include "prio.h"
#include "prlock.h"
#include "prlog.h"
#include "prmem.h"
#include "prthread.h"
#include "plstr.h"

#include "geoassert/geoassert.h" /* This line can be safely removed if you
                                    are building this in a non-Geocast
                                    environment. */

typedef PRLock          TDBLock;
typedef PRThread        TDBThread;
typedef PRCondVar       TDBCondVar;

#define tdb_Lock(l)     PR_Lock(l)
#define tdb_Unlock(l)   PR_Unlock(l)
#ifdef GeoAssert
#define tdb_ASSERT(x)   GeoAssert(x)
#else
#define tdb_ASSERT(x)   PR_ASSERT(x)
#endif

#define tdb_OpenFileRW(filename) PR_Open(filename, PR_RDWR | PR_CREATE_FILE, 0666)
#define tdb_OpenFileReadOnly(filename) PR_Open(filename, PR_RDONLY, 0666)
#define tdb_Close       PR_Close
#define tdb_Seek(fid, l)        (PR_Seek(fid, l, PR_SEEK_SET) == l ? TDB_SUCCESS : TDB_FAILURE)
#define tdb_Read        PR_Read
#define tdb_Write       PR_Write
#define tdb_Delete      PR_Delete
#define tdb_Rename      PR_Rename
#define tdb_MkDir       PR_MkDir
 
#define tdb_strdup      PL_strdup
#define tdb_strcasestr  PL_strcasestr

#else

#ifdef TDB_USE_THREADS
#undef TDB_USE_THREADS
#endif

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

typedef void* TDBLock;
typedef void* TDBThread;
typedef void* TDBCondVar;

#define tdb_Lock(l)     ((void) 0)
#define tdb_Unlock(l)   ((void) 0)

#define tdb_ASSERT assert

#define tdb_OpenFileRW(filename) fopen(filename, "r+")
#define tdb_OpenFileReadOnly(filename) fopen(filename, "r")
#define tdb_Close       fclose
#define tdb_Seek(fid, l)        (fseek(fid, l, SEEK_SET) == l ? TDB_SUCCESS : TDB_FAILURE)
#define tdb_Read(fid, buf, length)      fread(buf, 1, length, fid)
#define tdb_Write(fid, buf, length)     fwrite(buf, 1, length, fid)
#define tdb_Delete      unlink
#define tdb_Rename      rename
#define tdb_MkDir       mkdir

#define tdb_strdup      strdup
#define tdb_strcasestr  strstr  /* WRONG!  Should be a case-independent one!
                                   XXX ### */

#endif /* TDB_USE_NSPR */



/* Special node types that are used internally only. */

#define TDBTYPE_MINNODE 0       /* A node that is less than all other nodes. */
#define TDBTYPE_INTERNED 126    /* An interned string. */
#define TDBTYPE_MAXNODE 127     /* A node that is bigger than all others. */


/* Magic number that appears as first four bytes of db files. */
#define TDB_MAGIC       "TDB\n"


/* Current version number for db files. */

#define TDB_MAJOR_VERSION     11
#define TDB_MINOR_VERSION     0



/* Maximum number of different record types we are prepared to handle. */

#define TDB_MAXTYPES    20

/* The trees index things in the following orders:
   0:           0, 1, 2
   1:           1, 0, 2
   2:           2, 1, 0
   3:           1, 2, 0
*/

/* MINRECORDSIZE and MAXRECORDSIZE are the minimum and maximum possible record 
   size.  They're useful for sanity checking. */
#define BASERECORDSIZE (sizeof(TDBInt32) + NUMTREES * (2*sizeof(TDBPtr)+sizeof(TDBInt8)))
#define MINRECORDSIZE (BASERECORDSIZE + 3 * 2)
#define MAXRECORDSIZE (BASERECORDSIZE + 3 * 65540)


/* The meaning of the various flag bits in a record entry.  */

#define TDBFLAG_ISASSERT        0x1 /* On if this entry is an assertion, off
                                       if this entry is a false assertion. */
#define TDBFLAG_ISEXTENDED      0x2 /* On if this entry is incomplete, and the
                                       last four bytes of it is actually a
                                       pointer to an extension record.. */
#define TDBFLAG_KEYINSUFFICIENT 0x4 /* Even though the data in the key may
                                       look like it has enough to regenerate
                                       the entire record content, in fact
                                       it doesn't.  This gets set in key
                                       records when string data contains
                                       embedded nulls, since the technique
                                       for getting strings to and from
                                       keys relies on using NULL characters
                                       to mark end-of-string. */


/* The meaning of various flag bits in a record-type byte. */

#define TDB_FREERECORD          0x80 /* If set, then this record is in the
                                        free list. */


/* The meaning of various flag bits in a index record. */

#define TDB_LEAFFLAG  0x1



#define DB_OK 0

typedef TDBInt32 TDBPtr;




typedef struct _TDBCallbackInfo TDBCallbackInfo;
typedef struct _TDBIntern TDBIntern;
typedef struct _TDBPendingCall TDBPendingCall;
typedef struct _TDBRType TDBRType;
typedef struct _TDBVector TDBVector;
typedef struct _TDBWindex TDBWindex;
typedef struct _TDBWindexCursor TDBWindexCursor;



#endif /* _TDBtypes_h_ */
