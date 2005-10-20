/*
** 2001 September 16
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This file contains code that is specific to particular operating
** systems.  The purpose of this file is to provide a uniform abstraction
** on which the rest of SQLite can operate.
*/
#include "sqliteInt.h"
#include "os.h"

#if OS_OS2
# include <time.h>
# include <errno.h>
# include <unistd.h>
# define INCL_DOSFILEMGR
# define INCL_DOSERRORS
# define INCL_DOSPROCESS
# include <os2.h>
# include <stdio.h>
# include <stdlib.h>
# include <io.h>
# include <share.h>

/*
** Include code that is common to all os_*.c files
*/
#include "os_common.h"

/*
** Macros used to determine whether or not to use threads.  The
** SQLITE_UNIX_THREADS macro is defined if we are synchronizing for
** Posix threads and SQLITE_W32_THREADS is defined if we are
** synchronizing using Win32 threads.
*/
#if defined(THREADSAFE) && THREADSAFE
/* this mutex implementation only available with EMX */
# include <sys/builtin.h>
# include <sys/smutex.h>
# define SQLITE_OS2_THREADS 1
#endif

/*
** Do not include any of the File I/O interface procedures if the
** SQLITE_OMIT_DISKIO macro is defined (indicating that there database
** will be in-memory only)
*/
#ifndef SQLITE_OMIT_DISKIO

/*
** Delete the named file
*/
int sqlite3OsDelete(const char *zFilename){
  unlink(zFilename);
  return SQLITE_OK;
}

/*
** Return TRUE if the named file exists.
*/
int sqlite3OsFileExists(const char *zFilename){
  return access(zFilename, 0)==0;
}

#if 0 /* NOT USED */
/*
** Change the name of an existing file.
*/
int sqlite3OsFileRename(const char *zOldName, const char *zNewName){
  if( link(zOldName, zNewName) ){
    return SQLITE_ERROR;
  }
  unlink(zOldName);
  return SQLITE_OK;
}
#endif /* NOT USED */

/*
** Attempt to open a file for both reading and writing.  If that
** fails, try opening it read-only.  If the file does not exist,
** try to create it.
**
** On success, a handle for the open file is written to *id
** and *pReadonly is set to 0 if the file was opened for reading and
** writing or 1 if the file was opened read-only.  The function returns
** SQLITE_OK.
**
** On failure, the function returns SQLITE_CANTOPEN and leaves
** *id and *pReadonly unchanged.
*/
int sqlite3OsOpenReadWrite(
  const char *zFilename,
  OsFile *id,
  int *pReadonly
){

  id->h = sopen(zFilename, O_RDWR|O_CREAT|O_BINARY, SH_DENYNO, 0600);
  if( id->h<0 ){
    id->h = sopen(zFilename, O_RDONLY|O_BINARY, SH_DENYNO);
    if( id->h<0 ){
      return SQLITE_CANTOPEN; 
    }
    *pReadonly = 1;
  }else{
    *pReadonly = 0;
  }
  id->locked = 0;
  id->delOnClose = 0;
  TRACE3("OPEN    %-3d %s\n", id->h, zFilename);
  OpenCounter(+1);
  return SQLITE_OK;
}


/*
** Attempt to open a new file for exclusive access by this process.
** The file will be opened for both reading and writing.  To avoid
** a potential security problem, we do not allow the file to have
** previously existed.  Nor do we allow the file to be a symbolic
** link.
**
** If delFlag is true, then make arrangements to automatically delete
** the file when it is closed.
**
** On success, write the file handle into *id and return SQLITE_OK.
**
** On failure, return SQLITE_CANTOPEN.
*/
int sqlite3OsOpenExclusive(const char *zFilename, OsFile *id, int delFlag){

  if( access(zFilename, 0)==0 ){
    return SQLITE_CANTOPEN;
  }
  id->h = sopen(zFilename, O_RDWR|O_CREAT|O_EXCL|O_BINARY, SH_DENYNO, 0600);
  if( id->h<0 ){
    return SQLITE_CANTOPEN;
  }
  id->locked = 0;
  id->delOnClose = delFlag;
  if (delFlag)
    id->pathToDel = sqlite3OsFullPathname(zFilename);
  TRACE3("OPEN-EX %-3d %s\n", id->h, zFilename);
  OpenCounter(+1);
  return SQLITE_OK;
}

