/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_Dbc.cpp	10.4 (Sleepycat) 4/10/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_Dbc.h"

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbc_close
  (JNIEnv *jnienv, jobject jthis)
{
    int err;
    DBC *dbc = get_DBC(jnienv, jthis);

    if (!verify_non_null(jnienv, dbc))
        return;
    err = dbc->c_close(dbc);
    if (verify_return(jnienv, err))
    {
        set_private_info(jnienv, name_DBC, jthis, 0);
    }
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbc_del
  (JNIEnv *jnienv, jobject jthis, jint flags)
{
    int err;
    DBC *dbc = get_DBC(jnienv, jthis);

    if (!verify_non_null(jnienv, dbc))
        return;
    err = dbc->c_del(dbc, flags);
    verify_return(jnienv, err);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Dbc_get
  (JNIEnv *jnienv, jobject jthis,
   /*Dbt*/ jobject key, /*Dbt*/ jobject data, jint flags)
{
    // Depending on flags, the user may be supplying the key,
    // or else we may have to retrieve it.
    int retrieve_key = ((flags & (DB_SET|DB_SET_RANGE|DB_SET_RECNO)) == 0);
    int err;
    DBC *dbc = get_DBC(jnienv, jthis);
    LockedDBT dbkey(jnienv, key, retrieve_key);

    if (dbkey.has_error())
        return 0;
    LockedDBT dbdata(jnienv, data, 1);
    if (dbdata.has_error())
        return 0;

    if (!verify_non_null(jnienv, dbc))
        return -1;
    err = dbc->c_get(dbc, dbkey.dbt, dbdata.dbt, flags);
    if (err != DB_NOTFOUND) {
        verify_return(jnienv, err);
    }
    return err;
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbc_put
  (JNIEnv *jnienv, jobject jthis,
   /*Dbt*/ jobject key, /*Dbt*/ jobject data, jint flags)
{
    int err;
    DBC *dbc = get_DBC(jnienv, jthis);
    LockedDBT dbkey(jnienv, key, 0);
    if (dbkey.has_error())
        return;
    LockedDBT dbdata(jnienv, data, 0);
    if (dbdata.has_error())
        return;

    if (!verify_non_null(jnienv, dbc))
        return;
    err = dbc->c_put(dbc, dbkey.dbt, dbdata.dbt, flags);
    verify_return(jnienv, err);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbc_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DBC *dbc = get_DBC(jnienv, jthis);
    if (dbc) {
        // Free any data related to DBC here
    }
}
