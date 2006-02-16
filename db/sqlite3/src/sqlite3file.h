
extern "C" {

struct ThreadData;

/*
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

/*
** Forward declarations
*/
typedef struct OsFile OsFile;
typedef struct IoMethod IoMethod;

/*
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

/*
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

/*
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

/* Macro used to comment out routines that do not exists when there is
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


/* This additional API routine is available with redefinable I/O */
struct sqlite3OsVtbl *sqlite3_os_switch(void);
} // extern "C"