/*
** Attempt to open a new file for read-only access.
**
** On success, write the file handle into *id and return SQLITE_OK.
**
** On failure, return SQLITE_CANTOPEN.
*/
int sqlite3OsOpenReadOnly(const char *zFilename, OsFile *id){

  id->h = sopen(zFilename, O_RDONLY|O_BINARY, SH_DENYNO, 0600);
  if( id->h<0 ){
    return SQLITE_CANTOPEN;
  }
  id->locked = 0;
  id->delOnClose = 0;
  TRACE3("OPEN-RO %-3d %s\n", id->h, zFilename);
  OpenCounter(+1);
  return SQLITE_OK;
}

/*
** Attempt to open a file descriptor for the directory that contains a
** file.  This file descriptor can be used to fsync() the directory
** in order to make sure the creation of a new file is actually written
** to disk.
**
** This routine is only meaningful for Unix.  It is a no-op under
** windows since windows does not support hard links.
**
** On success, a handle for a previously open file is at *id is
** updated with the new directory file descriptor and SQLITE_OK is
** returned.
**
** On failure, the function returns SQLITE_CANTOPEN and leaves
** *id unchanged.
*/
int sqlite3OsOpenDirectory(
  const char *zDirname,
  OsFile *id
){
  return SQLITE_OK;
}

/*
** If the following global variable points to a string which is the
** name of a directory, then that directory will be used to store
** temporary files.
*/
char *sqlite3_temp_directory = 0;

