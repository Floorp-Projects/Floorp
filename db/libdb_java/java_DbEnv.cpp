/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_DbEnv.cpp	10.4 (Sleepycat) 4/10/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_DbEnv.h"

// XXX TODO:
// A prefix_info is allocated and stuffed into the db_errpfx for
// every DB_ENV created.  It holds a little extra info that is
// needed to resurrect the environment to make a callback.
// Strictly speaking, we only need this stuffed in if db_errfcn
// is set, but it's easier to always allocate it in the constructor
// and know it's always there.
//
// Note: We assume the following:
// We are the only one fiddling with the contents of db_errpfx and
// db_errcall.  In particular, db_errpfx and db_errcall are not
// initialized by appinit to some default value.
//
struct prefix_info
{
    JNIEnv *jnienv;
    char *orig_prefix;
    jobject dberrcall;
};

static void java_errcall_callback(const char *prefix, char *message)
{
    prefix_info *pi = (prefix_info *)prefix;

    // Note: these error cases are "impossible", and would
    // normally warrant an exception.  However, without
    // a jnienv, we cannot throw an exception...
    // We don't want to trap or exit, since the point of
    // this facility is for the user to completely control
    // error situations.
    //
    // TODO: find another way to indicate this error.
    //
    if (!pi)
        return;                 // These are really fatal asserts

    if (!pi->dberrcall)
        return;                 // These are really fatal asserts

    if (!pi->jnienv)
        return;                 // These are really fatal asserts

    jstring pre = get_java_string(pi->jnienv, pi->orig_prefix);
    jstring msg = get_java_string(pi->jnienv, message);
    jclass errcall_class = get_class(pi->jnienv, name_DbErrcall);
    jmethodID id = pi->jnienv->GetMethodID(errcall_class,
                           "errcall",
                           "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!id)
        return;

    pi->jnienv->CallVoidMethod(pi->dberrcall, id, pre, msg);
}

JAVADB_RW_ACCESS(DbEnv, jint, lorder, DB_ENV, db_lorder)
JAVADB_RW_ACCESS(DbEnv, jint, verbose, DB_ENV, db_verbose)
JAVADB_RW_ACCESS_STRING(DbEnv, home, DB_ENV, db_home, 0)
JAVADB_RW_ACCESS_STRING(DbEnv, log_1dir, DB_ENV, db_log_dir, 0)
JAVADB_RW_ACCESS_STRING(DbEnv, tmp_1dir, DB_ENV, db_tmp_dir, 0)
JAVADB_RW_ACCESS(DbEnv, jint, lk_1modes, DB_ENV, lk_modes)
JAVADB_RW_ACCESS(DbEnv, jint, lk_1max, DB_ENV, lk_max)
JAVADB_RW_ACCESS(DbEnv, jint, lk_1detect, DB_ENV, lk_detect)
JAVADB_RW_ACCESS(DbEnv, jint, data_1cnt, DB_ENV, data_cnt)
JAVADB_RW_ACCESS(DbEnv, jint, data_1next, DB_ENV, data_next)
JAVADB_RW_ACCESS(DbEnv, jint, lg_1max, DB_ENV, lg_max)
JAVADB_RW_ACCESS(DbEnv, jlong, mp_1mmapsize, DB_ENV, mp_mmapsize)
JAVADB_RW_ACCESS(DbEnv, jlong, mp_1size, DB_ENV, mp_size)
JAVADB_RW_ACCESS(DbEnv, jint, tx_1max, DB_ENV, tx_max)
JAVADB_RW_ACCESS(DbEnv, jint, flags, DB_ENV, flags)

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_init
  (JNIEnv *jnienv, jobject jthis)
{
    DB_ENV *dbenv = NEW(DB_ENV);
    memset(dbenv, 0, sizeof(DB_ENV));
    prefix_info *pi = NEW(prefix_info);
    pi->jnienv = 0;
    pi->orig_prefix = 0;
    pi->dberrcall = 0;
    dbenv->db_errpfx = (const char*)pi;
    set_private_info(jnienv, name_DB_ENV, jthis, dbenv);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_appinit
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis, jstring homeDir,
   jobjectArray /*String[]*/ db_config, jint flags)
{
    int err;
    DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);
    LockedString dbhomeDir(jnienv, homeDir);
    LockedStringArray db_db_config(jnienv, db_config);

    // Always turn on the thread flag for use with java.
    flags |= DB_THREAD;

    err = db_appinit(dbhomeDir.string,
                     (char *const *)db_db_config.string_array,
                     dbenv, flags);
    if (verify_return(jnienv, err)) {
        jclass dbenvClass = get_class(jnienv, name_DB_ENV);
        jobject obj;
        if (dbenv->lk_info) {
            obj = get_DbLockTab(jnienv, dbenv->lk_info);
            set_object_field(jnienv, dbenvClass, jthis, name_DB_LOCKTAB,
                             "lk_info_", obj);
        }
        if (dbenv->lg_info) {
            obj = get_DbLog(jnienv, dbenv->lg_info);
            set_object_field(jnienv, dbenvClass, jthis, name_DB_LOG,
                             "lg_info_", obj);
        }
        if (dbenv->mp_info) {
            obj = get_DbMpool(jnienv, dbenv->mp_info);
            set_object_field(jnienv, dbenvClass, jthis, name_DB_MPOOL,
                             "mp_info_", obj);
        }
        if (dbenv->tx_info) {
            obj = get_DbTxnMgr(jnienv, dbenv->tx_info);
            set_object_field(jnienv, dbenvClass, jthis, name_DB_TXNMGR,
                             "tx_info_", obj);
        }
    }
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_appexit
  (JNIEnv *jnienv, /*DbEnv*/ jobject jthis)
{
    int err;
    DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);

    if (dbenv) {
        err = db_appexit(dbenv);
        set_private_info(jnienv, name_DB_ENV, jthis, 0);
        verify_return(jnienv, err);
    }
}


