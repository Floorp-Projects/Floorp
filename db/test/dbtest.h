/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)dbtest.h	10.5 (Sleepycat) 4/10/98
 */

extern int debug_on, debug_print, debug_test;

void debug_check __P((void));
int process_am_options __P((Tcl_Interp *, int, char **, DB_INFO **));
int process_env_options __P((Tcl_Interp *, int, char **, DB_ENV **));

#define	DB_ENV_FLAGS \
"\n\
\t-cachesize <size of main memory buffer cache>\n\
\t-config <{name val} pairs>\n\
\t-conflicts <conflict matrix>\n\
\t-detect <deadlock detector mode>\n\
\t-dbenv <db environment cookie>\n\
\t-dbflags <flags to db_appinit>\n\
\t-dbhome <db home directory>\n\
\t-flags <bitmasks>\n\
\t-maxlocks <maximum number of locks in lock table>\n\
\t-maxsize <maximum log file size before changing files>\n\
\t-maxtxns <maximum number of concurrent transactions>\n\
\t-nmodes <number of lock modes>\n\
\t-shmem <shared memory type>"

#define DB_INFO_FLAGS \
"\n\
\t-cachesize <size of main memory buffer cache>\n\
\t-compare <btree comparison function>\n\
\t-ffactor <hash table page fill factor>\n\
\t-flags <bitmask>\n\
\t-hash <hash function>\n\
\t-minkey <minimum number of keys per btree page>\n\
\t-nelem <expected number of elements in table>\n\
\t-order <byte-order>\n\
\t-prefix <btree prefix function>\n\
\t-psize <file pagesize>\n\
\t-recdelim <recno record delimiter character>\n\
\t-reclen <recno record length>\n\
\t-recpad <recno record pad character>\n\
\t-recsrc <recno backing byte stream file>"
