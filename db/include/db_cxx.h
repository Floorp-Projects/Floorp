/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)db_cxx.h	10.17 (Sleepycat) 5/2/98
 */

#ifndef _DB_CXX_H_
#define _DB_CXX_H_
//
// C++ assumptions:
//
// To ensure portability to many platforms, both new and old, we make
// few assumptions about the C++ compiler and library.  For example,
// we do not expect STL, templates or namespaces to be available.  The
// "newest" C++ feature used is exceptions, which are used liberally
// to transmit error information.  Even the use of exceptions can be
// disabled at runtime, see setErrorModel().
//
// C++ naming conventions:
//
//  - All top level class names start with Db.
//  - All class members start with lower case letter.
//  - All private data members are suffixed with underscore.
//  - Use underscores to divide names into multiple words.
//  - Simple data accessors are named with get_ or set_ prefix.
//  - All method names are taken from names of functions in the C
//    layer of db (usually by dropping a prefix like "db_").
//    These methods have the same argument types and order,
//    other than dropping the explicit arg that acts as "this".
//
// As a rule, each DbFoo object has exactly one underlying DB_FOO struct
// (defined in db.h) associated with it.  In many cases, we inherit directly
// from the DB_FOO structure to make this relationship explicit.  Often,
// the underlying C layer allocates and deallocates these structures, so
// there is no easy way to add any data to the DbFoo class.  When you see
// a comment about whether data is permitted to be added, this is what
// is going on.  Of course, if we need to add data to such C++ classes
// in the future, we will arrange to have an indirect pointer to the
// DB_FOO struct (as some of the classes already have).
//


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Forward declarations
//

#include "db.h"

class Db;                                        // forward
class Dbc;                                       // forward
class DbEnv;                                     // forward
class DbException;                               // forward
class DbInfo;                                    // forward
class DbLock;                                    // forward
class DbLockTab;                                 // forward
class DbLog;                                     // forward
class DbLsn;                                     // forward
class DbMpool;                                   // forward
class DbMpoolFile;                               // forward
class Dbt;                                       // forward
class DbTxn;                                     // forward
class DbTxnMgr;                                  // forward

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Mechanisms for declaring classes
//

//
// Every class defined in this file has an _exported next to the class name.
// This is needed for WinTel machines so that the class methods can
// be exported or imported in a DLL as appropriate.  Users of the DLL
// use the define DB_USE_DLL.  When the DLL is built, DB_CREATE_DLL
// must be defined.
//
#if defined(_MSC_VER)

#  if defined(DB_CREATE_DLL)
#    define _exported __declspec(dllexport)      // creator of dll
#  elif defined(DB_USE_DLL)
#    define _exported __declspec(dllimport)      // user of dll
#  else
#    define _exported                            // static lib creator or user
#  endif

#else

#  define _exported

#endif

// DEFINE_DB_CLASS defines an imp_ data member and imp() accessor.
// The underlying type is a pointer to an opaque *Imp class, that
// gets converted to the correct implementation class by the implementation.
//
// Since these defines use "private/public" labels, and leave the access
// being "private", we always use these by convention before any data
// members in the private section of a class.  Keeping them in the
// private section also emphasizes that they are off limits to user code.
//
#define DEFINE_DB_CLASS(name) \
    public: class name##Imp* imp() { return imp_; } \
    public: const class name##Imp* imp() const { return imp_; } \
    private: class name##Imp* imp_


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Turn off inappropriate compiler warnings
//

#ifdef _MSC_VER

// These are level 4 warnings that are explicitly disabled.
// With Visual C++, by default you do not see above level 3 unless
// you use /W4.  But we like to compile with the highest level
// warnings to catch other errors.
//
// 4201: nameless struct/union
//       triggered by standard include file <winnt.h>
//
// 4514: unreferenced inline function has been removed
//       certain include files in MSVC define methods that are not called
//
#pragma warning(disable: 4201 4514)

