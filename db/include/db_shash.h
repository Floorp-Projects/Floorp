/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)db_shash.h	10.3 (Sleepycat) 4/10/98
 */

/* Hash Headers */
typedef	SH_TAILQ_HEAD(hash_head) DB_HASHTAB;

/*
 * HASHLOOKUP --
 *
 * Look up something in a shared memory hash table.  The "elt" argument
 * should be a key, and cmp_func must know how to compare a key to whatever
 * structure it is that appears in the hash table.  The comparison function
 * cmp_func is called as: cmp_func(lookup_elt, table_elt);
 * begin: address of the beginning of the hash table.
 * type: the structure type of the elements that are linked in each bucket.
 * field: the name of the field by which the "type" structures are linked.
 * elt: the item for which we are searching in the hash table.
 * result: the variable into which we'll store the element if we find it.
 * nelems: the number of buckets in the hash table.
 * hash_func: the hash function that operates on elements of the type of elt
 * cmp_func: compare elements of the type of elt with those in the table (of
 *	type "type").
 *
 * If the element is not in the hash table, this macro exits with result
 * set to NULL.
 */
#define	HASHLOOKUP(begin, type, field, elt, r, n, hash, cmp) do {	\
	DB_HASHTAB *__bucket;						\
	u_int32_t __ndx;						\
									\
	__ndx = hash(elt) % (n);					\
	__bucket = &begin[__ndx];					\
	for (r = SH_TAILQ_FIRST(__bucket, type);			\
	    r != NULL; r = SH_TAILQ_NEXT(r, field, type))		\
		if (cmp(elt, r))					\
			break;						\
} while(0)

/*
 * HASHINSERT --
 *
 * Insert a new entry into the hash table.  This assumes that lookup has
 * failed; don't call it if you haven't already called HASHLOOKUP.
 * begin: the beginning address of the hash table.
 * type: the structure type of the elements that are linked in each bucket.
 * field: the name of the field by which the "type" structures are linked.
 * elt: the item to be inserted.
 * nelems: the number of buckets in the hash table.
 * hash_func: the hash function that operates on elements of the type of elt
 */
#define	HASHINSERT(begin, type, field, elt, n, hash) do {		\
	u_int32_t __ndx;						\
	DB_HASHTAB *__bucket;						\
									\
	__ndx = hash(elt) % (n);					\
	__bucket = &begin[__ndx];					\
	SH_TAILQ_INSERT_HEAD(__bucket, elt, field, type);		\
} while(0)

/*
 * HASHREMOVE --
 *	Remove the entry with a key == elt.
 * begin: address of the beginning of the hash table.
 * type: the structure type of the elements that are linked in each bucket.
 * field: the name of the field by which the "type" structures are linked.
 * elt: the item to be deleted.
 * nelems: the number of buckets in the hash table.
 * hash_func: the hash function that operates on elements of the type of elt
 * cmp_func: compare elements of the type of elt with those in the table (of
 *	type "type").
 */
#define	HASHREMOVE(begin, type, field, elt, n, hash, cmp) {		\
	u_int32_t __ndx;						\
	DB_HASHTAB *__bucket;						\
	SH_TAILQ_ENTRY *__entp;						\
									\
	__ndx = hash(elt) % (n);					\
	__bucket = &begin[__ndx];					\
	HASHLOOKUP(begin, type, field, elt, __entp, n, hash, cmp);	\
	SH_TAILQ_REMOVE(__bucket, __entp, field, type);			\
}

/*
 * HASHREMOVE_EL --
 *	Given the object "obj" in the table, remove it.
 * begin: address of the beginning of the hash table.
 * type: the structure type of the elements that are linked in each bucket.
 * field: the name of the field by which the "type" structures are linked.
 * obj: the object in the table that we with to delete.
 * nelems: the number of buckets in the hash table.
 * hash_func: the hash function that operates on elements of the type of elt
 */
#define	HASHREMOVE_EL(begin, type, field, obj, n, hash) {		\
	u_int32_t __ndx;						\
	DB_HASHTAB *__bucket;						\
									\
	__ndx = hash(obj) % (n);					\
	__bucket = &begin[__ndx];					\
	SH_TAILQ_REMOVE(__bucket, obj, field, type);			\
}
