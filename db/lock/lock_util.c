/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)lock_util.c	10.9 (Sleepycat) 4/26/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_page.h"
#include "db_shash.h"
#include "hash.h"
#include "lock.h"

/*
 * __lock_cmp --
 *	This function is used to compare a DBT that is about to be entered
 *	into a hash table with an object already in the hash table.  Note
 *	that it just returns true on equal and 0 on not-equal.  Therefore
 *	this function cannot be used as a sort function; its purpose is to
 *	be used as a hash comparison function.
 *
 * PUBLIC: int __lock_cmp __P((const DBT *, DB_LOCKOBJ *));
 */
int
__lock_cmp(dbt, lock_obj)
	const DBT *dbt;
	DB_LOCKOBJ *lock_obj;
{
	void *obj_data;

	if (lock_obj->type != DB_LOCK_OBJTYPE)
		return (0);

	obj_data = SH_DBT_PTR(&lock_obj->lockobj);
	return (dbt->size == lock_obj->lockobj.size &&
		memcmp(dbt->data, obj_data, dbt->size) == 0);
}

/*
 * PUBLIC: int __lock_locker_cmp __P((u_int32_t, DB_LOCKOBJ *));
 */
int
__lock_locker_cmp(locker, lock_obj)
	u_int32_t locker;
	DB_LOCKOBJ *lock_obj;
{
	void *obj_data;

	if (lock_obj->type != DB_LOCK_LOCKER)
		return (0);

	obj_data = SH_DBT_PTR(&lock_obj->lockobj);
	return (memcmp(&locker, obj_data, sizeof(u_int32_t)) == 0);
}

/*
 * The next two functions are the hash functions used to store objects in the
 * lock hash table.  They are hashing the same items, but one (__lock_ohash)
 * takes a DBT (used for hashing a parameter passed from the user) and the
 * other (__lock_lhash) takes a DB_LOCKOBJ (used for hashing something that is
 * already in the lock manager).  In both cases, we have a special check to
 * fast path the case where we think we are doing a hash on a DB page/fileid
 * pair.  If the size is right, then we do the fast hash.
 *
 * We know that DB uses struct __db_ilocks for its lock objects.  The first
 * four bytes are the 4-byte page number and the next DB_FILE_ID_LEN bytes
 * are a unique file id, where the first 4 bytes on UNIX systems are the file
 * inode number, and the first 4 bytes on Windows systems are the FileIndexLow
 * bytes.  So, we use the XOR of the page number and the first four bytes of
 * the file id to produce a 32-bit hash value.
 *
 * We have no particular reason to believe that this algorithm will produce
 * a good hash, but we want a fast hash more than we want a good one, when
 * we're coming through this code path.
 */
#define FAST_HASH(P) {			\
	u_int32_t __h;			\
	u_int8_t *__cp, *__hp;		\
	__hp = (u_int8_t *)&__h;	\
	__cp = (u_int8_t *)(P);		\
	__hp[0] = __cp[0] ^ __cp[4];	\
	__hp[1] = __cp[1] ^ __cp[5];	\
	__hp[2] = __cp[2] ^ __cp[6];	\
	__hp[3] = __cp[3] ^ __cp[7];	\
	return (__h);			\
}

/*
 * __lock_ohash --
 *
 * PUBLIC: u_int32_t __lock_ohash __P((const DBT *));
 */
u_int32_t
__lock_ohash(dbt)
	const DBT *dbt;
{
	if (dbt->size == sizeof(struct __db_ilock))
		FAST_HASH(dbt->data);

	return (__ham_func5(dbt->data, dbt->size));
}

/*
 * __lock_lhash --
 *
 * PUBLIC: u_int32_t __lock_lhash __P((DB_LOCKOBJ *));
 */
u_int32_t
__lock_lhash(lock_obj)
	DB_LOCKOBJ *lock_obj;
{
	u_int32_t tmp;
	void *obj_data;

	obj_data = SH_DBT_PTR(&lock_obj->lockobj);
	if (lock_obj->type == DB_LOCK_LOCKER) {
		memcpy(&tmp, obj_data, sizeof(u_int32_t));
		return (tmp);
	}

	if (lock_obj->lockobj.size == sizeof(struct __db_ilock))
		FAST_HASH(obj_data);

	return (__ham_func5(obj_data, lock_obj->lockobj.size));
}

/*
 * __lock_locker_hash --
 *	Hash function for entering lockers into the hash table.  Since these
 *	are simply 32-bit unsigned integers, just return the locker value.
 *
 * PUBLIC: u_int32_t __lock_locker_hash __P((u_int32_t));
 */
u_int32_t
__lock_locker_hash(locker)
	u_int32_t locker;
{
	return (locker);
}