#endif

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Exception classes
//

// Almost any error in the DB library throws a DbException.
// Every exception should be considered an abnormality
// (e.g. bug, misuse of DB, file system error).
//
// NOTE: We would like to inherit from class exception and
//       let it handle what(), but there are
//       MSVC++ problems when <exception> is included.
//
class _exported DbException
{
public:
    virtual ~DbException();
    DbException(int err);
    DbException(const char *description);
    DbException(const char *prefix, int err);
    DbException(const char *prefix1, const char *prefix2, int err);
    const int get_errno();
    virtual const char *what() const;

    DbException(const DbException &);
    DbException &operator = (const DbException &);

private:
    char *what_;
    int err_;                   // errno
};


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Lock classes
//

class _exported DbLock
{
    friend DbLockTab;

public:
    DbLock(u_int);
    DbLock();

    u_int get_lock_id();
    void set_lock_id(u_int);

    int put(DbLockTab *locktab);

    DbLock(const DbLock &);
    DbLock &operator = (const DbLock &);

protected:
    // We can add data to this class if needed
    // since its contained class is not allocated by db.
    // (see comment at top)

    DB_LOCK lock_;
};

class _exported DbLockTab
{
friend DbEnv;
public:
    int close();
    int detect(u_int32_t flags, int atype);
    int get(u_int32_t locker, u_int32_t flags, const Dbt *obj,
            db_lockmode_t lock_mode, DbLock *lock);
    int id(u_int32_t *idp);
    int vec(u_int32_t locker, u_int32_t flags, DB_LOCKREQ list[],
	    int nlist, DB_LOCKREQ **elistp);

    // Create or remove new locktab files
    //
    static int open(const char *dir, u_int32_t flags, int mode,
                    DbEnv* dbenv, DbLockTab **regionp);
    static int unlink(const char *dir, int force, DbEnv* dbenv);

private:
    // We can add data to this class if needed
    // since it is implemented via a pointer.
    // (see comment at top)

    // copying not allowed
    //
    DbLockTab(const DbLockTab &);
    DbLockTab &operator = (const DbLockTab &);

    // Note: use DbLockTab::open() or DbEnv::get_lk_info()
    // to get pointers to a DbLockTab,
    // and call DbLockTab::close() rather than delete to release them.
    //
    DbLockTab();
    ~DbLockTab();

    DEFINE_DB_CLASS(DbLockTab);
};


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Log classes
//

class _exported DbLsn : protected DB_LSN
{
    friend DbLog;               // friendship needed to cast to base class
    friend DbMpool;
};

class _exported DbLog
{
friend DbEnv;
public:
    int archive(char **list[], u_int32_t flags, void *(*db_malloc)(size_t));
    int close();
    static int compare(const DbLsn *lsn0, const DbLsn *lsn1);
    int file(DbLsn *lsn, char *namep, int len);
    int flush(const DbLsn *lsn);
    int get(DbLsn *lsn, Dbt *data, u_int32_t flags);
    int put(DbLsn *lsn, const Dbt *data, u_int32_t flags);

    // Normally these would be called register and unregister to
    // parallel the C interface, but "register" is a reserved word.
    //
    int db_register(Db *dbp, const char *name, DBTYPE type, u_int32_t *fidp);
    int db_unregister(u_int32_t fid);

    // Create or remove new log files
    //
    static int open(const char *dir, u_int32_t flags, int mode,
                    DbEnv* dbenv, DbLog **regionp);
    static int unlink(const char *dir, int force, DbEnv* dbenv);

private:
    // We can add data to this class if needed
    // since it is implemented via a pointer.
    // (see comment at top)

    // Note: use DbLog::open() or DbEnv::get_lg_info()
    // to get pointers to a DbLog,
    // and call DbLog::close() rather than delete to release them.
    //
    DbLog();
    ~DbLog();

    // no copying
    DbLog(const DbLog &);
    operator = (const DbLog &);

