/* -*- Mode: C; indent-tabs-mode: nil; -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the TripleDB database library.
 *
 * The Initial Developer of the Original Code is Geocast Network Systems.
 * Portions created by Geocast are
 * Copyright (C) 1999 Geocast Network Systems. All
 * Rights Reserved.
 *
 * Contributor(s): Terry Weissman <terry@mozilla.org>
 */

/* tdb.c -- Routines operating on the database as a whole. */


#include "tdbtypes.h"
#include "prprf.h"

TDB* TDBOpen(const char* filename)
{
    PRStatus status;
    TDB* db;
    
    PR_ASSERT(filename != NULL);
    if (filename == NULL) return NULL;
    db = PR_NEWZAP(TDB);
    if (db == NULL) return NULL;
    db->filename = PR_Malloc(strlen(filename) + 1);
    if (db->filename == NULL) {
        PR_Free(db);
        return NULL;
    }
    strcpy(db->filename, filename);
    db->fid = PR_Open(filename, PR_RDWR | PR_CREATE_FILE, 0666);
    if (db->fid == NULL) {
        /* Can't open file. */
        goto FAIL;
    }

    db->mutex = PR_NewLock();
    if (db->mutex == NULL) goto FAIL;

    db->callbackcvargo = PR_NewCondVar(db->mutex);
    if (db->callbackcvargo == NULL) goto FAIL;

    db->callbackcvaridle = PR_NewCondVar(db->mutex);
    if (db->callbackcvaridle == NULL) goto FAIL;
    
    db->callbackthread =
        PR_CreateThread(PR_SYSTEM_THREAD, tdbCallbackThread, db,
                        PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
                        PR_JOINABLE_THREAD, 0);
    if (db->callbackthread == NULL) goto FAIL;

    PR_Lock(db->mutex);
    while (!db->callbackidle) {
        PR_WaitCondVar(db->callbackcvaridle, PR_INTERVAL_NO_TIMEOUT);
    }
    status = tdbLoadRoots(db);
    PR_Unlock(db->mutex);

    if (status == PR_FAILURE) goto FAIL;
    return db;

 FAIL:
    TDBClose(db);
    return NULL;
}


PRStatus tdbLoadRoots(TDB* db)
{
    char buf[TDB_FILEHEADER_SIZE];
    char* ptr;
    PRInt32 i, length;
    PRFileInfo info;

    if (PR_Seek(db->fid, 0, PR_SEEK_SET) < 0) {
        return PR_FAILURE;
    }

    i = 4 + sizeof(PRInt32) + (NUMTREES + 1) * sizeof(TDBPtr);
    length = PR_Read(db->fid, buf, i);
    if (length > 0 && length < i) {
        /* Ick.  This appears to be a short file.  Punt. */
        return PR_FAILURE;
    }
    if (length > 0) {
        if (memcmp(TDB_MAGIC, buf, 4) != 0) {
            /* Bad magic number. */
            return PR_FAILURE;
        }
        ptr = buf + 4;
        i = tdbGetInt32(&ptr);
        if (i != TDB_VERSION) {
            /* Bad version number. */
            return PR_FAILURE;
        }
        db->freeroot = tdbGetInt32(&ptr);
        for (i=0 ; i<NUMTREES ; i++) {
            db->roots[i] = tdbGetInt32(&ptr);
        }
    } else {
        db->dirty = db->rootdirty = PR_TRUE;
        tdbFlush(db);           /* Write out the roots the first time, so
                                   that we can accurately read the file
                                   size and stuff. */
    }
    PR_GetOpenFileInfo(db->fid, &info);
    if (info.size < TDB_FILEHEADER_SIZE) {
        int length = TDB_FILEHEADER_SIZE - info.size;
        if (PR_Seek(db->fid, info.size, PR_SEEK_SET) < 0) {
            return PR_FAILURE;
        }
        memset(buf, 0, length);
        if (length != PR_Write(db->fid, buf, length)) {
            return PR_FAILURE;
        }
        PR_GetOpenFileInfo(db->fid, &info);
        PR_ASSERT(info.size == TDB_FILEHEADER_SIZE);
    }
    db->filelength = info.size;
    return PR_SUCCESS;
}


