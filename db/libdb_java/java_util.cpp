/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_util.cpp	10.7 (Sleepycat) 5/2/98";
#endif /* not lint */

#include "java_util.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#define sys_errlist _sys_errlist
#define sys_nerr _sys_nerr
#endif

/****************************************************************
 *
 * Utility functions used by "glue" functions.
 *
 */

/* Use our own malloc for any objects allocated via DB_DBT_MALLOC,
 * since we must free them in the same library address space.
 */
extern "C"
void * java_db_malloc(size_t size)
{
    return malloc(size);
}

/* Get the private data from a Db* object as a (64 bit) java long.
 */
jlong get_private_long_info(JNIEnv *jnienv, const char *classname,
                            jobject obj)
{
    if (!obj)
        return 0;

    jclass dbClass = get_class(jnienv, classname);
    jfieldID id = jnienv->GetFieldID(dbClass, "private_info_", "J");
    return jnienv->GetLongField(obj, id);
}

/* Get the private data from a Db* object.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void *get_private_info(JNIEnv *jnienv, const char *classname,
                       jobject obj)
{
    long_to_ptr lp;
    if (!obj)
        return 0;
    lp.java_long = get_private_long_info(jnienv, classname, obj);
    return lp.ptr;
}

/* Set the private data in a Db* object as a (64 bit) java long.
 */
void set_private_long_info(JNIEnv *jnienv, const char *classname,
                           jobject obj, jlong value)
{
    jclass dbClass = get_class(jnienv, classname);
    jfieldID id = jnienv->GetFieldID(dbClass, "private_info_", "J");
    jnienv->SetLongField(obj, id, value);
}

/* Set the private data in a Db* object.
 * The private data is stored in the object as a Java long (64 bits),
 * which is long enough to store a pointer on current architectures.
 */
void set_private_info(JNIEnv *jnienv, const char *classname,
                      jobject obj, void *value)
{
    long_to_ptr lp;
    lp.java_long = 0;
    lp.ptr = value;
    set_private_long_info(jnienv, classname, obj, lp.java_long);
}

/*
 * Given a non-qualified name (e.g. "foo"), get the class handl
 * for the fully qualified name (e.g. "com.sleepycat.db.foo")
 */
jclass get_class(JNIEnv *jnienv, const char *classname)
{
    // TODO: cache these (remember to use NewGlobalRef)
    char fullname[128] = DB_PACKAGE_NAME;
    strcat(fullname, classname);
    return jnienv->FindClass(fullname);
}

/* Set an individual field in a Db* object.
 * The field must be an object type.
 */
void set_object_field(JNIEnv *jnienv, jclass class_of_this,
                      jobject jthis, const char *object_classname,
                      const char *name_of_field, jobject obj)
{
    char signature[512];

    strcpy(signature, "L");
    strcat(signature, DB_PACKAGE_NAME);
    strcat(signature, object_classname);
    strcat(signature, ";");

    jfieldID id  = jnienv->GetFieldID(class_of_this, name_of_field, signature);
    jnienv->SetObjectField(jthis, id, obj);
}


/* Report an exception back to the java side.
 */
void report_exception(JNIEnv *jnienv, const char *text, int err)
{
    jstring textString = get_java_string(jnienv, text);
    jclass dbexcept = get_class(jnienv, name_DB_EXCEPTION);
    jmethodID constructId = jnienv->GetMethodID(dbexcept, "<init>",
                                             "(Ljava/lang/String;I)V");
    jthrowable obj = (jthrowable)jnienv->AllocObject(dbexcept);
    jnienv->CallVoidMethod(obj, constructId, textString, err);
    jnienv->Throw(obj);
}

/* If the object is null, report an exception and return false (0),
 * otherwise return true (1).
 */
int verify_non_null(JNIEnv *jnienv, void *obj)
{
    if (obj == NULL) {
        report_exception(jnienv, "null object", 0);
        return 0;
    }
    return 1;
}

/* If the error code is non-zero, report an exception and return false (0),
 * otherwise return true (1).
 */
int verify_return(JNIEnv *jnienv, int err)
{
    char *errstring;

    if (err != 0) {
        if (err > 0 && (errstring = strerror(err)) != NULL) {
            report_exception(jnienv, errstring, err);
        }
        else {
            char buffer[64];
            sprintf(buffer, "error %d", err);
            report_exception(jnienv, buffer, err);
        }
        return 0;
    }
    return 1;
}