/*
** Create a temporary file name in zBuf.  zBuf must be big enough to
** hold at least SQLITE_TEMPNAME_SIZE characters.
*/
int sqlite3OsTempFileName(char *zBuf){
  static const char *azDirs[] = {
     "/temp",                     /* supercede from TEMP env var */
     "/temp",
     "/tmp",
     ".",
  };
  static const char zChars[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";
  int i, j;
  struct stat buf;
  const char *zDir = ".";
  azDirs[0] = getenv("TEMP");
  zDir = ".";
  for(i=0; i<sizeof(azDirs)/sizeof(azDirs[0]); i++){
    if( !azDirs[i] ) continue;
    if( stat(azDirs[i], &buf) ) continue;
    if( !S_ISDIR(buf.st_mode) ) continue;
    if( access(azDirs[i], 07) ) continue;
    zDir = azDirs[i];
    break;
  }
  do{
    snprintf(zBuf, SQLITE_TEMPNAME_SIZE, "%s/"TEMP_FILE_PREFIX, zDir);
    j = strlen(zBuf);
    sqlite3Randomness(15, &zBuf[j]);
    for(i=0; i<15; i++, j++){
      zBuf[j] = (char)zChars[ ((unsigned char)zBuf[j])%(sizeof(zChars)-1) ];
    }
    zBuf[j] = 0;
  }while( access(zBuf,0)==0 );
  return SQLITE_OK; 
}

#ifndef SQLITE_OMIT_PAGER_PRAGMAS
/*
** Check that a given pathname is a directory and is writable 
**
*/
int sqlite3OsIsDirWritable(char *zBuf){
  struct stat buf;
  if( zBuf==0 ) return 0;
  if( zBuf[0]==0 ) return 0;
  if( stat(zBuf, &buf) ) return 0;
  if( !S_ISDIR(buf.st_mode) ) return 0;
  if( access(zBuf, 07) ) return 0;
  return 1;
}
#endif /* SQLITE_OMIT_PAGER_PRAGMAS */

/*
** Read data from a file into a buffer.  Return SQLITE_OK if all
** bytes were read successfully and SQLITE_IOERR if anything goes
** wrong.
*/
int sqlite3OsRead(OsFile *id, void *pBuf, int amt){
  int got;
  
  SimulateIOError(SQLITE_IOERR);
  TIMER_START;
  got = read(id->h, pBuf, amt);
  TIMER_END;
  TRACE5("READ    %-3d %5d %7d %d\n", id->h, got, last_page, TIMER_ELAPSED);
  SEEK(0);
  /* if( got<0 ) got = 0; */
  if( got==amt ){
    return SQLITE_OK;
  }else{
    return SQLITE_IOERR;
  }
}

/*
** Write data from a buffer into a file.  Return SQLITE_OK on success
** or some other error code on failure.
*/
int sqlite3OsWrite(OsFile *id, const void *pBuf, int amt){
  int wrote = 0;
  
  assert( amt>0 );
  SimulateIOError(SQLITE_IOERR);
  SimulateDiskfullError;
  TIMER_START;
  wrote = write(id->h, pBuf, amt);
  TIMER_END;
  if ( wrote<0 ){
    return SQLITE_IOERR;
  }
  TRACE5("WRITE   %-3d %5d %7d %d\n", id->h, got, last_page, TIMER_ELAPSED);
  SEEK(0);
  if( (wrote - amt)>0 ){
    return SQLITE_FULL;
  }
  return SQLITE_OK;
}

/*
** Move the read/write pointer in a file.
*/
int sqlite3OsSeek(OsFile *id, i64 offset){
  
  SEEK(offset/1024 + 1);
    long pos;
    pos = lseek(id->h, offset, SEEK_SET);
    if ( pos<0 )
      return SQLITE_IOERR;
    return SQLITE_OK;
}

#ifdef SQLITE_TEST
/*
** Count the number of fullsyncs and normal syncs.  This is used to test
** that syncs and fullsyncs are occuring at the right times.
*/
int sqlite3_sync_count = 0;
int sqlite3_fullsync_count = 0;
#endif

/*
** The fsync() system call does not work as advertised on many
** unix systems.  The following procedure is an attempt to make
** it work better.
**
** The SQLITE_NO_SYNC macro disables all fsync()s.  This is useful
** for testing when we want to run through the test suite quickly.
** You are strongly advised *not* to deploy with SQLITE_NO_SYNC
** enabled, however, since with SQLITE_NO_SYNC enabled, an OS crash
** or power failure will likely corrupt the database file.
*/
static int full_fsync(int fd, int fullSync){
  int rc;

  /* Record the number of times that we do a normal fsync() and 
  ** FULLSYNC.  This is used during testing to verify that this procedure
  ** gets called with the correct arguments.
  */
#ifdef SQLITE_TEST
  if( fullSync ) sqlite3_fullsync_count++;
  sqlite3_sync_count++;
#endif

  /* If we compiled with the SQLITE_NO_SYNC flag, then syncing is a
  ** no-op
  */
#ifdef SQLITE_NO_SYNC
  rc = SQLITE_OK;
#else

#ifdef F_FULLFSYNC
  if( fullSync ){
    rc = fcntl(fd, F_FULLFSYNC, 0);
  }else{
    rc = 1;
  }
  /* If the FULLSYNC failed, try to do a normal fsync() */
  if( rc ) rc = fsync(fd);

#else
  rc = fsync(fd);
#endif /* defined(F_FULLFSYNC) */
#endif /* defined(SQLITE_NO_SYNC) */

  return rc;
}

/*
** Make sure all writes to a particular file are committed to disk.
**
** Under Unix, also make sure that the directory entry for the file
** has been created by fsync-ing the directory that contains the file.
** If we do not do this and we encounter a power failure, the directory
** entry for the journal might not exist after we reboot.  The next
** SQLite to access the file will not know that the journal exists (because
** the directory entry for the journal was never created) and the transaction
** will not roll back - possibly leading to database corruption.
*/
int sqlite3OsSync(OsFile *id){
  
  SimulateIOError(SQLITE_IOERR);
  TRACE2("SYNC    %-3d\n", id->h);
  if( full_fsync(id->h, id->fullSync)){
    return SQLITE_IOERR;
  }else{
    return SQLITE_OK;
  }
}

/*
** Sync the directory zDirname. This is a no-op on operating systems other
** than UNIX.
*/
int sqlite3OsSyncDirectory(const char *zDirname){
  SimulateIOError(SQLITE_IOERR);
  return SQLITE_OK;
}

/*
** Truncate an open file to a specified size
*/
int sqlite3OsTruncate(OsFile *id, i64 nByte){
  
  SimulateIOError(SQLITE_IOERR);
  return ftruncate(id->h, nByte)==0 ? SQLITE_OK : SQLITE_IOERR;
}

/*
** Determine the current size of a file in bytes
*/
int sqlite3OsFileSize(OsFile *id, i64 *pSize){
  struct stat buf;
  
  SimulateIOError(SQLITE_IOERR);
  if( fstat(id->h, &buf)!=0 ){
    return SQLITE_IOERR;
  }
  *pSize = buf.st_size;
  return SQLITE_OK;
}

#define N_LOCKBYTE       0x7fffffffL
#define FIRST_LOCKBYTE   0L

/*
** Change the status of the lock on the file "id" to be a readlock.
** If the file was write locked, then this reduces the lock to a read.
** If the file was read locked, then this acquires a new read lock.
**
** Return SQLITE_OK on success and SQLITE_BUSY on failure.  If this
** library was compiled with large file support (LFS) but LFS is not
** available on the host, then an SQLITE_NOLFS is returned.
*/
int sqlite3OsCheckReservedLock(OsFile *id){
  int rc;
  if( id->locked>0 ){
    rc = SQLITE_OK;
  }else{
    APIRET s;
    FILELOCK ulock = {0L, 0L};
    FILELOCK lock = {FIRST_LOCKBYTE, N_LOCKBYTE};
    long readlock = 1L;
    if( id->locked<0 ){
      ulock.lOffset = lock.lOffset;
      ulock.lRange = lock.lRange;
      readlock += 2L;                    /* atomic unlock/lock */
    }
    s = DosSetFileLocks(id->h, &ulock, &lock, 0L, readlock);
    if( s!=NO_ERROR ){
      rc = 1;
    }else{
      rc = 0;
      id->locked = 1;
    }
  }
  return rc;
}

#ifdef SQLITE_DEBUG
/*
** Helper function for printing out trace information from debugging
** binaries. This returns the string represetation of the supplied
** integer lock-type.
*/
static const char * locktypeName(int locktype){
  switch( locktype ){
  case NO_LOCK: return "NONE";
  case SHARED_LOCK: return "SHARED";
  case RESERVED_LOCK: return "RESERVED";
  case PENDING_LOCK: return "PENDING";
  case EXCLUSIVE_LOCK: return "EXCLUSIVE";
  }
  return "ERROR";
}
#endif

/*
** Change the lock status to be an exclusive or write lock.  Return
** SQLITE_OK on success and SQLITE_BUSY on a failure.  If this
** library was compiled with large file support (LFS) but LFS is not
** available on the host, then an SQLITE_NOLFS is returned.
*/
int sqlite3OsLock(OsFile *id, int locktype){
  int rc;
  if( id->locked<0 ){
    rc = SQLITE_OK;
  }else{
    APIRET s;
    FILELOCK ulock = {0L, 0L};
    FILELOCK lock = {FIRST_LOCKBYTE, N_LOCKBYTE};
    long writelock = 0L;
    if( id->locked>0 ){
      ulock.lOffset = lock.lOffset;
      ulock.lRange = lock.lRange;
      writelock += 2L;                   /* atomic unlock/lock */
    }
    s = DosSetFileLocks(id->h, &ulock, &lock, 0L, writelock);
    if( s!=NO_ERROR ){
      rc = SQLITE_BUSY;
    }else{
      rc = SQLITE_OK;
      id->locked = -1;
    }
  }
  return rc;
}

/*
** Unlock the given file descriptor.  If the file descriptor was
** not previously locked, then this routine is a no-op.  If this
** library was compiled with large file support (LFS) but LFS is not
** available on the host, then an SQLITE_NOLFS is returned.
*/
int sqlite3OsUnlock(OsFile *id, int locktype){
  int rc;
  if( id->locked==0 ){
    rc = SQLITE_OK;
  }else{
    APIRET s;
    FILELOCK ulock = {FIRST_LOCKBYTE, N_LOCKBYTE};
    FILELOCK lock = {0L, 0L};
    s = DosSetFileLocks(id->h, &ulock, &lock, 0L, 0L);
    if( s!=NO_ERROR ){
      rc = SQLITE_BUSY;
    }else{
      rc = SQLITE_OK;
      id->locked = 0;
    }
  }
  return rc;
}

/*
** Close a file.
*/
int sqlite3OsClose(OsFile *id){
  close(id->h);
  if( id->delOnClose ){
    unlink(id->pathToDel);
    sqliteFree(id->pathToDel);
  }
  TRACE2("CLOSE   %-3d\n", id->h);
  id->isOpen = 0;
  OpenCounter(-1);
  return SQLITE_OK;
}

/*
** Turn a relative pathname into a full pathname.  Return a pointer
** to the full pathname stored in space obtained from sqliteMalloc().
** The calling function is responsible for freeing this space once it
** is no longer needed.
*/
char *sqlite3OsFullPathname(const char *zRelative){
  char *zFull = 0;
  char zPath[260];    /* max OS/2 path length, incl drive */
  if( !_abspath(zPath, zRelative, sizeof(zPath)) ){
    sqlite3SetString(&zFull, zPath, 0);
  }else{
    char zBuf[260];
    snprintf(zPath, sizeof(zPath), "%s/%s",
             getcwd(zBuf, sizeof(zBuf)), zRelative);
    sqlite3SetString(&zFull, zPath, 0);
  }
  return zFull;
}

#endif /* SQLITE_OMIT_DISKIO */
/***************************************************************************
** Everything above deals with file I/O.  Everything that follows deals
** with other miscellanous aspects of the operating system interface
****************************************************************************/

/*
** Get information to seed the random number generator.  The seed
** is written into the buffer zBuf[256].  The calling function must
** supply a sufficiently large buffer.
*/
int sqlite3OsRandomSeed(char *zBuf){
  /* We have to initialize zBuf to prevent valgrind from reporting
  ** errors.  The reports issued by valgrind are incorrect - we would
  ** prefer that the randomness be increased by making use of the
  ** uninitialized space in zBuf - but valgrind errors tend to worry
  ** some users.  Rather than argue, it seems easier just to initialize
  ** the whole array and silence valgrind, even if that means less randomness
  ** in the random seed.
  **
  ** When testing, initializing zBuf[] to zero is all we do.  That means
  ** that we always use the same random number sequence.* This makes the
  ** tests repeatable.
  */
  memset(zBuf, 0, 256);

#if !defined(SQLITE_TEST)
  {
    int pid;
    time((time_t*)zBuf);
    pid = getpid();
    memcpy(&zBuf[sizeof(time_t)], &pid, sizeof(pid));
  }
#endif
  return SQLITE_OK;
}

/*
** Sleep for a little while.  Return the amount of time slept.
*/
int sqlite3OsSleep(int ms){
  APIRET rc;
  rc = DosSleep(ms);
  return ms;
}

/*
** Static variables used for thread synchronization
*/
static int inMutex = 0;
#ifdef SQLITE_OS2_THREADS
  static _smutex mutex = 0;
#endif

/*
** The following pair of routine implement mutual exclusion for
** multi-threaded processes.  Only a single thread is allowed to
** executed code that is surrounded by EnterMutex() and LeaveMutex().
**
** SQLite uses only a single Mutex.  There is not much critical
** code and what little there is executes quickly and without blocking.
*/
void sqlite3OsEnterMutex(){
#ifdef SQLITE_OS2_THREADS
  _smutex_request(&mutex);
#endif
  assert( !inMutex );
  inMutex = 1;
}
void sqlite3OsLeaveMutex(){
  assert( inMutex );
  inMutex = 0;
#ifdef SQLITE_OS2_THREADS
  _smutex_release(&mutex);
#endif
}

/*
** The following variable, if set to a non-zero value, becomes the result
** returned from sqlite3OsCurrentTime().  This is used for testing.
*/
#ifdef SQLITE_TEST
int sqlite3_current_time = 0;
#endif

/*
** Find the current time (in Universal Coordinated Time).  Write the
** current time and date as a Julian Day number into *prNow and
** return 0.  Return 1 if the time and date cannot be found.
*/
int sqlite3OsCurrentTime(double *prNow){
  time_t t;
  time(&t);
  *prNow = t/86400.0 + 2440587.5;
#ifdef SQLITE_TEST
  if( sqlite3_current_time ){
    *prNow = sqlite_current_time/86400.0 + 2440587.5;
  }
#endif
  return 0;
}

#endif /* OS_OS2 */
