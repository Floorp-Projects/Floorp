/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_DbLsn.cpp	10.3 (Sleepycat) 5/2/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>              // needed for FILENAME_MAX

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_DbLsn.h"

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLsn_init_1lsn
  (JNIEnv *jnienv, /*DbLsn*/ jobject jthis)
{
    DB_LSN *lsn = NEW(DB_LSN);
    memset(lsn, 0, sizeof(DB_LSN));
    set_private_info(jnienv, name_DB_LSN, jthis, lsn);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLsn_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DB_LSN *dblsn = get_DB_LSN(jnienv, jthis);
    if (dblsn) {
        DELETE(dblsn);
    }
}