/* Create an object of the given class, calling its default constructor.
 */
jobject create_default_object(JNIEnv *jnienv, const char *class_name)
{
    jclass dbclass = get_class(jnienv, class_name);
    jmethodID id = jnienv->GetMethodID(dbclass, "<init>", "()V");
    jobject object = jnienv->NewObject(dbclass, id);
    return object;
}

/* Convert an DB object to a Java encapsulation of that object.
 * Note: This implementation creates a new Java object on each call,
 * so it is generally useful when a new DB object has just been created.
 */
jobject convert_object(JNIEnv *jnienv, const char *class_name, void *dbobj)
{
    if (!dbobj)
        return 0;

    jobject jo = create_default_object(jnienv, class_name);
    set_private_info(jnienv, class_name, jo, dbobj);
    return jo;
}

/* Create a copy of the string
 */
char *dup_string(const char *str)
{
    char *retval = NEW_ARRAY(char, strlen(str)+1);
    strcpy(retval, str);
    return retval;
}

/* Create a java string from the given string
 */
jstring get_java_string(JNIEnv *jnienv, const char* string)
{
    if (string == 0)
        return 0;
    return jnienv->NewStringUTF(string);
}

/* Convert a java object to the various C pointers they represent.
 */
DB *get_DB(JNIEnv *jnienv, jobject obj)
{
    return (DB *)get_private_info(jnienv, name_DB, obj);
}

DB_BTREE_STAT *get_DB_BTREE_STAT(JNIEnv *jnienv, jobject obj)
{
    return (DB_BTREE_STAT *)get_private_info(jnienv, name_DB_BTREE_STAT, obj);
}

DBC *get_DBC(JNIEnv *jnienv, jobject obj)
{
    return (DBC *)get_private_info(jnienv, name_DBC, obj);
}

DB_ENV *get_DB_ENV(JNIEnv *jnienv, jobject obj)
{
    return (DB_ENV *)get_private_info(jnienv, name_DB_ENV, obj);
}

DB_INFO *get_DB_INFO(JNIEnv *jnienv, jobject obj)
{
    return (DB_INFO *)get_private_info(jnienv, name_DB_INFO, obj);
}

// A DB_LOCK is just a size_t, so no need for indirection.
DB_LOCK get_DB_LOCK(JNIEnv *jnienv, jobject obj)
{
    return (DB_LOCK)get_private_long_info(jnienv, name_DB_LOCK, obj);
}

DB_LOCKTAB *get_DB_LOCKTAB(JNIEnv *jnienv, jobject obj)
{
    return (DB_LOCKTAB *)get_private_info(jnienv, name_DB_LOCKTAB, obj);
}

DB_LOG *get_DB_LOG(JNIEnv *jnienv, jobject obj)
{
    return (DB_LOG *)get_private_info(jnienv, name_DB_LOG, obj);
}

DB_LOG_STAT *get_DB_LOG_STAT(JNIEnv *jnienv, jobject obj)
{
    return (DB_LOG_STAT *)get_private_info(jnienv, name_DB_LOG_STAT, obj);
}

DB_LSN *get_DB_LSN(JNIEnv *jnienv, jobject obj)
{
    return (DB_LSN *)get_private_info(jnienv, name_DB_LSN, obj);
}

DB_MPOOL *get_DB_MPOOL(JNIEnv *jnienv, jobject obj)
{
    return (DB_MPOOL *)get_private_info(jnienv, name_DB_MPOOL, obj);
}

DB_MPOOL_FSTAT *get_DB_MPOOL_FSTAT(JNIEnv *jnienv, jobject obj)
{
    return (DB_MPOOL_FSTAT *)get_private_info(jnienv, name_DB_MPOOL_FSTAT, obj);
}

DB_MPOOL_STAT *get_DB_MPOOL_STAT(JNIEnv *jnienv, jobject obj)
{
    return (DB_MPOOL_STAT *)get_private_info(jnienv, name_DB_MPOOL_STAT, obj);
}

DB_TXN *get_DB_TXN(JNIEnv *jnienv, jobject obj)
{
    return (DB_TXN *)get_private_info(jnienv, name_DB_TXN, obj);
}