// Make a copy of the lk_conflict array to pass back to the user
// (different semantics from C/C++, where the user gets a real memory
// array that they can manipulate.
//
JNIEXPORT jobjectArray JNICALL Java_com_sleepycat_db_DbEnv_get_1lk_1conflicts
  (JNIEnv *jnienv, jobject jthis)
{
    DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);

    if (!verify_non_null(jnienv, dbenv))
        return 0;

    // create array of array of bytes
    jclass bytearray_class = jnienv->FindClass("[B");
    int len = dbenv->lk_modes;
    jobjectArray array = jnienv->NewObjectArray(len, bytearray_class, 0);
    for (int i=0; i<len; i++) {
        jbyteArray subArray = jnienv->NewByteArray(len);
        jnienv->SetByteArrayRegion(subArray, 0, len,
                                   (jbyte *)&dbenv->lk_conflicts[i*len]);
    }
    return array;
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_set_1lk_1conflicts
  (JNIEnv *jnienv, jobject jthis, jobjectArray array)
{
    DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);
    static const char * const array_length_msg =
        "array length does not match lk_modes";

    if (!verify_non_null(jnienv, dbenv))
        return;

    int len = dbenv->lk_modes;
    jsize jlen = jnienv->GetArrayLength(array);
    if (jlen != len) {
        report_exception(jnienv, array_length_msg, 0);
        return;
    }

    // TODO: possibly delete old contents, and delete this array
    // in destructor.  How to distinguish from "default" value?
    //
    dbenv->lk_conflicts = NEW_ARRAY(unsigned char, len * len);

    for (int i=0; i<len; i++) {
        jobject subArray = jnienv->GetObjectArrayElement(array, i);
        jnienv->GetByteArrayRegion((jbyteArray)subArray, 0, len,
                                   (jbyte *)&dbenv->lk_conflicts[i*len]);
    }
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbEnv_get_1version_1major
  (JNIEnv *jnienv, jclass /*this_class*/)
{
    return DB_VERSION_MAJOR;
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbEnv_get_1version_1minor
  (JNIEnv *jnienv, jclass /*this_class*/)
{
    return DB_VERSION_MINOR;
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbEnv_get_1version_1patch
  (JNIEnv *jnienv, jclass /*this_class*/)
{
    return DB_VERSION_PATCH;
}

JNIEXPORT jstring JNICALL Java_com_sleepycat_db_DbEnv_get_1version_1string
  (JNIEnv *jnienv, jclass /*this_class*/)
{
    return jnienv->NewStringUTF(DB_VERSION_STRING);
}

// See discussion on errpfx above
JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_set_1errcall
  (JNIEnv *jnienv, jobject jthis, jobject errcall)
{
    DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);

    if (verify_non_null(jnienv, dbenv)) {
        prefix_info *pi = (prefix_info*)(dbenv->db_errpfx);
        if (pi->dberrcall) {
            jnienv->DeleteGlobalRef(pi->dberrcall);
        }
        if (errcall) {
            pi->dberrcall = jnienv->NewGlobalRef(errcall);
            dbenv->db_errcall = java_errcall_callback;
            pi->jnienv = jnienv;
        }
        else {
            pi->dberrcall = 0;
            dbenv->db_errcall = 0;
            pi->jnienv = 0;
        }
    }
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbEnv_get_1errcall
  (JNIEnv *jnienv, jobject jthis)
{
    DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);

    if (verify_non_null(jnienv, dbenv)) {
        prefix_info *pi = (prefix_info*)(dbenv->db_errpfx);
        return pi->dberrcall;
    }
    return 0;
}

JNIEXPORT jstring JNICALL Java_com_sleepycat_db_DbEnv_get_1errpfx
  (JNIEnv *jnienv, jobject jthis)
{
    DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);

    if (verify_non_null(jnienv, dbenv)) {
        prefix_info *pi = (prefix_info*)(dbenv->db_errpfx);
        return get_java_string(jnienv, pi->orig_prefix);
    }
    return 0;
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_set_1errpfx
  (JNIEnv *jnienv, jobject jthis, jstring str)
{
    DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);

    if (verify_non_null(jnienv, dbenv)) {
        prefix_info *pi = (prefix_info*)(dbenv->db_errpfx);
        if (pi->orig_prefix)
            DELETE(pi->orig_prefix);
        if (str)
            pi->orig_prefix =
                dup_string(jnienv->GetStringUTFChars(str, NULL));
        else
            pi->orig_prefix = 0;
    }
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbEnv_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DB_ENV *dbenv = get_DB_ENV(jnienv, jthis);
    if (dbenv) {
        // Free any data related to DB_ENV here
        prefix_info *pi = (prefix_info*)dbenv->db_errpfx;
        if (pi)
            DELETE(pi);
        DELETE(dbenv);
    }

    // Shouldn't see this object again, but just in case
    set_private_info(jnienv, name_DB_ENV, jthis, 0);
}
