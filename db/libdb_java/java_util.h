/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)java_util.h	10.6 (Sleepycat) 5/2/98
 */

#ifndef _JAVA_UTIL_H_
#define _JAVA_UTIL_H_

#include "db.h"
#include <jni.h>
#include <string.h>             // needed for memset

#ifdef _MSC_VER

// These are level 4 warnings that are explicitly disabled.
// With Visual C++, by default you do not see above level 3 unless
// you use /W4.  But we like to compile with the highest level
// warnings to catch other errors.
//
// 4201: nameless struct/union
//       triggered by standard include file <winnt.h>
//
// 4244: '=' : convert from '__int64' to 'unsigned int', possible loss of data
//       results from making size_t data members correspond to jlongs
//
// 4514: unreferenced inline function has been removed
//       jni.h defines methods that are not called
//
// 4127: conditional expression is constant
//       occurs because of arg in JAVADB_RW_ACCESS_STRING macro
//
// 4100: unreferenced formal parameters: TODO: remove after everythings done
//
#pragma warning(disable: 4100 4244 4201 4514 4127)

#endif

#define DB_PACKAGE_NAME "com/sleepycat/db/"

/* Union to convert longs to pointers (see {get,set}_private_info).
 */
typedef union {
    jlong java_long;
    void *ptr;
} long_to_ptr;

/****************************************************************
 *
 * DBT_info and LockedDBT classes
 *
 * A DBT_info is created whenever a Dbt (java) object is created,
 * and a pointer to it is stored in its private info storage.
 * It is subclassed from DBT, because we must retain some extra
 * information in it while it is in use.  In particular, when
 * a java array is associated with it, we need to keep a Globally
 * Locked reference to it so it is not GC'd.  This reference is
 * released when the Dbt is GC'd.
 *
 * In contrast, a LockedDBT class is only in existence during a
 * single native call to the DB API.  Its constructor's job is
 * to temporarily convert any java array found in the DBT_info
 * to actual bytes in memory that remain locked in place.  These
 * bytes are used during the call to the underlying DB C layer,
 * and are released and/or copied back by the destructor.
 * Thus, a LockedDBT must be declared as a stack object to
 * function properly.
 */

/*
 *
 * Declaration of class DBT_info
 *
 * See description above.
 */
class DBT_info : public DBT
{
public:
    DBT_info();
    ~DBT_info();
    void release(JNIEnv *jnienv);

    jbyteArray array_;
    int offset_;
};

/*
 *
 * Declaration of class LockedDBT
 *
 * See description above.
 */
class LockedDBT
{
public:
    // After the constructor returns, if has_error() is false,
    // then dbt must be initialized.
    //
    LockedDBT(JNIEnv *jnienv, jobject obj, int is_retrieve_op);
    ~LockedDBT();
    int has_error()     { return has_error_; }

public:
    DBT_info *dbt;
    int java_array_len_;

private:
    JNIEnv *env_;
    jobject obj_;
    jbyte *java_data_;
    int has_error_;
    int is_retrieve_op_;
};

/****************************************************************
 *
 * Declaration of class LockedString
 *
 * Given a java jstring object, this gets an encapsulated
 * const char *.  When the LockedString object is destroyed, the
 * char * array is released.
 */
class LockedString
{
public:
    LockedString(JNIEnv *jnienv, jstring jstr);
    ~LockedString();

public:
    const char *string;
private:
    JNIEnv *env_;
    jstring jstr_;
};

/****************************************************************
 *
 * Declaration of class LockedStringArray
 *
 * Given a java jobjectArray object (that must be a String[]),
 * we extract the individual strings and build a const char **
 * When the LockedStringArray object is destroyed, the individual
 * strings are released.
 */
class LockedStringArray
{
public:
    LockedStringArray(JNIEnv *jnienv, jobjectArray arr);
    ~LockedStringArray();

public:
    const char **string_array;
private:
    JNIEnv *env_;
    jobjectArray arr_;
};

/****************************************************************
 *
 * Utility functions and definitions used by "glue" functions.
 *
 */

#define NOT_IMPLEMENTED(str) \
	report_exception(jnienv, str /*concatenate*/ ": not implemented", 0)

/* Use our own malloc for any objects allocated via DB_DBT_MALLOC,
 * since we must free them in the same library address space.
 */
extern "C"
void * java_db_malloc(size_t size);

/* Get the private data from a Db* object as a (64 bit) java long.
 */
jlong get_private_long_info(JNIEnv *jnienv, const char *classname,
                            jobject obj);

/* Get the private data from a Db* object.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void *get_private_info(JNIEnv *jnienv, const char *classname,
                       jobject obj);

/* Set the private data in a Db* object as a (64 bit) java long.
 */
void set_private_long_info(JNIEnv *jnienv, const char *classname,
                           jobject obj, jlong value);

/* Set the private data in a Db* object.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void set_private_info(JNIEnv *jnienv, const char *classname,
                      jobject obj, void *value);

/*
 * Given a non-qualified name (e.g. "foo"), get the class handl
 * for the fully qualified name (e.g. "com.sleepycat.db.foo")
 */
