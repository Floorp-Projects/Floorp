/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_Dbt.cpp	10.4 (Sleepycat) 4/10/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_Dbt.h"


JAVADB_RW_ACCESS(Dbt, jint, size, DBT, size)
JAVADB_RW_ACCESS(Dbt, jint, ulen, DBT, ulen)
JAVADB_RW_ACCESS(Dbt, jint, dlen, DBT, dlen)
JAVADB_RW_ACCESS(Dbt, jint, doff, DBT, doff)
JAVADB_RW_ACCESS(Dbt, jint, flags, DBT, flags)

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbt_init
  (JNIEnv *jnienv, jobject jthis)
{
    DBT_info *dbt = NEW(DBT_info);
    set_private_info(jnienv, name_DBT, jthis, dbt);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbt_internal_1set_1data
  (JNIEnv *jnienv, jobject jthis, jbyteArray array)
{
    DBT_info *db_this = get_DBT(jnienv, jthis);

    if (verify_non_null(jnienv, db_this)) {

        // If we previously allocated an array for java,
        // must release reference.
        db_this->release(jnienv);

        // Make the array a global ref, it won't be GC'd till we release it.
        if (array)
            array = (jbyteArray)jnienv->NewGlobalRef(array);
        db_this->array_ = array;
    }
}

JNIEXPORT jbyteArray JNICALL Java_com_sleepycat_db_Dbt_get_1data
  (JNIEnv *jnienv, jobject jthis)
{
    DBT_info *db_this = get_DBT(jnienv, jthis);

    if (verify_non_null(jnienv, db_this)) {
        return db_this->array_;
    }
    return 0;
}


JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbt_set_1offset
  (JNIEnv *jnienv, jobject jthis, jint offset)
{
    DBT_info *db_this = get_DBT(jnienv, jthis);

    if (verify_non_null(jnienv, db_this)) {
        db_this->offset_ = offset;
    }
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Dbt_get_1offset
  (JNIEnv *jnienv, jobject jthis)
{
    DBT_info *db_this = get_DBT(jnienv, jthis);

    if (verify_non_null(jnienv, db_this)) {
        return db_this->offset_;
    }
    return 0;
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbt_set_1recno_1key_1data(JNIEnv *jnienv, jobject jthis, jint value)
{
    LockedDBT dbt_this(jnienv, jthis, 0);
    if (dbt_this.has_error())
        return;

    if (!dbt_this.dbt->data ||
        dbt_this.java_array_len_ < sizeof(db_recno_t)) {
        char buf[200];
        sprintf(buf, "set_recno_key_data error: %p %p %d %d",
                dbt_this.dbt, dbt_this.dbt->data,
                dbt_this.dbt->ulen, sizeof(db_recno_t));
        report_exception(jnienv, buf, 0);
    }
    else {
        *(db_recno_t*)(dbt_this.dbt->data) = value;
    }
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_Dbt_get_1recno_1key_1data(JNIEnv *jnienv, jobject jthis)
{
    // Although this is kind of like "retrieve", we don't support
    // DB_DBT_MALLOC for this operation, so we tell LockedDBT constructor
    // that is not a retrieve.
    //
    LockedDBT dbt_this(jnienv, jthis, 0);
    if (dbt_this.has_error())
        return 0;

    if (!dbt_this.dbt->data ||
        dbt_this.java_array_len_ < sizeof(db_recno_t)) {
        char buf[200];
        sprintf(buf, "get_recno_key_data error: %p %p %d %d",
                dbt_this.dbt, dbt_this.dbt->data,
                dbt_this.dbt->ulen, sizeof(db_recno_t));
        report_exception(jnienv, buf, 0);
        return 0;
    }
    else {
        return *(db_recno_t*)(dbt_this.dbt->data);
    }
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_Dbt_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DBT_info *dbt = get_DBT(jnienv, jthis);
    if (dbt) {
        // Free any data related to DBT here
        dbt->release(jnienv);
        DELETE(dbt);
    }
}