PRStatus TDBClose(TDB* db)
{
    PRStatus result = PR_SUCCESS;
    PR_ASSERT(db != NULL);
    if (db == NULL) return PR_SUCCESS;
    if (db->mutex) PR_Lock(db->mutex);
    PR_ASSERT(db->firstcursor == NULL);
    if (db->firstcursor != NULL) {
        if (db->mutex) PR_Unlock(db->mutex);
        return PR_FAILURE;
    }
    
    if (db->dirty) {
        tdbFlush(db);
    }

    if (db->callbackthread) {
        db->killcallbackthread = PR_TRUE;
        PR_NotifyAllCondVar(db->callbackcvargo);
        PR_Unlock(db->mutex);
        PR_JoinThread(db->callbackthread);
        PR_Lock(db->mutex);
    }
    if (db->callbackcvargo) {
        PR_DestroyCondVar(db->callbackcvargo);
    }
    if (db->callbackcvaridle) {
        PR_DestroyCondVar(db->callbackcvaridle);
    }
    if (db->mutex) {
        PR_Unlock(db->mutex);
        PR_DestroyLock(db->mutex);
    }
        
    if (db->fid) {
        result = PR_Close(db->fid);
    }
    PR_Free(db->filename);
    PR_Free(db);
    return result;
}



PRStatus TDBRebuildDatabase(TDB* db)
{
    PRStatus status;
    char* tmpfile;
    PRFileInfo info;
    PRInt32 count = 0;
    TDB* tmpdb;

    PR_ASSERT(db != NULL);
    if (db == NULL) return PR_SUCCESS;
    PR_Lock(db->mutex);
    do {
        tmpfile = PR_smprintf("%s-tmp-%d", db->filename, ++count);
        status = PR_GetFileInfo(tmpfile, &info);
    } while (status == PR_SUCCESS);
    tmpdb = TDBOpen(tmpfile);
    if (!tmpdb) return PR_FAILURE;
                                /* ### Finish writing me!  */
    
    return PR_FAILURE;
}


PRStatus tdbFlush(TDB* db)
{
    PRInt32 length;
    PRInt32 i;
    PRStatus status;
    TDBRecord* record;
    TDBRecord** recptr;
    char* ptr;
    PR_ASSERT(db != NULL);
    if (db == NULL) return PR_FAILURE;
    if (db->rootdirty) {
        PR_ASSERT(db->dirty);
        if (PR_Seek(db->fid, 0, PR_SEEK_SET) < 0) {
            return PR_FAILURE;
        }
        length = 4 + sizeof(PRInt32) + (NUMTREES + 1) * sizeof(TDBPtr);
        status = tdbGrowIobuf(db, length);
        if (status != PR_SUCCESS) return status;
        ptr = db->iobuf;
        memcpy(ptr, TDB_MAGIC, 4);
        ptr += 4;
        tdbPutInt32(&ptr, TDB_VERSION);
        tdbPutInt32(&ptr, db->freeroot);
        for (i=0 ; i<NUMTREES ; i++) {
            tdbPutInt32(&ptr, db->roots[i]);
        }
        PR_ASSERT(ptr - db->iobuf <= TDB_FILEHEADER_SIZE);
        if (PR_Write(db->fid, db->iobuf, ptr - db->iobuf) != ptr - db->iobuf) {
            return PR_FAILURE;
        }
        db->rootdirty = PR_FALSE;
    }
    recptr = &(db->firstrecord);
    while (*recptr) {
        record = *recptr;
        if (record->dirty) {
            PR_ASSERT(db->dirty);
	    PR_ASSERT(record->refcnt == 0);
            status = tdbSaveRecord(db, record);
            if (status != PR_SUCCESS) return status;
        }
	if (record->refcnt == 0) {
	    *recptr = record->next;
	    status = tdbFreeRecord(record);
            if (status != PR_SUCCESS) return status;
	} else {
	    PR_ASSERT(record->refcnt > 0);
	    recptr = (&record->next);
	}
    }
    db->dirty = PR_FALSE;
    return PR_SUCCESS;
}


PRStatus tdbThrowAwayUnflushedChanges(TDB* db)
{
    TDBRecord* record;
    db->dirty = PR_FALSE;
    db->rootdirty = PR_FALSE;
    for (record = db->firstrecord ; record ; record = record->next) {
        record->dirty = PR_FALSE;
    }
    tdbFlush(db);
    return tdbLoadRoots(db);
}


PRStatus tdbGrowIobuf(TDB* db, PRInt32 length)
{
    if (length > db->iobuflength) {
        db->iobuflength = length;
        if (db->iobuf == NULL) {
            db->iobuf = PR_Malloc(db->iobuflength);
        } else {
            db->iobuf = PR_Realloc(db->iobuf, db->iobuflength);
        }
        if (db->iobuf == NULL) {
            return PR_FAILURE;
        }
    }
    return PR_SUCCESS;
}


void tdbMarkCorrupted(TDB* db)
{
    PR_ASSERT(0);
    /* ### Write me!!! ### */
}