    DEFINE_DB_CLASS(DbLog);
};


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Memory pool classes
//

class _exported DbMpoolFile
{
friend DbEnv;
public:
    int close();
    int get(db_pgno_t *pgnoaddr, u_int32_t flags, void *pagep);
    int put(void *pgaddr, u_int32_t flags);
    int set(void *pgaddr, u_int32_t flags);
    int sync();

    static int open(DbMpool *mp, const char *file,
                    u_int32_t flags, int mode, size_t pagesize,
                    DB_MPOOL_FINFO *finfop, DbMpoolFile **mpf);

private:
    // We can add data to this class if needed
    // since it is implemented via a pointer.
    // (see comment at top)

    // Note: use DbMpoolFile::open()
    // to get pointers to a DbMpoolFile,
    // and call DbMpoolFile::close() rather than delete to release them.
    //
    DbMpoolFile();

    // Shut g++ up.
protected:
    ~DbMpoolFile();

private:
    // no copying
    DbMpoolFile(const DbMpoolFile &);
    operator = (const DbMpoolFile &);

    DEFINE_DB_CLASS(DbMpoolFile);
};

class _exported DbMpool
{
friend DbEnv;
public:
    int close();

    // access to low level interface
    // Normally this would be called register to parallel
    // the C interface, but "register" is a reserved word.
    //
    int db_register(int ftype,
                    int (*pgin)(db_pgno_t pgno, void *pgaddr, DBT *pgcookie),
                    int (*pgout)(db_pgno_t pgno, void *pgaddr, DBT *pgcookie));

    int stat(DB_MPOOL_STAT **gsp, DB_MPOOL_FSTAT ***fsp,
             void *(*db_malloc)(size_t));
    int sync(DbLsn *lsn);
    int trickle(int pct, int *nwrotep);

    // Create or remove new mpool files
    //
    static int open(const char *dir, u_int32_t flags, int mode,
                    DbEnv* dbenv, DbMpool **regionp);
    static int unlink(const char *dir, int force, DbEnv* dbenv);

private:
    // We can add data to this class if needed
    // since it is implemented via a pointer.
    // (see comment at top)

    // Note: use DbMpool::open() or DbEnv::get_mp_info()
    // to get pointers to a DbMpool,
    // and call DbMpool::close() rather than delete to release them.
    //
    DbMpool();
    ~DbMpool();

    // no copying
    DbMpool(const DbMpool &);
    DbMpool &operator = (const DbMpool &);

    DEFINE_DB_CLASS(DbMpool);
};


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Transaction classes
//

class _exported DbTxnMgr
{
friend DbEnv;
public:
    int begin(DbTxn *pid, DbTxn **tid);
    int checkpoint(u_int32_t kbyte, u_int32_t min) const;
    int close();
    int stat(DB_TXN_STAT **statp, void *(*db_malloc)(size_t));

    // Create or remove new txnmgr files
    //
    static int open(const char *dir, u_int32_t flags, int mode,
                    DbEnv* dbenv, DbTxnMgr **regionp);
    static int unlink(const char *dir, int force, DbEnv* dbenv);

private:
    // We can add data to this class if needed
    // since it is implemented via a pointer.
    // (see comment at top)

    // Note: use DbTxnMgr::open() or DbEnv::get_tx_info()
    // to get pointers to a DbTxnMgr,
    // and call DbTxnMgr::close() rather than delete to release them.
    //
    DbTxnMgr();
    ~DbTxnMgr();

    // no copying
    DbTxnMgr(const DbTxnMgr &);
    operator = (const DbTxnMgr &);

    DEFINE_DB_CLASS(DbTxnMgr);
};

class _exported DbTxn
{
friend DbTxnMgr;
public:
    int abort();
    int commit();
    u_int32_t id();
    int prepare();

private:
    // We can add data to this class if needed
    // since it is implemented via a pointer.
    // (see comment at top)