DB_TXNMGR *get_DB_TXNMGR(JNIEnv *jnienv, jobject obj)
{
    return (DB_TXNMGR *)get_private_info(jnienv, name_DB_TXNMGR, obj);
}

DB_TXN_STAT *get_DB_TXN_STAT(JNIEnv *jnienv, jobject obj)
{
    return (DB_TXN_STAT *)get_private_info(jnienv, name_DB_TXN_STAT, obj);
}

DBT_info *get_DBT(JNIEnv *jnienv, jobject obj)
{
    return (DBT_info *)get_private_info(jnienv, name_DBT, obj);
}


/* Convert a C pointer to the various Java objects they represent.
 */
jobject get_DbBtreeStat(JNIEnv *jnienv, DB_BTREE_STAT *dbobj)
{
    return convert_object(jnienv, name_DB_BTREE_STAT, dbobj);
}

jobject get_Dbc(JNIEnv *jnienv, DBC *dbobj)
{
    return convert_object(jnienv, name_DBC, dbobj);
}

jobject get_DbLockTab(JNIEnv *jnienv, DB_LOCKTAB *dbobj)
{
    return convert_object(jnienv, name_DB_LOCKTAB, dbobj);
}

jobject get_DbLog(JNIEnv *jnienv, DB_LOG *dbobj)
{
    return convert_object(jnienv, name_DB_LOG, dbobj);
}

jobject get_DbLogStat(JNIEnv *jnienv, DB_LOG_STAT *dbobj)
{
    return convert_object(jnienv, name_DB_LOG_STAT, dbobj);
}

// LSNs are different since they are really normally
// treated as by-value objects.  We actually create
// a pointer to the LSN as store that, deleting it
// when the LSN is GC'd.
//
jobject get_DbLsn(JNIEnv *jnienv, DB_LSN dbobj)
{
    DB_LSN *lsnp = NEW(DB_LSN);
    *lsnp = dbobj;
    return convert_object(jnienv, name_DB_LSN, lsnp);
}

jobject get_DbMpool(JNIEnv *jnienv, DB_MPOOL *dbobj)
{
    return convert_object(jnienv, name_DB_MPOOL, dbobj);
}

jobject get_DbMpoolFStat(JNIEnv *jnienv, DB_MPOOL_FSTAT *dbobj)
{
    return convert_object(jnienv, name_DB_MPOOL_FSTAT, dbobj);
}

jobject get_DbMpoolStat(JNIEnv *jnienv, DB_MPOOL_STAT *dbobj)
{
    return convert_object(jnienv, name_DB_MPOOL_STAT, dbobj);
}

jobject get_DbTxn(JNIEnv *jnienv, DB_TXN *dbobj)
{
    return convert_object(jnienv, name_DB_TXN, dbobj);
}

jobject get_DbTxnMgr(JNIEnv *jnienv, DB_TXNMGR *dbobj)
{
    return convert_object(jnienv, name_DB_TXNMGR, dbobj);
}

jobject get_DbTxnStat(JNIEnv *jnienv, DB_TXN_STAT *dbobj)
{
    return convert_object(jnienv, name_DB_TXN_STAT, dbobj);
}

/****************************************************************
 *
 * Implementation of class DBT_info
 *
 */
DBT_info::DBT_info()
:   array_(0)
,   offset_(0)
{
    memset((DBT*)this, 0, sizeof(DBT));
}

void DBT_info::release(JNIEnv *jnienv)
{
    if (array_ != 0) {
        jnienv->DeleteGlobalRef(array_);
    }
}

DBT_info::~DBT_info()
{
}

/****************************************************************
 *
 * Implementation of class LockedDBT
 *
 */