PRInt32 tdbGetInt32(char** ptr)
{
    PRInt32 result;
    memcpy(&result, *ptr, sizeof(PRInt32));
    *ptr += sizeof(PRInt32);
    return result;
}

PRInt32 tdbGetInt16(char** ptr)
{
    PRInt16 result;
    memcpy(&result, *ptr, sizeof(PRInt16));
    *ptr += sizeof(PRInt16);
    return result;
}

PRInt8 tdbGetInt8(char** ptr)
{
    PRInt8 result;
    memcpy(&result, *ptr, sizeof(PRInt8));
    *ptr += sizeof(PRInt8);
    return result;
}

PRInt64 tdbGetInt64(char** ptr)
{
    PRInt64 result;
    memcpy(&result, *ptr, sizeof(PRInt64));
    *ptr += sizeof(PRInt64);
    return result;
}

PRUint16 tdbGetUInt16(char** ptr)
{
    PRUint16 result;
    memcpy(&result, *ptr, sizeof(PRUint16));
    *ptr += sizeof(PRUint16);
    return result;
}


void tdbPutInt32(char** ptr, PRInt32 value)
{
    memcpy(*ptr, &value, sizeof(PRInt32));
    *ptr += sizeof(PRInt32);
}

void tdbPutInt16(char** ptr, PRInt16 value) 
{
    memcpy(*ptr, &value, sizeof(PRInt16));
    *ptr += sizeof(PRInt16);
}

void tdbPutUInt16(char** ptr, PRUint16 value) 
{
    memcpy(*ptr, &value, sizeof(PRUint16));
    *ptr += sizeof(PRUint16);
}

void tdbPutInt8(char** ptr, PRInt8 value)
{
    memcpy(*ptr, &value, sizeof(PRInt8));
    *ptr += sizeof(PRInt8);
}

void tdbPutInt64(char** ptr, PRInt64 value)
{
    memcpy(*ptr, &value, sizeof(PRInt64));
    *ptr += sizeof(PRInt64);
}


/*
 * The below was an attempt to write all these routines in a nice way, so
 * that the database would be platform independent.  But I keep choking
 * sign bits and things.  I give up for now.  These probably should be
 * resurrected and fixed up.
 * 
 * PRInt32 tdbGetInt32(char** ptr)
 * {
 *     PRInt32 result = ((*ptr)[0] << 24) | ((*ptr)[1] << 16) |
 *      ((*ptr)[2] << 8) | (*ptr)[3];
 *     (*ptr) += 4;
 *     return result;
 * }
 * 
 * PRInt32 tdbGetInt16(char** ptr)
 * {
 *     PRInt16 result = ((*ptr)[0] << 8) | (*ptr)[1];
 *     (*ptr) += 2;
 *     return result;
 * }
 * 
 * PRUint16 tdbGetUInt16(char** ptr)
 * {
 *     PRUint16 result = ((*ptr)[0] << 8) | (*ptr)[1];
 *     (*ptr) += 2;
 *     return result;
 * }
 * 
 * PRInt8 tdbGetInt8(char** ptr)
 * {
 *     return (PRInt8) (*((*ptr)++));
 * }
 * 
 * 
 * PRInt64 tdbGetInt64(char** ptr)
 * {
 *     PRInt64 a = tdbGetInt32(ptr);
 *     return (a << 32) | tdbGetInt32(ptr);
 * }
 * 
 * 
 * void tdbPutInt32(char** ptr, PRInt32 value)
 * {
 *     *((*ptr)++) = (value >> 24) & 0xff;
 *     *((*ptr)++) = (value >> 16) & 0xff;
 *     *((*ptr)++) = (value >> 8) & 0xff;
 *     *((*ptr)++) = (value) & 0xff;
 * }
 *     
 *     
 * void tdbPutInt16(char** ptr, PRInt16 value) 
 * {
 *     *((*ptr)++) = (value >> 8) & 0xff;
 *     *((*ptr)++) = (value) & 0xff;
 * }
 *     
 * void tdbPutUInt16(char** ptr, PRUint16 value) 
 * {
 *     *((*ptr)++) = (value >> 8) & 0xff;
 *     *((*ptr)++) = (value) & 0xff;
 * }
 *     
 * void tdbPutInt8(char** ptr, PRInt32 value)
 * {
 *     *((*ptr)++) = value;
 * }
 * 
 * void tdbPutInt64(char** ptr, PRInt64 value)
 * {
 *     tdbPutInt32(ptr, ((PRInt32) (value >> 32)));
 *     tdbPutInt32(ptr, ((PRInt32) (value & 0xffffffff)));
 * }
 */