    // Note: use DbTxnMgr::begin() to get pointers to a DbTxn,
    // and call DbTxn::abort() or DbTxn::commit rather than
    // delete to release them.
    //
    DbTxn();
    ~DbTxn();

    // no copying
    DbTxn(const DbTxn &);
    operator = (const DbTxn &);

    DEFINE_DB_CLASS(DbTxn);
};


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Application classes
//

//
// A set of application options - define how this application uses
// the db library.
//
class _exported DbInfo : protected DB_INFO
{
    friend DbEnv;
    friend Db;

public:
    DbInfo();
    ~DbInfo();

    // Byte order.
    int	get_lorder() const;
    void set_lorder(int);

    // Underlying cache size.
    size_t get_cachesize() const;
    void set_cachesize(size_t);

    // Underlying page size.
    size_t get_pagesize() const;
    void set_pagesize(size_t);

    // Local heap allocation.
    typedef void *(*db_malloc_fcn)(size_t);
    db_malloc_fcn get_malloc() const;
    void set_malloc(db_malloc_fcn);

    ////////////////////////////////////////////////////////////////
    // Btree access method.

    // Maximum keys per page.
    int	get_bt_maxkey() const;
    void set_bt_maxkey(int);

    // Minimum keys per page.
    int	get_bt_minkey() const;
    void set_bt_minkey(int);

    // Comparison function.
    typedef int (*bt_compare_fcn)(const DBT *, const DBT *);
    bt_compare_fcn get_bt_compare() const;
    void set_bt_compare(bt_compare_fcn);

    // Prefix function.
    typedef size_t (*bt_prefix_fcn)(const DBT *, const DBT *);
    bt_prefix_fcn get_bt_prefix() const;
    void set_bt_prefix(bt_prefix_fcn);

    ////////////////////////////////////////////////////////////////
    // Hash access method.

    // Fill factor.
    u_int32_t get_h_ffactor() const;
    void set_h_ffactor(u_int32_t);

    // Number of elements.
    u_int32_t get_h_nelem() const;
    void set_h_nelem(u_int32_t);

    // Hash function.
    typedef u_int32_t (*h_hash_fcn)(const void *, u_int32_t);
    h_hash_fcn get_h_hash() const;
    void set_h_hash(h_hash_fcn);

    ////////////////////////////////////////////////////////////////
    // Recno access method.

    // Fixed-length padding byte.
    int	get_re_pad() const;
    void set_re_pad(int);

    // Variable-length delimiting byte.
    int	get_re_delim() const;
    void set_re_delim(int);

    // Length for fixed-length records.
    u_int32_t get_re_len() const;
    void set_re_len(u_int32_t);

    // Source file name.
    char *get_re_source() const;
    void set_re_source(char *);

    // Note: some flags are set as side effects of calling
    // above "set" methods.
    //
    u_int32_t get_flags() const;
    void set_flags(u_int32_t);


    // (deep) copying of this object is allowed.
    //
    DbInfo(const DbInfo &);
    DbInfo &operator = (const DbInfo &);

private:
    // We can add data to this class if needed
    // since parent class is not allocated by db.
    // (see comment at top)
};

//
// Base application class.  Provides functions for opening a database.
// User of this library can use this class as a starting point for
// developing a DB application - derive their application class from
// this one, add application control logic.
//
// Note that if you use the default constructor, you must explicitly
// call appinit() before any other db activity (e.g. opening files)
//
class _exported DbEnv : protected DB_ENV
{
friend DbTxnMgr;
friend DbLog;
friend DbLockTab;
friend DbMpool;
friend Db;

public:

    ~DbEnv();

    // This constructor can be used to immediately initialize the
    // application with these arguments.  Do not use it if you
    // need to set other parameters via the access methods.
    //
    DbEnv(const char *homeDir, char *const *db_config, u_int32_t flags);