LockedDBT::LockedDBT(JNIEnv *jnienv, jobject obj, int is_retrieve_op)
    : env_(jnienv)
    , obj_(obj)
    , has_error_(0)
    , is_retrieve_op_(is_retrieve_op)
    , java_array_len_(0)
    , java_data_(0)
    /* dbt initialized below */
{
    dbt = (DBT_info *)get_private_info(jnienv, name_DBT, obj);
    if (!verify_non_null(jnienv, dbt)) {
        has_error_ = 1;
        return;
    }

    if ((dbt->flags & DB_DBT_USERMEM) || !is_retrieve_op_) {

        // If writing with DB_DBT_USERMEM or reading, then the data
        // should point the java array.
        //
        if (!dbt->array_) {
            report_exception(jnienv, "Dbt.data is null", 0);
            has_error_ = 1;
            return;
        }

        // Verify other parameters
        //
        java_array_len_ = jnienv->GetArrayLength(dbt->array_);
        if (dbt->offset_ < 0 ) {
            report_exception(jnienv, "Dbt.offset illegal", 0);
            has_error_ = 1;
            return;
        }
        if (dbt->ulen + dbt->offset_ > java_array_len_) {
            report_exception(jnienv,
                           "Dbt.ulen + Dbt.offset greater than array length", 0);
            has_error_ = 1;
            return;
        }

        java_data_ = jnienv->GetByteArrayElements(dbt->array_, NULL);
        dbt->data = java_data_ + dbt->offset_;
    }
    else {

        // If writing with DB_DBT_MALLOC, then the data is
        // allocated by DB.
        //
        dbt->data = 0;
    }
}

LockedDBT::~LockedDBT()
{
    // If there was an error in the constructor,
    // everything is already cleaned up.
    //
    if (has_error_)
        return;

    if ((dbt->flags & DB_DBT_USERMEM) || !is_retrieve_op_) {

        // If writing with DB_DBT_USERMEM or reading, then the data
        // may be already in the java array, in which case,
        // we just need to release it.
        // If DB didn't put it in the array (indicated by the
        // dbt->data changing), we need to do that
        //
        jbyte *data = (jbyte *)dbt->data;
        data -= dbt->offset_;
        if (data != java_data_) {
            env_->SetByteArrayRegion(dbt->array_, dbt->offset_, dbt->ulen, data);
        }
        env_->ReleaseByteArrayElements(dbt->array_, java_data_, 0);
        dbt->data = 0;
    }
    else {

        // If writing with DB_DBT_MALLOC, then the data was allocated
        // by DB.  If dbt->data is zero, it means an error occurred
        // (and should have been already reported).
        //
        if (dbt->data) {

            // Release any old references.
            //
            dbt->release(env_);

            dbt->array_ = (jbyteArray)
                env_->NewGlobalRef(env_->NewByteArray(dbt->size));
            dbt->offset_ = 0;
            env_->SetByteArrayRegion(dbt->array_, 0, dbt->size, (jbyte *)dbt->data);
            free(dbt->data);
            dbt->data = 0;
        }
    }
}

/****************************************************************
 *
 * Implementation of class LockedString
 *
 */
LockedString::LockedString(JNIEnv *jnienv, jstring jstr)
    : env_(jnienv)
    , jstr_(jstr)
{
    if (jstr == 0)
        string = 0;
    else
        string = jnienv->GetStringUTFChars(jstr, NULL);
}

LockedString::~LockedString()
{
    if (jstr_)
        env_->ReleaseStringUTFChars(jstr_, string);
}


/****************************************************************
 *
 * Implementation of class LockedStringArray
 *
 */
LockedStringArray::LockedStringArray(JNIEnv *jnienv, jobjectArray arr)
    : env_(jnienv)
    , arr_(arr)
    , string_array(0)
{
    typedef const char *conststr;

    if (arr != 0) {
        int count = jnienv->GetArrayLength(arr);
        string_array = NEW_ARRAY(conststr, count+1);
        for (int i=0; i<count; i++) {
            jstring jstr = (jstring)jnienv->GetObjectArrayElement(arr, i);
            if (jstr == 0) {
                //
                // An embedded null in the string array is treated
                // as an endpoint.
                //
                string_array[i] = 0;
                break;
            }
            else {
                string_array[i] = jnienv->GetStringUTFChars(jstr, NULL);
            }
        }
        string_array[count] = 0;
    }
}

LockedStringArray::~LockedStringArray()
{
    if (arr_) {
        int count = env_->GetArrayLength(arr_);
        for (int i=0; i<count; i++) {
            if (string_array[i] == 0)
                break;
            jstring jstr = (jstring)env_->GetObjectArrayElement(arr_, i);
            env_->ReleaseStringUTFChars(jstr, string_array[i]);
        }
        DELETE(string_array);
    }
}