jclass get_class(JNIEnv *jnienv, const char *classname);

/* Set an individual field in a Db* object.
 * The field must be an object type.
 */
void set_object_field(JNIEnv *jnienv, jclass class_of_this,
                      jobject jthis, const char *object_classname,
                      const char *name_of_field, jobject obj);

/* Report an exception back to the java side.
 */
void report_exception(JNIEnv *jnienv, const char *text, int err);

/* If the object is null, report an exception and return false (0),
 * otherwise return true (1).
 */
int verify_non_null(JNIEnv *jnienv, void *obj);

/* If the error code is non-zero, report an exception and return false (0),
 * otherwise return true (1).
 */
int verify_return(JNIEnv *jnienv, int err);

/* Create an object of the given class, calling its default constructor.
 */
jobject create_default_object(JNIEnv *jnienv, const char *class_name);

/* Convert an DB object to a Java encapsulation of that object.
 * Note: This implementation creates a new Java object on each call,
 * so it is generally useful when a new DB object has just been created.
 */
jobject convert_object(JNIEnv *jnienv, const char *class_name, void *dbobj);

/* Create a copy of the string
 */
char *dup_string(const char *str);

/* Create a java string from the given string
 */
jstring get_java_string(JNIEnv *jnienv, const char* string);


/* Convert a java object to the various C pointers they represent.
 */
DB             *get_DB            (JNIEnv *jnienv, jobject obj);
DB_BTREE_STAT  *get_DB_BTREE_STAT (JNIEnv *jnienv, jobject obj);
DBC            *get_DBC           (JNIEnv *jnienv, jobject obj);
DB_ENV         *get_DB_ENV        (JNIEnv *jnienv, jobject obj);
DB_INFO        *get_DB_INFO       (JNIEnv *jnienv, jobject obj);
DB_LOCK         get_DB_LOCK       (JNIEnv *jnienv, jobject obj); // not a ptr
DB_LOCKTAB     *get_DB_LOCKTAB    (JNIEnv *jnienv, jobject obj);
DB_LOG         *get_DB_LOG        (JNIEnv *jnienv, jobject obj);
DB_LOG_STAT    *get_DB_LOG_STAT   (JNIEnv *jnienv, jobject obj);
DB_LSN         *get_DB_LSN        (JNIEnv *jnienv, jobject obj);
DB_MPOOL       *get_DB_MPOOL      (JNIEnv *jnienv, jobject obj);
DB_MPOOL_FSTAT *get_DB_MPOOL_FSTAT(JNIEnv *jnienv, jobject obj);
DB_MPOOL_STAT  *get_DB_MPOOL_STAT (JNIEnv *jnienv, jobject obj);
DB_TXN         *get_DB_TXN        (JNIEnv *jnienv, jobject obj);
DB_TXNMGR      *get_DB_TXNMGR     (JNIEnv *jnienv, jobject obj);
DB_TXN_STAT    *get_DB_TXN_STAT   (JNIEnv *jnienv, jobject obj);
DBT_info       *get_DBT           (JNIEnv *jnienv, jobject obj);

/* Convert a C object to the various java pointers they represent.
 */
jobject get_DbBtreeStat  (JNIEnv *jnienv, DB_BTREE_STAT *dbobj);
jobject get_Dbc          (JNIEnv *jnienv, DBC *dbobj);
jobject get_DbLockTab    (JNIEnv *jnienv, DB_LOCKTAB *dbobj);
jobject get_DbLog        (JNIEnv *jnienv, DB_LOG *dbobj);
jobject get_DbLogStat    (JNIEnv *jnienv, DB_LOG_STAT *dbobj);
jobject get_DbLsn        (JNIEnv *jnienv, DB_LSN dbobj);
jobject get_DbMpool      (JNIEnv *jnienv, DB_MPOOL *dbobj);
jobject get_DbMpoolStat  (JNIEnv *jnienv, DB_MPOOL_STAT *dbobj);
jobject get_DbMpoolFStat (JNIEnv *jnienv, DB_MPOOL_FSTAT *dbobj);
jobject get_DbTxn        (JNIEnv *jnienv, DB_TXN *dbobj);
jobject get_DbTxnMgr     (JNIEnv *jnienv, DB_TXNMGR *dbobj);
jobject get_DbTxnStat    (JNIEnv *jnienv, DB_TXN_STAT *dbobj);