    // Use this constructor if you wish to *delay* the initialization
    // of the db library.  This is useful if you need to set
    // any particular parameters via the access methods below.
    // Then call appinit() to complete the initialization.
    //
    DbEnv();

    // Used in conjunction with the default constructor to
    // complete the initialization of the db library.
    //
    int appinit(const char *homeDir, char *const *db_config, u_int32_t flags);

    // Called automatically when DbEnv is destroyed, or can be
    // called at any time to shut down Db.
    //
    int appexit();

    ////////////////////////////////////////////////////////////////
    // simple get/set access methods
    //
    // If you are calling set_ methods, you need to
    // use the default constructor along with appinit().

    // Byte order.
    int	get_lorder() const;
    void set_lorder(int);

    // Error message callback.
    typedef void (*db_errcall_fcn)(const char *, char *);
    db_errcall_fcn get_errcall() const;
    void set_errcall(db_errcall_fcn);

    // Error message file stream.
    FILE *get_errfile() const;
    void set_errfile(FILE *);

    // Error message prefix.
    const char *get_errpfx() const;
    void set_errpfx(const char *);

    // Generate debugging messages.
    int get_verbose() const;
    void set_verbose(int);

    ////////////////////////////////////////////////////////////////
    // User paths.

    // Database home.
    char *get_home() const;
    void set_home(char *);

    // Database log file directory.
    char *get_log_dir() const;
    void set_log_dir(char *);

    // Database tmp file directory.
    char *get_tmp_dir() const;
    void set_tmp_dir(char *);

    // Database data file directories.
    char **get_data_dir() const;
    void set_data_dir(char **);

    // Database data file slots.
    int get_data_cnt() const;
    void set_data_cnt(int);

    // Next Database data file slot.
    int get_data_next() const;
    void set_data_next(int);


    ////////////////////////////////////////////////////////////////
    // Locking.

    // Return from lock_open().
    DbLockTab *get_lk_info() const;

    // Two dimensional conflict matrix.
    u_int8_t *get_lk_conflicts() const;
    void set_lk_conflicts(u_int8_t *);

    // Number of lock modes in table.
    int get_lk_modes() const;
    void set_lk_modes(int);

    // Maximum number of locks.
    u_int32_t get_lk_max() const;
    void set_lk_max(u_int32_t);

    // Deadlock detect on every conflict.
    u_int32_t get_lk_detect() const;
    void set_lk_detect(u_int32_t);


    ////////////////////////////////////////////////////////////////
    // Logging.

    // Return from log_open().
    DbLog *get_lg_info() const;

    // Maximum file size.
    u_int32_t get_lg_max() const;
    void set_lg_max(u_int32_t);


    ////////////////////////////////////////////////////////////////
    // Memory pool.

    // Return from memp_open().
    DbMpool *get_mp_info() const;

    // Maximum file size for mmap.
    size_t get_mp_mmapsize() const;
    void set_mp_mmapsize(size_t);

    // Bytes in the mpool cache.
    size_t get_mp_size() const;
    void set_mp_size(size_t);


    ////////////////////////////////////////////////////////////////
    // Transactions.

    // Return from txn_open().
    DbTxnMgr *get_tx_info() const;

    // Maximum number of transactions.
    u_int32_t get_tx_max() const;
    void set_tx_max(u_int32_t);

    // Dispatch function for recovery.
    typedef int (*tx_recover_fcn)(DB_LOG *, DBT *, DB_LSN *, int, void *);
    tx_recover_fcn get_tx_recover() const;
    void set_tx_recover(tx_recover_fcn);

    // Flags.
    u_int32_t get_flags() const;
    void set_flags(u_int32_t);

    ////////////////////////////////////////////////////////////////
    // The default error model is to throw an exception whenever
    // an error occurs.  This generally allows for cleaner logic
    // for transaction processing, as a try block can surround a
    // single transaction.  Alternatively, since almost every method
    // returns an error code (errno), the error model can be set to
    // not throw exceptions, and instead return the appropriate code.
    //
    enum ErrorModel { Exception, ErrorReturn };
    void set_error_model(ErrorModel);
    ErrorModel get_error_model() const;

