/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_DbLockTab.cpp	10.3 (Sleepycat) 4/10/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_DbLockTab.h"

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLockTab_close
  (JNIEnv *jnienv, /*DbLocktab*/ jobject jthis)
{
    int err;
    DB_LOCKTAB *dblocktab = get_DB_LOCKTAB(jnienv, jthis);

    if (!verify_non_null(jnienv, dblocktab))
        return;
    err = lock_close(dblocktab);
    if (verify_return(jnienv, err))
    {
        set_private_info(jnienv, name_DB_LOCKTAB, jthis, 0);
    }
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLockTab_detect
  (JNIEnv *jnienv, jobject jthis, jint atype, jint flags)
{
    int err;
    DB_LOCKTAB *dblocktab = get_DB_LOCKTAB(jnienv, jthis);

    if (!verify_non_null(jnienv, dblocktab))
        return;
    err = lock_detect(dblocktab, atype, flags);
    verify_return(jnienv, err);
}

JNIEXPORT /*DbLock*/ jobject JNICALL Java_com_sleepycat_db_DbLockTab_get
  (JNIEnv *jnienv, jobject jthis, /*u_int32_t*/ jint locker,
   jint flags, /*const Dbt*/ jobject obj, /*db_lockmode_t*/ jint lock_mode)
{
    int err;
    DB_LOCKTAB *dblocktab = get_DB_LOCKTAB(jnienv, jthis);
    DB_LOCK dblock;
    LockedDBT dbobj(jnienv, obj, 0);
    if (dbobj.has_error())
        return 0;
    /*DbLock*/ jobject retval = NULL;

    if (!verify_non_null(jnienv, dblocktab))
        return retval;
    err = lock_get(dblocktab, locker, flags, dbobj.dbt,
                   (db_lockmode_t)lock_mode, &dblock);
    if (verify_return(jnienv, err)) {
        retval = create_default_object(jnienv, name_DB_LOCK);
        set_private_long_info(jnienv, name_DB_LOCK, retval, dblock);
    }
    return retval;
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbLockTab_id
  (JNIEnv *jnienv, jobject jthis)
{
    int err;
    DB_LOCKTAB *dblocktab = get_DB_LOCKTAB(jnienv, jthis);

    if (!verify_non_null(jnienv, dblocktab))
        return -1;
    u_int32_t id;
    err = lock_id(dblocktab, &id);
    verify_return(jnienv, err);
    return id;
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbLockTab_open
  (JNIEnv *jnienv, jclass jthis_class, jstring dir, jint flags,
   jint mode, /*DbEnv*/ jobject dbenv)
{
    int err;
    jobject retval = NULL;
    DB_LOCKTAB *dblocktab;
    DB_ENV *db_dbenv = get_DB_ENV(jnienv, dbenv);
    LockedString dbdir(jnienv, dir);

    if (verify_non_null(jnienv, db_dbenv)) {
        err = lock_open(dbdir.string, flags, mode,
                      db_dbenv, &dblocktab);
        if (verify_return(jnienv, err)) {
            retval = create_default_object(jnienv, name_DB_LOCKTAB);
            set_private_info(jnienv, name_DB_LOCKTAB, retval, dblocktab);
        }
    }
    return retval;
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLockTab_unlink
  (JNIEnv *jnienv, jclass jthis_class, jstring dir, jint force,
   /*DbEnv*/ jobject dbenv)
{
    int err;
    DB_ENV *db_dbenv = get_DB_ENV(jnienv, dbenv);
    LockedString dbdir(jnienv, dir);

    if (verify_non_null(jnienv, db_dbenv)) {
        err = lock_unlink(dbdir.string, force, db_dbenv);
        verify_return(jnienv, err);
    }
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLockTab_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DB_LOCKTAB *dblocktab = get_DB_LOCKTAB(jnienv, jthis);
    if (dblocktab) {
        // Free any data related to DB_LOCKTAB here.
        // Unfortunately, we cannot really autoclose because
        // finalize is not called in any particular order
    }
}
