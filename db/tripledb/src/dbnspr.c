/* -*- Mode: C; indent-tabs-mode: nil; -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the TripleDB database library.
 *
 * The Initial Developer of the Original Code is
 * Geocast Network Systems.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terry Weissman <terry@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* #include "db/db_int.h" */
#define MEGABYTE        1048576

#include "tdbtypes.h"

#ifdef TDB_USE_NSPR

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "db/db.h"

#include "plstr.h"

#include "prerror.h"
#include "prinrval.h"
#include "prio.h"
#include "prmem.h"
#include "prthread.h"


static int
func_close(int fd)
{
    if (fd == 0) return -1;
    if (PR_Close((PRFileDesc*) fd) == PR_SUCCESS) return 0;
    return PR_GetError();
}


static void
func_dirfree(char** names, int count)
{
    int i;
    for (i=0 ; i<count ; i++) {
        PR_Free(names[i]);
    }
    PR_Free(names);
}


static int
func_dirlist(const char* dirname, char*** names, int* cnt)
{
    int max = 0;
    int count = 0;
    char** result = NULL;
    PRDir* dir;
    PRDirEntry* entry;
    dir = PR_OpenDir(dirname);
    if (dir == NULL) return PR_GetError();
    while (NULL != (entry = PR_ReadDir(dir, PR_SKIP_NONE))) {
        if (count >= max) {
            max += 100;
            result = result ? PR_Realloc(result, max * sizeof(char*)) :
                PR_Malloc(max * sizeof(char*));
            if (!result) return PR_GetError();
        }
        result[count] = PL_strdup(entry->name);
        if (result[count] == NULL) {
            func_dirfree(result, count);
            return PR_GetError();
        }
        count++;
    }
    *names = result;
    *cnt = count;
    return 0;
}


static int
func_exists(const char *path, int *isdirp)
{
    PRFileInfo info;
    if (PR_GetFileInfo(path, &info) != PR_SUCCESS) {
        return PR_GetError();
    }
    if (isdirp) {
        *isdirp = (info.type == PR_FILE_DIRECTORY);
    }
    return 0;
}


static int
func_fsync(int fd)
{
    return PR_Sync((PRFileDesc*) fd) == PR_SUCCESS ? 0 : PR_GetError();
}


static int
func_ioinfo(const char *path, int fd, u_int32_t *mbytesp, u_int32_t *bytesp,
            u_int32_t *iosizep)
{
    PRFileInfo64 info;
    if (PR_GetFileInfo64(path, &info) != PR_SUCCESS) {
        return PR_GetError();
    }
    if (mbytesp) *mbytesp = info.size / MEGABYTE;
    if (bytesp) *bytesp = info.size % MEGABYTE;
    if (iosizep) *iosizep = (8 * 1024); /* ### Ack!  Fix! *** */
    return 0;
}


static int
func_open(const char* path, int flags, int mode)
{
    PRIntn f = 0;
    PRFileDesc* fid;
    if (flags & O_RDONLY) {
        f |= PR_RDONLY;
    }
    if (flags & O_WRONLY) {
        f |= PR_WRONLY;
    }
    if (flags & O_RDWR) {
        f |= PR_RDWR;
    }
    if (flags & O_CREAT) {
        f |= PR_CREATE_FILE;
    }
    if (flags & O_TRUNC) {
        f |= PR_TRUNCATE;
    }
    if (flags & O_SYNC) {
        f |= PR_SYNC;
    }
    fid = PR_Open(path, f, mode);
    if (fid == NULL) return -1;
    return (int) fid;
}


static ssize_t
func_read(int fd, void* buf, size_t nbytes)
{
    return PR_Read((PRFileDesc*) fd, buf, nbytes);
}


static int
func_seek(int fd, size_t pgsize,
          db_pgno_t pageno, u_int32_t relative, int rewind, int db_whence)
{
    PRSeekWhence whence;
    PRInt64 offset;
    switch (db_whence) {
    case SEEK_CUR:
        whence = PR_SEEK_CUR;
        break;
    case SEEK_END:
        whence = PR_SEEK_END;
        break;
    case SEEK_SET:
        whence = PR_SEEK_SET;
        break;
    default:
        return -1;
    }
    offset = ((PRInt64) pgsize) * ((PRInt64) pageno) + relative;
    if (rewind) offset = -offset;
    if (PR_Seek64((PRFileDesc*) fd, offset, whence) < 0) return PR_GetError();
    return 0;
}


static int
func_sleep(u_long seconds, u_long microseconds)
{
    if (PR_Sleep(PR_SecondsToInterval(seconds) +
                 PR_MicrosecondsToInterval(microseconds)) == PR_SUCCESS) {
        return 0;
    }
    return PR_GetError();
}


static int
func_unlink(const char* path)
{
    return PR_Delete(path) == PR_SUCCESS ? 0 : PR_GetError();
}


static ssize_t
func_write(int fd, const void* buffer, size_t nbytes)
{
    return PR_Write((PRFileDesc*) fd, buffer, nbytes);
}


TDBStatus
tdbMakeDBCompatableWithNSPR(DB_ENV* env)
{
    if (env->set_func_close(env, func_close) != DB_OK) return TDB_FAILURE;
    if (env->set_func_dirfree(env, func_dirfree) != DB_OK) return TDB_FAILURE;
    if (env->set_func_dirlist(env, func_dirlist) != DB_OK) return TDB_FAILURE;
    if (env->set_func_exists(env, func_exists) != DB_OK) return TDB_FAILURE;
    if (env->set_func_free(env, PR_Free) != DB_OK) return TDB_FAILURE;
    if (env->set_func_fsync(env, func_fsync) != DB_OK) return TDB_FAILURE;
    if (env->set_func_ioinfo(env, func_ioinfo) != DB_OK) return TDB_FAILURE;
    if (env->set_func_malloc(env, PR_Malloc) != DB_OK) return TDB_FAILURE;
    /* if (env->set_func_map(env, func_map) != DB_OK) return TDB_FAILURE; */
    if (env->set_func_open(env, (int (*)(const char *, int, ...)) func_open)
        != DB_OK) return TDB_FAILURE;
    if (env->set_func_read(env, func_read) != DB_OK) return TDB_FAILURE;
    if (env->set_func_realloc(env, PR_Realloc) != DB_OK) return TDB_FAILURE;
    if (env->set_func_seek(env, func_seek) != DB_OK) return TDB_FAILURE;
    if (env->set_func_sleep(env, func_sleep) != DB_OK) return TDB_FAILURE;
    if (env->set_func_unlink(env, func_unlink) != DB_OK) return TDB_FAILURE;
    /* if (env->set_func_unmap(env, func_unmap) != DB_OK) return TDB_FAILURE;*/
    if (env->set_func_write(env, func_write) != DB_OK) return TDB_FAILURE;
    /* if (env->set_func_yield(env, func_yield) != DB_OK) return TDB_FAILURE;*/

    return TDB_SUCCESS;
}


#endif /* TDB_USE_NSPR */