    // If an error is detected and the error call function
    // or stream is set, a message is dispatched or printed.
    // If a prefix is set, each message is prefixed.
    //
    // You can use set_errcall() or set_errfile() above to control
    // error functionality using a C model.  Alternatively, you can
    // call set_error_stream() to force all errors to a C++ stream.
    // It is unwise to mix these approaches.
    //
    class ostream* get_error_stream() const;
    void set_error_stream(class ostream*);

    // used internally
    static int runtime_error(const char *caller, int err, int in_destructor = 0);

private:
    // We can add data to this class if needed
    // since parent class is not allocated by db.
    // (see comment at top)

    // no copying
    DbEnv(const DbEnv &);
    operator = (const DbEnv &);

    ErrorModel error_model_;
    static void stream_error_function(const char *, char *);
    static ostream *error_stream_;
};

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Table access classes
//

//
// Represents a database table = a set of keys with associated values.
//
class _exported Db
{
    friend DbEnv;

public:
    int close(u_int32_t flags);
    int cursor(DbTxn *txnid, Dbc **cursorp);
    int del(DbTxn *txnid, Dbt *key, u_int32_t flags);
    int fd(int *fdp);
    int get(DbTxn *txnid, Dbt *key, Dbt *data, u_int32_t flags);
    int put(DbTxn *txnid, Dbt *key, Dbt *data, u_int32_t flags);
    int stat(void *sp, void *(*db_malloc)(size_t), u_int32_t flags);
    int sync(u_int32_t flags);

    DBTYPE get_type() const;

    static int open(const char *fname, DBTYPE type, u_int32_t flags,
                    int mode, DbEnv *dbenv, DbInfo *info, Db **dbpp);

private:
    // We can add data to this class if needed
    // since it is implemented via a pointer.
    // (see comment at top)

    // Note: use Db::open() to get initialize pointers to a Db,
    // and call Db::close() rather than delete to release them.
    Db();
    ~Db();

    // no copying
    Db(const Db &);
    Db &operator = (const Db &);

    DEFINE_DB_CLASS(Db);
};

//
// A chunk of data, maybe a key or value.
//
class _exported Dbt : private DBT
{
    friend Dbc;
    friend Db;
    friend DbLog;
    friend DbMpoolFile;
    friend DbLockTab;

public:

    // key/data
    void *get_data() const;
    void set_data(void *);

    // key/data length
    u_int32_t get_size() const;
    void set_size(u_int32_t);

    // RO: length of user buffer.
    u_int32_t get_ulen() const;
    void set_ulen(u_int32_t);

    // RO: get/put record length.
    u_int32_t get_dlen() const;
    void set_dlen(u_int32_t);

    // RO: get/put record offset.
    u_int32_t get_doff() const;
    void set_doff(u_int32_t);

    // flags
    u_int32_t get_flags() const;
    void set_flags(u_int32_t);

    Dbt(void *data, size_t size);
    Dbt();
    ~Dbt();
    Dbt(const Dbt &);
    Dbt &operator = (const Dbt &);

private:
    // We can add data to this class if needed
    // since parent class is not allocated by db.
    // (see comment at top)
};

class _exported Dbc : protected DBC
{
    friend Db;

public:
    int close();
    int del(u_int32_t flags);
    int get(Dbt* key, Dbt *data, u_int32_t flags);
    int put(Dbt* key, Dbt *data, u_int32_t flags);

private:
    // No data is permitted in this class (see comment at top)

    // Note: use Db::cursor() to get pointers to a Dbc,
    // and call Dbc::close() rather than delete to release them.
    //
    Dbc();
    ~Dbc();

    // no copying
    Dbc(const Dbc &);
    Dbc &operator = (const Dbc &);
};
#endif /* !_DB_CXX_H_ */