// The java names of DB classes
const char * const name_DB             = "Db";
const char * const name_DB_BTREE_STAT  = "DbBtreeStat";
const char * const name_DBC            = "Dbc";
const char * const name_DB_ENV         = "DbEnv";
const char * const name_DB_EXCEPTION   = "DbException";
const char * const name_DB_INFO        = "DbInfo";
const char * const name_DB_LOCK        = "DbLock";
const char * const name_DB_LOCKTAB     = "DbLockTab";
const char * const name_DB_LOG         = "DbLog";
const char * const name_DB_LOG_STAT    = "DbLogStat";
const char * const name_DB_LSN         = "DbLsn";
const char * const name_DB_MPOOL       = "DbMpool";
const char * const name_DB_MPOOL_FSTAT = "DbMpoolFStat";
const char * const name_DB_MPOOL_STAT  = "DbMpoolStat";
const char * const name_DBT            = "Dbt";
const char * const name_DB_TXN         = "DbTxn";
const char * const name_DB_TXNMGR      = "DbTxnMgr";
const char * const name_DB_TXN_STAT    = "DbTxnStat";
const char * const name_DbErrcall      = "DbErrcall";

#define JAVADB_RO_ACCESS(j_class, j_fieldtype, j_field, c_type, c_field)    \
extern "C" JNIEXPORT j_fieldtype JNICALL                                    \
  Java_com_sleepycat_db_##j_class##_get_1##j_field                          \
  (JNIEnv *jnienv, jobject jthis)                                           \
{                                                                           \
    c_type *db_this = get_##c_type(jnienv, jthis);                          \
                                                                            \
    if (verify_non_null(jnienv, db_this)) {                                 \
        return db_this->c_field;                                            \
    }                                                                       \
    return 0;                                                               \
}

#define JAVADB_WO_ACCESS(j_class, j_fieldtype, j_field, c_type, c_field)    \
extern "C" JNIEXPORT void JNICALL                                           \
  Java_com_sleepycat_db_##j_class##_set_1##j_field                          \
  (JNIEnv *jnienv, jobject jthis, j_fieldtype value)                        \
{                                                                           \
    c_type *db_this = get_##c_type(jnienv, jthis);                          \
                                                                            \
    if (verify_non_null(jnienv, db_this)) {                                 \
        db_this->c_field = value;                                           \
    }                                                                       \
}

#define JAVADB_RW_ACCESS(j_class, j_fieldtype, j_field, c_type, c_field)    \
        JAVADB_RO_ACCESS(j_class, j_fieldtype, j_field, c_type, c_field)    \
        JAVADB_WO_ACCESS(j_class, j_fieldtype, j_field, c_type, c_field)

#define JAVADB_RO_ACCESS_STRING(j_class, j_field, c_type, c_field, dealloc) \
extern "C" JNIEXPORT jstring JNICALL                                        \
  Java_com_sleepycat_db_##j_class##_get_1##j_field                          \
  (JNIEnv *jnienv, jobject jthis)                                           \
{                                                                           \
    c_type *db_this = get_##c_type(jnienv, jthis);                          \
                                                                            \
    if (verify_non_null(jnienv, db_this)) {                                 \
        return get_java_string(jnienv, db_this->c_field);                   \
    }                                                                       \
    return 0;                                                               \
}

#define JAVADB_WO_ACCESS_STRING(j_class, j_field, c_type, c_field, dealloc) \
extern "C" JNIEXPORT void JNICALL                                           \
  Java_com_sleepycat_db_##j_class##_set_1##j_field                          \
  (JNIEnv *jnienv, jobject jthis, jstring value)                            \
{                                                                           \
    c_type *db_this = get_##c_type(jnienv, jthis);                          \
                                                                            \
    if (verify_non_null(jnienv, db_this)) {                                 \
        if (dealloc && db_this->c_field)                                    \
            delete (char*)db_this->c_field;                                 \
        if (value)                                                          \
            db_this->c_field =                                              \
                dup_string(jnienv->GetStringUTFChars(value, NULL));         \
        else                                                                \
            db_this->c_field = 0;                                           \
    }                                                                       \
}

#define JAVADB_RW_ACCESS_STRING(j_class, j_field, c_type, c_field, dealloc) \
        JAVADB_RO_ACCESS_STRING(j_class, j_field, c_type, c_field, dealloc) \
        JAVADB_WO_ACCESS_STRING(j_class, j_field, c_type, c_field, dealloc)


// Replacements for C++ new and delete.
// Our experience with Sparc/gcc shared libraries is that new/delete
// does not work correctly, so we use malloc.
// Obviously, these macros are quite limited and do not accept
// constructor args.  But our use of new/delete is also quite
// limited in this library.
//
#undef NEW
#undef NEW_ARRAY
#undef DELETE
#undef DELETE_ARRAY

#ifdef unix
//
// currently NEW zeros (to prevent common bugs), NEW_ARRAY does not,
// since it is almost always used for string creation.
//
#define NEW(type)		((type*)memset(malloc(sizeof(type)), 0, sizeof(type)))
#define NEW_ARRAY(type,n)	((type*)malloc(sizeof(type)*n))
#define DELETE(p)		(free(p))
#define DELETE_ARRAY(p)		(free(p))
#else
#define NEW(type)		new type
#define NEW_ARRAY(type,n)	new type[n]
#define DELETE(p)		delete p
#define DELETE_ARRAY(p)		delete [] p
#endif

#endif /* !_JAVA_UTIL_H_ */
