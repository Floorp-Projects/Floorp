/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */
/*
 *
 *  memcache.c - routines that implement an in-memory cache.
 *
 *  To Do:  1) ber_dup_ext().
 *	    2) referrals and reference?
 */

#include <assert.h>
#include "ldap-int.h"

/*
 * Extra size allocated to BerElement.
 * XXXmcs: must match EXBUFSIZ in liblber/io.c?
 */
#define EXTRA_SIZE		    1024

/* Mode constants for function memcache_access() */
#define MEMCACHE_ACCESS_ADD	    0
#define MEMCACHE_ACCESS_APPEND	    1
#define MEMCACHE_ACCESS_APPEND_LAST 2
#define MEMCACHE_ACCESS_FIND	    3
#define MEMCACHE_ACCESS_DELETE	    4
#define MEMCACHE_ACCESS_DELETE_ALL  5
#define MEMCACHE_ACCESS_UPDATE	    6
#define MEMCACHE_ACCESS_FLUSH	    7
#define MEMCACHE_ACCESS_FLUSH_ALL   8
#define MEMCACHE_ACCESS_FLUSH_LRU   9

/* Mode constants for function memcache_adj_size */
#define MEMCACHE_SIZE_DEDUCT	    0
#define MEMCACHE_SIZE_ADD	    1

#define MEMCACHE_SIZE_ENTRIES       1
#define MEMCACHE_SIZE_NON_ENTRIES   2

/* Size used for calculation if given size of cache is 0 */
#define MEMCACHE_DEF_SIZE	    131072		/* 128K bytes */

/* Index into different list structure */
#define LIST_TTL		    0
#define LIST_LRU		    1
#define LIST_TMP		    2
#define LIST_TOTAL		    3

/* Macros to make code more readable */
#define NSLDAPI_VALID_MEMCACHE_POINTER( cp )	( (cp) != NULL )
#define NSLDAPI_STR_NONNULL( s )		( (s) ? (s) : "" )
#define NSLDAPI_SAFE_STRLEN( s )		( (s) ? strlen((s)) + 1 : 1 )

/* Macros dealing with mutex */
#define LDAP_MEMCACHE_MUTEX_LOCK( c ) \
	if ( (c) && ((c)->ldmemc_lock_fns).ltf_mutex_lock ) { \
	    ((c)->ldmemc_lock_fns).ltf_mutex_lock( (c)->ldmemc_lock ); \
	}

#define LDAP_MEMCACHE_MUTEX_UNLOCK( c ) \
	if ( (c) && ((c)->ldmemc_lock_fns).ltf_mutex_unlock ) { \
	    ((c)->ldmemc_lock_fns).ltf_mutex_unlock( (c)->ldmemc_lock ); \
	}

#define LDAP_MEMCACHE_MUTEX_ALLOC( c ) \
	((c) && ((c)->ldmemc_lock_fns).ltf_mutex_alloc ? \
	    ((c)->ldmemc_lock_fns).ltf_mutex_alloc() : NULL)

#define LDAP_MEMCACHE_MUTEX_FREE( c ) \
	if ( (c) && ((c)->ldmemc_lock_fns).ltf_mutex_free ) { \
	    ((c)->ldmemc_lock_fns).ltf_mutex_free( (c)->ldmemc_lock ); \
	}

/* Macros used for triming unnecessary spaces in a basedn */
#define NSLDAPI_IS_SPACE( c ) \
	(((c) == ' ') || ((c) == '\t') || ((c) == '\n'))

#define NSLDAPI_IS_SEPARATER( c ) \
	((c) == ',')

/* Hash table callback function pointer definition */
typedef int (*HashFuncPtr)(int table_size, void *key);
typedef int (*PutDataPtr)(void **ppTableData, void *key, void *pData);
typedef int (*GetDataPtr)(void *pTableData, void *key, void **ppData);
typedef int (*RemoveDataPtr)(void **ppTableData, void *key, void **ppData);
typedef int (*MiscFuncPtr)(void **ppTableData, void *key, void *pData);
typedef void (*ClrTableNodePtr)(void **ppTableData, void *pData);

/* Structure of a node in a hash table */
typedef struct HashTableNode_struct {
    void *pData;
} HashTableNode;

/* Structure of a hash table */
typedef struct HashTable_struct {
    HashTableNode   *table;
    int		    size;
    HashFuncPtr	    hashfunc;
    PutDataPtr	    putdata;
    GetDataPtr	    getdata;
    MiscFuncPtr     miscfunc;
    RemoveDataPtr   removedata;
    ClrTableNodePtr clrtablenode;
} HashTable;

/* Structure uniquely identifies a search request */
typedef struct ldapmemcacheReqId_struct {
    LDAP				*ldmemcrid_ld;
    int					ldmemcrid_msgid;
} ldapmemcacheReqId;

/* Structure representing a ldap handle associated to memcache */
typedef struct ldapmemcacheld_struct {
    LDAP		    		*ldmemcl_ld;
    struct ldapmemcacheld_struct	*ldmemcl_next;
} ldapmemcacheld;

/* Structure representing header of a search result */
typedef struct ldapmemcacheRes_struct {
    char				*ldmemcr_basedn;
    unsigned long			ldmemcr_crc_key;
    unsigned long			ldmemcr_resSize;
    unsigned long			ldmemcr_timestamp;
    LDAPMessage				*ldmemcr_resHead;
    LDAPMessage				*ldmemcr_resTail;
    ldapmemcacheReqId			ldmemcr_req_id;
    struct ldapmemcacheRes_struct	*ldmemcr_next[LIST_TOTAL];
    struct ldapmemcacheRes_struct	*ldmemcr_prev[LIST_TOTAL];
    struct ldapmemcacheRes_struct	*ldmemcr_htable_next;
} ldapmemcacheRes;

/* Structure for memcache statistics */
typedef struct ldapmemcacheStats_struct {
    unsigned long			ldmemcstat_tries;
    unsigned long			ldmemcstat_hits;
} ldapmemcacheStats;

/* Structure of a memcache object */
struct ldapmemcache {
    unsigned long			ldmemc_ttl;
    unsigned long			ldmemc_size;
    unsigned long			ldmemc_size_used;
    unsigned long			ldmemc_size_entries;
    char				**ldmemc_basedns;
    void				*ldmemc_lock;
    ldapmemcacheld			*ldmemc_lds;
    HashTable				*ldmemc_resTmp;
    HashTable				*ldmemc_resLookup;
    ldapmemcacheRes			*ldmemc_resHead[LIST_TOTAL];
    ldapmemcacheRes			*ldmemc_resTail[LIST_TOTAL];
    struct ldap_thread_fns		ldmemc_lock_fns;
    ldapmemcacheStats			ldmemc_stats;
};

/* Function prototypes */
static int memcache_exist(LDAP *ld);
static int memcache_add_to_ld(LDAP *ld, int msgid, LDAPMessage *pMsg);
static int memcache_compare_dn(const char *main_dn, const char *dn, int scope);
static int memcache_dup_message(LDAPMessage *res, int msgid, int fromcache, 
				LDAPMessage **ppResCopy, unsigned long *pSize);
static BerElement* memcache_ber_dup(BerElement* pBer, unsigned long *pSize);

static void memcache_trim_basedn_spaces(char *basedn);
static int memcache_validate_basedn(LDAPMemCache *cache, const char *basedn);
static int memcache_get_ctrls_len(LDAPControl **ctrls);
static void memcache_append_ctrls(char *buf, LDAPControl **serverCtrls,
				  LDAPControl **clientCtrls);
static int memcache_adj_size(LDAPMemCache *cache, unsigned long size,
                             int usageFlags, int bAdd);
static int memcache_free_entry(LDAPMemCache *cache, ldapmemcacheRes *pRes);
static int memcache_expired(LDAPMemCache *cache, ldapmemcacheRes *pRes, 
			    unsigned long curTime);
static int memcache_add_to_list(LDAPMemCache *cache, ldapmemcacheRes *pRes, 
				int index);
static int memcache_add_res_to_list(ldapmemcacheRes *pRes, LDAPMessage *pMsg, 
				    unsigned long size);
static int memcache_free_from_list(LDAPMemCache *cache, ldapmemcacheRes *pRes, 
				   int index);
static int memcache_search(LDAP *ld, unsigned long key, LDAPMessage **ppRes);
static int memcache_add(LDAP *ld, unsigned long key, int msgid,
			const char *basedn);
static int memcache_append(LDAP *ld, int msgid, LDAPMessage *pRes);
static int memcache_append_last(LDAP *ld, int msgid, LDAPMessage *pRes);
static int memcache_remove(LDAP *ld, int msgid);
#if 0	/* function not used */
static int memcache_remove_all(LDAP *ld);
#endif /* 0 */
static int memcache_access(LDAPMemCache *cache, int mode, 
			   void *pData1, void *pData2, void *pData3);
#ifdef LDAP_DEBUG
static void memcache_print_list( LDAPMemCache *cache, int index );
static void memcache_report_statistics( LDAPMemCache *cache );
#endif /* LDAP_DEBUG */

static int htable_calculate_size(int sizelimit);
static int htable_sizeinbytes(HashTable *pTable);
static int htable_put(HashTable *pTable, void *key, void *pData);
static int htable_get(HashTable *pTable, void *key, void **ppData);
static int htable_misc(HashTable *pTable, void *key, void *pData);
static int htable_remove(HashTable *pTable, void *key, void **ppData);
static int htable_removeall(HashTable *pTable, void *pData);
static int htable_create(int size_limit, HashFuncPtr hashf,
                         PutDataPtr putDataf, GetDataPtr getDataf,
			 RemoveDataPtr removeDataf, ClrTableNodePtr clrNodef,
			 MiscFuncPtr miscOpf, HashTable **ppTable);
static int htable_free(HashTable *pTable);

static int msgid_hashf(int table_size, void *key);
static int msgid_putdata(void **ppTableData, void *key, void *pData);
static int msgid_getdata(void *pTableData, void *key, void **ppData);
static int msgid_removedata(void **ppTableData, void *key, void **ppData);
static int msgid_clear_ld_items(void **ppTableData, void *key, void *pData);
static void msgid_clearnode(void **ppTableData, void *pData);

static int attrkey_hashf(int table_size, void *key);
static int attrkey_putdata(void **ppTableData, void *key, void *pData);
static int attrkey_getdata(void *pTableData, void *key, void **ppData);
static int attrkey_removedata(void **ppTableData, void *key, void **ppData);
static void attrkey_clearnode(void **ppTableData, void *pData);

static unsigned long crc32_convert(char *buf, int len);

/* Create a memcache object. */
int
LDAP_CALL
ldap_memcache_init( unsigned long ttl, unsigned long size,
                    char **baseDNs, struct ldap_thread_fns *thread_fns,
		    LDAPMemCache **cachep )
{
    unsigned long total_size = 0;

    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_memcache_init\n", 0, 0, 0 );

    if ( cachep == NULL ) {
	return( LDAP_PARAM_ERROR );
    }

    if ((*cachep = (LDAPMemCache*)NSLDAPI_CALLOC(1,
	    sizeof(LDAPMemCache))) == NULL) {
	return ( LDAP_NO_MEMORY );
    }

    total_size += sizeof(LDAPMemCache);

    (*cachep)->ldmemc_ttl = ttl;
    (*cachep)->ldmemc_size = size;
    (*cachep)->ldmemc_lds = NULL;

    /* Non-zero default size needed for calculating size of hash tables */
    size = (size ? size : MEMCACHE_DEF_SIZE);

    if (thread_fns) {
	memcpy(&((*cachep)->ldmemc_lock_fns), thread_fns, 
           sizeof(struct ldap_thread_fns));
    } else {
	memset(&((*cachep)->ldmemc_lock_fns), 0, 
	   sizeof(struct ldap_thread_fns));
    }

    (*cachep)->ldmemc_lock = LDAP_MEMCACHE_MUTEX_ALLOC( *cachep );

    /* Cache basedns */
    if (baseDNs != NULL) {

	int i;

	for (i = 0; baseDNs[i]; i++) {
		;
	}

	(*cachep)->ldmemc_basedns = (char**)NSLDAPI_CALLOC(i + 1,
		sizeof(char*));

	if ((*cachep)->ldmemc_basedns == NULL) {
    	    ldap_memcache_destroy(*cachep);
	    *cachep = NULL;
	    return ( LDAP_NO_MEMORY );
	}

	total_size += (i + 1) * sizeof(char*);

	for (i = 0; baseDNs[i]; i++) {
	    (*cachep)->ldmemc_basedns[i] = nsldapi_strdup(baseDNs[i]);
	    total_size += strlen(baseDNs[i]) + 1;
	}

	(*cachep)->ldmemc_basedns[i] = NULL;
    }

    /* Create hash table for temporary cache */
    if (htable_create(size, msgid_hashf, msgid_putdata, msgid_getdata,
                      msgid_removedata, msgid_clearnode, msgid_clear_ld_items,
		      &((*cachep)->ldmemc_resTmp)) != LDAP_SUCCESS) {
	ldap_memcache_destroy(*cachep);
	*cachep = NULL;
	return( LDAP_NO_MEMORY );
    }

    total_size += htable_sizeinbytes((*cachep)->ldmemc_resTmp);

    /* Create hash table for primary cache */
    if (htable_create(size, attrkey_hashf, attrkey_putdata, 
	              attrkey_getdata, attrkey_removedata, attrkey_clearnode,
		      NULL, &((*cachep)->ldmemc_resLookup)) != LDAP_SUCCESS) {
	ldap_memcache_destroy(*cachep);
	*cachep = NULL;
	return( LDAP_NO_MEMORY );
    }

    total_size += htable_sizeinbytes((*cachep)->ldmemc_resLookup);
    
    /* See if there is enough room so far */
    if (memcache_adj_size(*cachep, total_size, MEMCACHE_SIZE_NON_ENTRIES,
	                  MEMCACHE_SIZE_ADD) != LDAP_SUCCESS) {
	ldap_memcache_destroy(*cachep);
	*cachep = NULL;
	return( LDAP_SIZELIMIT_EXCEEDED );
    }

    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_memcache_init new cache 0x%x\n",
	    *cachep, 0, 0 );

    return( LDAP_SUCCESS );
}

/* Associates a ldap handle to a memcache object. */
int
LDAP_CALL
ldap_memcache_set( LDAP *ld, LDAPMemCache *cache )
{
    int nRes = LDAP_SUCCESS;

    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_memcache_set\n", 0, 0, 0 );

    if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) )
	return( LDAP_PARAM_ERROR );

    LDAP_MUTEX_LOCK( ld, LDAP_MEMCACHE_LOCK );

    if (ld->ld_memcache != cache) {

        LDAPMemCache *c = ld->ld_memcache;
	ldapmemcacheld *pCur = NULL;
	ldapmemcacheld *pPrev = NULL;

	/* First dissociate handle from old cache */

	LDAP_MEMCACHE_MUTEX_LOCK( c );

	pCur = (c ? c->ldmemc_lds : NULL);
	for (; pCur; pCur = pCur->ldmemcl_next) {
	    if (pCur->ldmemcl_ld == ld)
		break;
	    pPrev = pCur;
	}

	if (pCur) {

	    ldapmemcacheReqId reqid;

	    reqid.ldmemcrid_ld = ld;
	    reqid.ldmemcrid_msgid = -1;
	    htable_misc(c->ldmemc_resTmp, (void*)&reqid, (void*)c);

	    if (pPrev)
		pPrev->ldmemcl_next = pCur->ldmemcl_next;
	    else
		c->ldmemc_lds = pCur->ldmemcl_next;
	    NSLDAPI_FREE(pCur);
	    pCur = NULL;

	    memcache_adj_size(c, sizeof(ldapmemcacheld),
	                MEMCACHE_SIZE_NON_ENTRIES, MEMCACHE_SIZE_DEDUCT);
	}

	LDAP_MEMCACHE_MUTEX_UNLOCK( c );

	ld->ld_memcache = NULL;

	/* Exit if no new cache is specified */
	if (cache == NULL) {
	    LDAP_MUTEX_UNLOCK( ld, LDAP_MEMCACHE_LOCK );
	    return( LDAP_SUCCESS );
	}

	/* Then associate handle with new cache */

        LDAP_MEMCACHE_MUTEX_LOCK( cache );

	if ((nRes = memcache_adj_size(cache, sizeof(ldapmemcacheld),
	       MEMCACHE_SIZE_NON_ENTRIES, MEMCACHE_SIZE_ADD)) != LDAP_SUCCESS) {
	    LDAP_MEMCACHE_MUTEX_UNLOCK( cache );
	    LDAP_MUTEX_UNLOCK( ld, LDAP_MEMCACHE_LOCK );
	    return nRes;
	}

	pCur = (ldapmemcacheld*)NSLDAPI_CALLOC(1, sizeof(ldapmemcacheld));
	if (pCur == NULL) {
	    memcache_adj_size(cache, sizeof(ldapmemcacheld), 
		              MEMCACHE_SIZE_NON_ENTRIES, MEMCACHE_SIZE_DEDUCT);
	    nRes = LDAP_NO_MEMORY;
	} else {
	    pCur->ldmemcl_ld = ld;
	    pCur->ldmemcl_next = cache->ldmemc_lds;
	    cache->ldmemc_lds = pCur;
	    ld->ld_memcache = cache;
	}

	LDAP_MEMCACHE_MUTEX_UNLOCK( cache );
    }

    LDAP_MUTEX_UNLOCK( ld, LDAP_MEMCACHE_LOCK );

    return nRes;
}

/* Retrieves memcache with which the ldap handle has been associated. */
int
LDAP_CALL
ldap_memcache_get( LDAP *ld, LDAPMemCache **cachep )
{
    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_memcache_get ld: 0x%x\n", ld, 0, 0 );

    if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) || cachep == NULL ) { 
	return( LDAP_PARAM_ERROR );
    }

    LDAP_MUTEX_LOCK( ld, LDAP_MEMCACHE_LOCK );
    *cachep = ld->ld_memcache;
    LDAP_MUTEX_UNLOCK( ld, LDAP_MEMCACHE_LOCK );

    return( LDAP_SUCCESS );
}

/*
 * Function that stays inside libldap and proactively expires items from
 * the given cache.  This should be called from a newly created thread since
 * it will not return until after ldap_memcache_destroy() is called.
 */
void
LDAP_CALL
ldap_memcache_update( LDAPMemCache *cache )
{
    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_memcache_update: cache 0x%x\n",
	    cache, 0, 0 );

    if ( !NSLDAPI_VALID_MEMCACHE_POINTER( cache )) {
	return;
    }

    LDAP_MEMCACHE_MUTEX_LOCK( cache );
    memcache_access(cache, MEMCACHE_ACCESS_UPDATE, NULL, NULL, NULL);
    LDAP_MEMCACHE_MUTEX_UNLOCK( cache );
}

/* Removes specified entries from given memcache. */
void
LDAP_CALL
ldap_memcache_flush( LDAPMemCache *cache, char *dn, int scope )
{
    LDAPDebug( LDAP_DEBUG_TRACE,
	    "ldap_memcache_flush( cache: 0x%x, dn: %s, scope: %d)\n",
	    cache, ( dn == NULL ) ? "(null)" : dn, scope );

    if ( !NSLDAPI_VALID_MEMCACHE_POINTER( cache )) {
	return;
    }

    LDAP_MEMCACHE_MUTEX_LOCK( cache );

    if (!dn) {
	memcache_access(cache, MEMCACHE_ACCESS_FLUSH_ALL, NULL, NULL, NULL);
    } else {
	memcache_access(cache, MEMCACHE_ACCESS_FLUSH, 
	                (void*)dn, (void*)scope, NULL);
    }

    LDAP_MEMCACHE_MUTEX_UNLOCK( cache );
}

/* Destroys the given memcache. */
void
LDAP_CALL
ldap_memcache_destroy( LDAPMemCache *cache )
{
    int i = 0;
    unsigned long size = sizeof(LDAPMemCache);
    ldapmemcacheld *pNode = NULL, *pNextNode = NULL;

    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_memcache_destroy( 0x%x )\n",
	    cache, 0, 0 );

    if ( !NSLDAPI_VALID_MEMCACHE_POINTER( cache )) {
	return;
    }

    /* Dissociate all ldap handes from this cache. */
    LDAP_MEMCACHE_MUTEX_LOCK( cache );

    for (pNode = cache->ldmemc_lds; pNode; pNode = pNextNode, i++) {
        LDAP_MUTEX_LOCK( pNode->ldmemcl_ld, LDAP_MEMCACHE_LOCK );
	cache->ldmemc_lds = pNode->ldmemcl_next;
	pNode->ldmemcl_ld->ld_memcache = NULL;
        LDAP_MUTEX_UNLOCK( pNode->ldmemcl_ld, LDAP_MEMCACHE_LOCK );
	pNextNode = pNode->ldmemcl_next;
	NSLDAPI_FREE(pNode);
    }

    size += i * sizeof(ldapmemcacheld);

    LDAP_MEMCACHE_MUTEX_UNLOCK( cache );

    /* Free array of basedns */
    if (cache->ldmemc_basedns) {
	for (i = 0; cache->ldmemc_basedns[i]; i++) {
	    size += strlen(cache->ldmemc_basedns[i]) + 1;
	    NSLDAPI_FREE(cache->ldmemc_basedns[i]);
	}
	size += (i + 1) * sizeof(char*);
	NSLDAPI_FREE(cache->ldmemc_basedns);
    }

    /* Free hash table used for temporary cache */
    if (cache->ldmemc_resTmp) {
	size += htable_sizeinbytes(cache->ldmemc_resTmp);
	memcache_access(cache, MEMCACHE_ACCESS_DELETE_ALL, NULL, NULL, NULL);
	htable_free(cache->ldmemc_resTmp);
    }

    /* Free hash table used for primary cache */
    if (cache->ldmemc_resLookup) {
	size += htable_sizeinbytes(cache->ldmemc_resLookup);
	memcache_access(cache, MEMCACHE_ACCESS_FLUSH_ALL, NULL, NULL, NULL);
	htable_free(cache->ldmemc_resLookup);
    }

    memcache_adj_size(cache, size, MEMCACHE_SIZE_NON_ENTRIES, 
	              MEMCACHE_SIZE_DEDUCT);

    LDAP_MEMCACHE_MUTEX_FREE( cache );

    NSLDAPI_FREE(cache);
}

/************************* Internal API Functions ****************************/

/* Creates an integer key by applying the Cyclic Reduntency Check algorithm on
   a long string formed by concatenating all the search parameters plus the
   current bind DN.  The key is used in the cache for looking up cached
   entries.  It is assumed that the CRC algorithm will generate
   different integers from different byte strings. */
int
ldap_memcache_createkey(LDAP *ld, const char *base, int scope, 
			const char *filter, char **attrs, 
                        int attrsonly, LDAPControl **serverctrls, 
                        LDAPControl **clientctrls, unsigned long *keyp)
{
    int nRes, i, j, i_smallest;
    int len;
    int defport;
    char buf[50];
    char *tmp, *defhost, *binddn, *keystr, *tmpbase;

    if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) || (keyp == NULL) )
	return( LDAP_PARAM_ERROR );

    *keyp = 0;

    if (!memcache_exist(ld))
	return( LDAP_LOCAL_ERROR );

    LDAP_MUTEX_LOCK( ld, LDAP_MEMCACHE_LOCK );
    LDAP_MEMCACHE_MUTEX_LOCK( ld->ld_memcache );
    nRes = memcache_validate_basedn(ld->ld_memcache, base);
    LDAP_MEMCACHE_MUTEX_UNLOCK( ld->ld_memcache );
    LDAP_MUTEX_UNLOCK( ld, LDAP_MEMCACHE_LOCK );

    if (nRes != LDAP_SUCCESS)
	return nRes;

    defhost = NSLDAPI_STR_NONNULL(ld->ld_defhost);
    defport = ld->ld_defport;
    tmpbase = nsldapi_strdup(NSLDAPI_STR_NONNULL(base));
    memcache_trim_basedn_spaces(tmpbase);

    if ((binddn = nsldapi_get_binddn(ld)) == NULL)
	binddn = "";

    sprintf(buf, "%i\n%i\n%i\n", defport, scope, (attrsonly ? 1 : 0));
    len = NSLDAPI_SAFE_STRLEN(buf) + NSLDAPI_SAFE_STRLEN(tmpbase) +
	  NSLDAPI_SAFE_STRLEN(filter) + NSLDAPI_SAFE_STRLEN(defhost) +
	  NSLDAPI_SAFE_STRLEN(binddn);

    if (attrs) {
	for (i = 0; attrs[i]; i++) {

	    for (i_smallest = j = i; attrs[j]; j++) {
		if (strcasecmp(attrs[i_smallest], attrs[j]) > 0)
		    i_smallest = j;
	    }

	    if (i != i_smallest) {
		tmp = attrs[i];
		attrs[i] = attrs[i_smallest];
		attrs[i_smallest] = tmp;
	    }

	    len += NSLDAPI_SAFE_STRLEN(attrs[i]);
	}
    } else {
	len += 1;
    }

    len += memcache_get_ctrls_len(serverctrls) + 
	   memcache_get_ctrls_len(clientctrls) + 1;

    if ((keystr = (char*)NSLDAPI_CALLOC(len, sizeof(char))) == NULL) {
	NSLDAPI_FREE(defhost);
	return( LDAP_NO_MEMORY );
    }

    sprintf(keystr, "%s\n%s\n%s\n%s\n%s\n", binddn, tmpbase,
	    NSLDAPI_STR_NONNULL(defhost), NSLDAPI_STR_NONNULL(filter), 
	    NSLDAPI_STR_NONNULL(buf));

    if (attrs) {
	for (i = 0; attrs[i]; i++) {
	    strcat(keystr, NSLDAPI_STR_NONNULL(attrs[i]));
	    strcat(keystr, "\n");
	}
    } else {
	strcat(keystr, "\n");
    }

    for (tmp = keystr; *tmp; 
         *tmp += (*tmp >= 'a' && *tmp <= 'z' ? 'A'-'a' : 0), tmp++) {
		;
	}

    memcache_append_ctrls(keystr, serverctrls, clientctrls);

    /* CRC algorithm */
    *keyp = crc32_convert(keystr, len);

    NSLDAPI_FREE(keystr);
    NSLDAPI_FREE(tmpbase);

    return LDAP_SUCCESS;
}

/* Searches the cache for the right cached entries, and if found, attaches
   them to the given ldap handle.  This function relies on locking by the
   caller. */
int
ldap_memcache_result(LDAP *ld, int msgid, unsigned long key)
{
    int nRes;
    LDAPMessage *pMsg = NULL;

    LDAPDebug( LDAP_DEBUG_TRACE,
	    "ldap_memcache_result( ld: 0x%x, msgid: %d, key: 0x%8.8lx)\n",
	    ld, msgid, key );

    if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) || (msgid < 0) ) {
	return( LDAP_PARAM_ERROR );
    }

    if (!memcache_exist(ld)) {
	return( LDAP_LOCAL_ERROR );
    }

    LDAP_MUTEX_LOCK( ld, LDAP_MEMCACHE_LOCK );
    LDAP_MEMCACHE_MUTEX_LOCK( ld->ld_memcache );

    /* Search the cache and append the results to ld if found */
    ++ld->ld_memcache->ldmemc_stats.ldmemcstat_tries;
    if ((nRes = memcache_search(ld, key, &pMsg)) == LDAP_SUCCESS) {
	nRes = memcache_add_to_ld(ld, msgid, pMsg);
	++ld->ld_memcache->ldmemc_stats.ldmemcstat_hits;
	LDAPDebug( LDAP_DEBUG_TRACE,
		"ldap_memcache_result: key 0x%8.8lx found in cache\n",
		key, 0, 0 );
    } else {
	LDAPDebug( LDAP_DEBUG_TRACE,
		"ldap_memcache_result: key 0x%8.8lx not found in cache\n",
		key, 0, 0 );
    }

#ifdef LDAP_DEBUG
    memcache_print_list( ld->ld_memcache, LIST_LRU );
    memcache_report_statistics( ld->ld_memcache );
#endif /* LDAP_DEBUG */

    LDAP_MEMCACHE_MUTEX_UNLOCK( ld->ld_memcache );
    LDAP_MUTEX_UNLOCK( ld, LDAP_MEMCACHE_LOCK );

    return nRes;
}

/* Creates a new header in the cache so that entries arriving from the
   directory server can later be cached under the header. */
int
ldap_memcache_new(LDAP *ld, int msgid, unsigned long key, const char *basedn)
{
    int nRes;

    if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) ) {
	return( LDAP_PARAM_ERROR );
    }

    LDAP_MUTEX_LOCK( ld, LDAP_MEMCACHE_LOCK );

    if (!memcache_exist(ld)) {
        LDAP_MUTEX_UNLOCK( ld, LDAP_MEMCACHE_LOCK );
	return( LDAP_LOCAL_ERROR );
    }

    LDAP_MEMCACHE_MUTEX_LOCK( ld->ld_memcache );
    nRes = memcache_add(ld, key, msgid, basedn);
    LDAP_MEMCACHE_MUTEX_UNLOCK( ld->ld_memcache );
    LDAP_MUTEX_UNLOCK( ld, LDAP_MEMCACHE_LOCK );

    return nRes;
}

/* Appends a chain of entries to an existing cache header.  Parameter "bLast"
   indicates whether there will be more entries arriving for the search in
   question. */
int
ldap_memcache_append(LDAP *ld, int msgid, int bLast, LDAPMessage *result)
{
    int nRes = LDAP_SUCCESS;

    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_memcache_append( ld: 0x%x, ", ld, 0, 0 );
    LDAPDebug( LDAP_DEBUG_TRACE, "msgid %d, bLast: %d, result: 0x%x)\n",
	    msgid, bLast, result );

    if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) || !result ) {
	return( LDAP_PARAM_ERROR );
    }

    LDAP_MUTEX_LOCK( ld, LDAP_MEMCACHE_LOCK );

    if (!memcache_exist(ld)) {
        LDAP_MUTEX_UNLOCK( ld, LDAP_MEMCACHE_LOCK );
	return( LDAP_LOCAL_ERROR );
    }

    LDAP_MEMCACHE_MUTEX_LOCK( ld->ld_memcache );

    if (!bLast)
	nRes = memcache_append(ld, msgid, result);
    else
	nRes = memcache_append_last(ld, msgid, result);

    LDAPDebug( LDAP_DEBUG_TRACE,
	    "ldap_memcache_append: %s result for msgid %d\n",
	    ( nRes == LDAP_SUCCESS ) ? "added" : "failed to add", msgid , 0 );

    LDAP_MEMCACHE_MUTEX_UNLOCK( ld->ld_memcache );
    LDAP_MUTEX_UNLOCK( ld, LDAP_MEMCACHE_LOCK );

    return nRes;
}

/* Removes partially cached results for a search as a result of calling
   ldap_abandon() by the client. */
int
ldap_memcache_abandon(LDAP *ld, int msgid)
{
    int nRes;

    if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) || (msgid < 0) ) {
	return( LDAP_PARAM_ERROR );
    }

    LDAP_MUTEX_LOCK( ld, LDAP_MEMCACHE_LOCK );

    if (!memcache_exist(ld)) {
        LDAP_MUTEX_UNLOCK( ld, LDAP_MEMCACHE_LOCK );
	return( LDAP_LOCAL_ERROR );
    }

    LDAP_MEMCACHE_MUTEX_LOCK( ld->ld_memcache );
    nRes = memcache_remove(ld, msgid);
    LDAP_MEMCACHE_MUTEX_UNLOCK( ld->ld_memcache );
    LDAP_MUTEX_UNLOCK( ld, LDAP_MEMCACHE_LOCK );

    return nRes;
}

/*************************** helper functions *******************************/

/* Removes extraneous spaces in a basedn so that basedns differ by only those
   spaces will be treated as equal.  Extraneous spaces are those that
   precedes the basedn and those that follow a comma. */
/*
 * XXXmcs: this is a bit too agressive... we need to deal with the fact that
 * commas and spaces may be quoted, in which case it is wrong to remove them.
 */
static void
memcache_trim_basedn_spaces(char *basedn)
{
    char *pRead, *pWrite;

    if (!basedn) 
	return;

    for (pWrite = pRead = basedn; *pRead; ) {
	for (; *pRead && NSLDAPI_IS_SPACE(*pRead); pRead++) {
		;
	}
	for (; *pRead && !NSLDAPI_IS_SEPARATER(*pRead);
	    *(pWrite++) = *(pRead++)) {
	    ;
	}
	*(pWrite++) = (*pRead ? *(pRead++) : *pRead);
    }
}

/* Verifies whether the results of a search should be cached or not by
   checking if the search's basedn falls under any of the basedns for which
   the memcache is responsible. */
static int
memcache_validate_basedn(LDAPMemCache *cache, const char *basedn)
{
    int i;

    if ( cache->ldmemc_basedns == NULL ) {
	return( LDAP_SUCCESS );
    }

#if 1
    if (basedn == NULL) {
	basedn = "";
    }
#else
    /* XXXmcs: I do not understand this code... */
    if (basedn == NULL)
	return (cache->ldmemc_basedns && cache->ldmemc_basedns[0] ?
	                             LDAP_OPERATIONS_ERROR : LDAP_SUCCESS);
    }
#endif

    for (i = 0; cache->ldmemc_basedns[i]; i++) {
	if (memcache_compare_dn(basedn, cache->ldmemc_basedns[i], 
	                        LDAP_SCOPE_SUBTREE) == LDAP_COMPARE_TRUE) {
	    return( LDAP_SUCCESS );
	}
    }

    return( LDAP_OPERATIONS_ERROR );
}

/* Calculates the length of the buffer needed to concatenate the contents of
   a ldap control. */
static int
memcache_get_ctrls_len(LDAPControl **ctrls)
{
    int len = 0, i;

    if (ctrls) {
	for (i = 0; ctrls[i]; i++) {
	    len += strlen(NSLDAPI_STR_NONNULL(ctrls[i]->ldctl_oid)) +
	           (ctrls[i]->ldctl_value).bv_len + 4;
	}
    }

    return len;
}

/* Contenates the contents of client and server controls to a buffer. */
static void
memcache_append_ctrls(char *buf, LDAPControl **serverCtrls,
				  LDAPControl **clientCtrls)
{
    int i, j;
    char *pCh = buf + strlen(buf);
    LDAPControl **ctrls;

    for (j = 0; j < 2; j++) {

	if ((ctrls = (j ? clientCtrls : serverCtrls)) == NULL)
	    continue;

	for (i = 0; ctrls[i]; i++) {
	    sprintf(pCh, "%s\n", NSLDAPI_STR_NONNULL(ctrls[i]->ldctl_oid));
	    pCh += strlen(NSLDAPI_STR_NONNULL(ctrls[i]->ldctl_oid)) + 1;
	    if ((ctrls[i]->ldctl_value).bv_len > 0) {
		memcpy(pCh, (ctrls[i]->ldctl_value).bv_val, 
		       (ctrls[i]->ldctl_value).bv_len);
		pCh += (ctrls[i]->ldctl_value).bv_len;
	    }
	    sprintf(pCh, "\n%i\n", (ctrls[i]->ldctl_iscritical ? 1 : 0));
	    pCh += 3;
	}
    }
}

/* Increases or decreases the size (in bytes) the given memcache currently
   uses.  If the size goes over the limit, the function returns an error. */
static int
memcache_adj_size(LDAPMemCache *cache, unsigned long size,
                  int usageFlags, int bAdd)
{
    LDAPDebug( LDAP_DEBUG_TRACE,
	    "memcache_adj_size: attempting to %s %ld %s bytes...\n",
	    bAdd ? "add" : "remove", size,
	    ( usageFlags & MEMCACHE_SIZE_ENTRIES ) ? "entry" : "non-entry" );

    if (bAdd) {
	cache->ldmemc_size_used += size;
	if ((cache->ldmemc_size > 0) &&
	    (cache->ldmemc_size_used > cache->ldmemc_size)) {

	    if (size > cache->ldmemc_size_entries) {
	        cache->ldmemc_size_used -= size;
		LDAPDebug( LDAP_DEBUG_TRACE,
			"memcache_adj_size: failed (size > size_entries %ld).\n",
			cache->ldmemc_size_entries, 0, 0 );
		return( LDAP_SIZELIMIT_EXCEEDED );
	    }

	    while (cache->ldmemc_size_used > cache->ldmemc_size) {
		if (memcache_access(cache, MEMCACHE_ACCESS_FLUSH_LRU,
		                    NULL, NULL, NULL) != LDAP_SUCCESS) {
	            cache->ldmemc_size_used -= size;
		    LDAPDebug( LDAP_DEBUG_TRACE,
			    "memcache_adj_size: failed (LRU flush failed).\n",
			    0, 0, 0 );
		    return( LDAP_SIZELIMIT_EXCEEDED );
	        }
	    }
	}
	if (usageFlags & MEMCACHE_SIZE_ENTRIES)
	    cache->ldmemc_size_entries += size;
    } else {
	cache->ldmemc_size_used -= size;
	assert(cache->ldmemc_size_used >= 0);
	if (usageFlags & MEMCACHE_SIZE_ENTRIES)
	    cache->ldmemc_size_entries -= size;
    }

#ifdef LDAP_DEBUG
    if ( cache->ldmemc_size == 0 ) {	/* no size limit */
	LDAPDebug( LDAP_DEBUG_TRACE,
		"memcache_adj_size: succeeded (new size: %ld bytes).\n",
		cache->ldmemc_size_used, 0, 0 );
    } else {
	LDAPDebug( LDAP_DEBUG_TRACE,
		"memcache_adj_size: succeeded (new size: %ld bytes, "
		"free space: %ld bytes).\n", cache->ldmemc_size_used,
		cache->ldmemc_size - cache->ldmemc_size_used, 0 );
    }
#endif /* LDAP_DEBUG */

    return( LDAP_SUCCESS );
}

/* Searches the cache for results for a particular search identified by
   parameter "key", which was generated ldap_memcache_createkey(). */
static int
memcache_search(LDAP *ld, unsigned long key, LDAPMessage **ppRes)
{
    int nRes;
    ldapmemcacheRes *pRes;

    *ppRes = NULL;

    if (!memcache_exist(ld))
        return LDAP_LOCAL_ERROR;

    nRes = memcache_access(ld->ld_memcache, MEMCACHE_ACCESS_FIND,
	                   (void*)&key, (void*)(&pRes), NULL);

    if (nRes != LDAP_SUCCESS)
	return nRes;

    *ppRes = pRes->ldmemcr_resHead;
    assert((pRes->ldmemcr_req_id).ldmemcrid_msgid == -1);

    return( LDAP_SUCCESS );
}

/* Adds a new header into the cache as a place holder for entries
   arriving later. */
static int
memcache_add(LDAP *ld, unsigned long key, int msgid,
			const char *basedn)
{
    ldapmemcacheReqId reqid;

    if (!memcache_exist(ld))
        return LDAP_LOCAL_ERROR;

    reqid.ldmemcrid_msgid = msgid;
    reqid.ldmemcrid_ld = ld;

    return memcache_access(ld->ld_memcache, MEMCACHE_ACCESS_ADD,
	                   (void*)&key, (void*)&reqid, (void*)basedn);
}

/* Appends search entries arriving from the dir server to the cache. */
static int
memcache_append(LDAP *ld, int msgid, LDAPMessage *pRes)
{
    ldapmemcacheReqId reqid;

    if (!memcache_exist(ld))
        return LDAP_LOCAL_ERROR;

    reqid.ldmemcrid_msgid = msgid;
    reqid.ldmemcrid_ld = ld;

    return memcache_access(ld->ld_memcache, MEMCACHE_ACCESS_APPEND,
	                   (void*)&reqid, (void*)pRes, NULL);
}

/* Same as memcache_append(), but the entries being appended are the
   last from the dir server.  Once all entries for a search have arrived,
   the entries are moved from secondary to primary cache, and a time
   stamp is given to the entries. */
static int
memcache_append_last(LDAP *ld, int msgid, LDAPMessage *pRes)
{
    ldapmemcacheReqId reqid;

    if (!memcache_exist(ld))
        return LDAP_LOCAL_ERROR;

    reqid.ldmemcrid_msgid = msgid;
    reqid.ldmemcrid_ld = ld;

    return memcache_access(ld->ld_memcache, MEMCACHE_ACCESS_APPEND_LAST,
	                   (void*)&reqid, (void*)pRes, NULL);
}

/* Removes entries from the temporary cache. */
static int
memcache_remove(LDAP *ld, int msgid)
{
    ldapmemcacheReqId reqid;

    if (!memcache_exist(ld))
        return LDAP_LOCAL_ERROR;

    reqid.ldmemcrid_msgid = msgid;
    reqid.ldmemcrid_ld = ld;

    return memcache_access(ld->ld_memcache, MEMCACHE_ACCESS_DELETE,
	                   (void*)&reqid, NULL, NULL);
}

#if 0 /* this function is not used */
/* Wipes out everything in the temporary cache directory. */
static int
memcache_remove_all(LDAP *ld)
{
    if (!memcache_exist(ld))
        return LDAP_LOCAL_ERROR;

    return memcache_access(ld->ld_memcache, MEMCACHE_ACCESS_DELETE_ALL,
	                   NULL, NULL, NULL);
}
#endif /* 0 */

/* Returns TRUE or FALSE */
static int
memcache_exist(LDAP *ld)
{
    return (ld->ld_memcache != NULL);
}

/* Attaches cached entries to an ldap handle. */
static int
memcache_add_to_ld(LDAP *ld, int msgid, LDAPMessage *pMsg)
{
    int nRes = LDAP_SUCCESS;
    LDAPMessage **r;
    LDAPMessage *pCopy;

    nRes = memcache_dup_message(pMsg, msgid, 1, &pCopy, NULL);
    if (nRes != LDAP_SUCCESS)
	return nRes;

    LDAP_MUTEX_LOCK( ld, LDAP_RESP_LOCK );

    for (r = &(ld->ld_responses); *r; r = &((*r)->lm_next))
	if ((*r)->lm_msgid == msgid)
	    break;

    if (*r)
	for (r = &((*r)->lm_chain); *r; r = &((*r)->lm_chain)) {
		;
	}

    *r = pCopy;

    LDAP_MUTEX_UNLOCK( ld, LDAP_RESP_LOCK );
    
    return nRes;
}

/* Check if main_dn is included in {dn, scope} */
static int
memcache_compare_dn(const char *main_dn, const char *dn, int scope)
{
    int nRes;
    char **components = NULL;
    char **main_components = NULL;

    components = ldap_explode_dn(dn, 0);
    main_components = ldap_explode_dn(main_dn, 0);

    if (!components || !main_components) {
	nRes = LDAP_COMPARE_TRUE;
    }
    else {

	int i, main_i;

	main_i = ldap_count_values(main_components) - 1;
	i = ldap_count_values(components) - 1;

	for (; i >= 0 && main_i >= 0; i--, main_i--) {
	    if (strcasecmp(main_components[main_i], components[i]))
		break;
	}

	if (i >= 0 && main_i >= 0) {
	    nRes = LDAP_COMPARE_FALSE;
	}
	else if (i < 0 && main_i < 0) {
	    if (scope != LDAP_SCOPE_ONELEVEL)
		nRes = LDAP_COMPARE_TRUE;
	    else
		nRes = LDAP_COMPARE_FALSE;
	}
	else if (main_i < 0) {
	    nRes = LDAP_COMPARE_FALSE;
	}
	else {
	    if (scope == LDAP_SCOPE_BASE) 
		nRes = LDAP_COMPARE_FALSE;
	    else if (scope == LDAP_SCOPE_SUBTREE)
		nRes = LDAP_COMPARE_TRUE;
	    else if (main_i == 0)
		nRes = LDAP_COMPARE_TRUE;
	    else
		nRes = LDAP_COMPARE_FALSE;
	}
    }

    if (components)
	ldap_value_free(components);

    if (main_components)
	ldap_value_free(main_components);

    return nRes;
}

/* Dup a complete separate copy of a berelement, including the buffers
   the berelement points to. */
static BerElement*
memcache_ber_dup(BerElement* pBer, unsigned long *pSize)
{
    BerElement *p = ber_dup(pBer);

    *pSize = 0;

    if (p) {

	*pSize += sizeof(BerElement) + EXTRA_SIZE;

	if (p->ber_len <= EXTRA_SIZE) {
	    p->ber_flags |= LBER_FLAG_NO_FREE_BUFFER;
	    p->ber_buf = (char*)p + sizeof(BerElement);
	} else {
	    p->ber_flags &= ~LBER_FLAG_NO_FREE_BUFFER;
	    p->ber_buf = (char*)NSLDAPI_CALLOC(1, p->ber_len);
	    *pSize += (p->ber_buf ? p->ber_len : 0);
	}

	if (p->ber_buf) {
	    p->ber_ptr = p->ber_buf + (pBer->ber_ptr - pBer->ber_buf);
	    p->ber_end = p->ber_buf + p->ber_len;
	    memcpy(p->ber_buf, pBer->ber_buf, p->ber_len);
	} else {
	    ber_free(p, 0);
	    p = NULL;
	    *pSize = 0;
	}
    }

    return p;
}

/* Dup a entry or a chain of entries. */
static int
memcache_dup_message(LDAPMessage *res, int msgid, int fromcache,
				LDAPMessage **ppResCopy, unsigned long *pSize)
{
    int nRes = LDAP_SUCCESS;
    unsigned long ber_size;
    LDAPMessage *pCur;
    LDAPMessage **ppCurNew;

    *ppResCopy = NULL;

    if (pSize)
	*pSize = 0;

    /* Make a copy of res */
    for (pCur = res, ppCurNew = ppResCopy; pCur; 
         pCur = pCur->lm_chain, ppCurNew = &((*ppCurNew)->lm_chain)) {

	if ((*ppCurNew = (LDAPMessage*)NSLDAPI_CALLOC(1, 
	                                     sizeof(LDAPMessage))) == NULL) {
	    nRes = LDAP_NO_MEMORY;
	    break;
	}

	memcpy(*ppCurNew, pCur, sizeof(LDAPMessage));
	(*ppCurNew)->lm_next = NULL;
	(*ppCurNew)->lm_ber = memcache_ber_dup(pCur->lm_ber, &ber_size);
	(*ppCurNew)->lm_msgid = msgid;
	(*ppCurNew)->lm_fromcache = (fromcache != 0);

	if (pSize)
	    *pSize += sizeof(LDAPMessage) + ber_size;
    }

    if ((nRes != LDAP_SUCCESS) && (*ppResCopy != NULL)) {
	ldap_msgfree(*ppResCopy);
	*ppResCopy = NULL;
	if (pSize)
	    *pSize = 0;
    }

    return nRes;
}

/************************* Cache Functions ***********************/

/* Frees a cache header. */
static int
memcache_free_entry(LDAPMemCache *cache, ldapmemcacheRes *pRes)
{
    if (pRes) {

	unsigned long size = sizeof(ldapmemcacheRes); 

	if (pRes->ldmemcr_basedn) {
	    size += strlen(pRes->ldmemcr_basedn) + 1;
	    NSLDAPI_FREE(pRes->ldmemcr_basedn);
	}

	if (pRes->ldmemcr_resHead) {
	    size += pRes->ldmemcr_resSize;
	    ldap_msgfree(pRes->ldmemcr_resHead);
	}

	NSLDAPI_FREE(pRes);

	memcache_adj_size(cache, size, MEMCACHE_SIZE_ENTRIES,
	                  MEMCACHE_SIZE_DEDUCT);
    }

    return( LDAP_SUCCESS );
}

/* Detaches a cache header from the list of headers. */
static int
memcache_free_from_list(LDAPMemCache *cache, ldapmemcacheRes *pRes, int index)
{
    if (pRes->ldmemcr_prev[index])
	pRes->ldmemcr_prev[index]->ldmemcr_next[index] = 
	                                     pRes->ldmemcr_next[index];

    if (pRes->ldmemcr_next[index])
	pRes->ldmemcr_next[index]->ldmemcr_prev[index] = 
	                                     pRes->ldmemcr_prev[index];

    if (cache->ldmemc_resHead[index] == pRes)
	cache->ldmemc_resHead[index] = pRes->ldmemcr_next[index];

    if (cache->ldmemc_resTail[index] == pRes)
	cache->ldmemc_resTail[index] = pRes->ldmemcr_prev[index];

    pRes->ldmemcr_prev[index] = NULL;
    pRes->ldmemcr_next[index] = NULL;

    return( LDAP_SUCCESS );
}

/* Inserts a new cache header to a list of headers. */
static int
memcache_add_to_list(LDAPMemCache *cache, ldapmemcacheRes *pRes, int index)
{
    if (cache->ldmemc_resHead[index])
	cache->ldmemc_resHead[index]->ldmemcr_prev[index] = pRes;
    else
	cache->ldmemc_resTail[index] = pRes;

    pRes->ldmemcr_prev[index] = NULL;
    pRes->ldmemcr_next[index] = cache->ldmemc_resHead[index];
    cache->ldmemc_resHead[index] = pRes;

    return( LDAP_SUCCESS );
}

/* Appends a chain of entries to the given cache header. */
static int
memcache_add_res_to_list(ldapmemcacheRes *pRes, LDAPMessage *pMsg,
				    unsigned long size)
{
    if (pRes->ldmemcr_resTail)
	pRes->ldmemcr_resTail->lm_chain = pMsg;
    else
	pRes->ldmemcr_resHead = pMsg;

    for (pRes->ldmemcr_resTail = pMsg;
         pRes->ldmemcr_resTail->lm_chain;
         pRes->ldmemcr_resTail = pRes->ldmemcr_resTail->lm_chain) {
		;
	}

    pRes->ldmemcr_resSize += size;

    return( LDAP_SUCCESS );
}


#ifdef LDAP_DEBUG
static void
memcache_print_list( LDAPMemCache *cache, int index )
{
    char		*name;
    ldapmemcacheRes	*restmp;

    switch( index ) {
    case LIST_TTL:
	name = "TTL";
	break;
    case LIST_LRU:
	name = "LRU";
	break;
    case LIST_TMP:
	name = "TMP";
	break;
    case LIST_TOTAL:
	name = "TOTAL";
	break;
    default:
	name = "unknown";
    }

    LDAPDebug( LDAP_DEBUG_TRACE, "memcache 0x%x %s list:\n",
	    cache, name, 0 );
    for ( restmp = cache->ldmemc_resHead[index]; restmp != NULL;
	    restmp = restmp->ldmemcr_next[index] ) {
	LDAPDebug( LDAP_DEBUG_TRACE,
		"    key: 0x%8.8lx, ld: 0x%x, msgid: %d\n",
		restmp->ldmemcr_crc_key,
		restmp->ldmemcr_req_id.ldmemcrid_ld,
		restmp->ldmemcr_req_id.ldmemcrid_msgid );
    }
    LDAPDebug( LDAP_DEBUG_TRACE, "memcache 0x%x end of %s list.\n",
	    cache, name, 0 );
}
#endif /* LDAP_DEBUG */

/* Tells whether a cached result has expired. */
static int
memcache_expired(LDAPMemCache *cache, ldapmemcacheRes *pRes,
                 unsigned long curTime)
{
    if (!cache->ldmemc_ttl)
	return 0;

    return ((unsigned long)difftime(
	                     (time_t)curTime, 
			     (time_t)(pRes->ldmemcr_timestamp)) >=
			                          cache->ldmemc_ttl);
}

/* Operates the cache in a central place. */
static int
memcache_access(LDAPMemCache *cache, int mode, 
			   void *pData1, void *pData2, void *pData3)
{
    int nRes = LDAP_SUCCESS;
    unsigned long size = 0;

    /* Add a new cache header to the cache. */
    if (mode == MEMCACHE_ACCESS_ADD) {
	unsigned long key = *((unsigned long*)pData1);
	char *basedn = (char*)pData3;
	ldapmemcacheRes *pRes = NULL;
	void* hashResult = NULL;

	nRes = htable_get(cache->ldmemc_resTmp, pData2, &hashResult);
	if (nRes == LDAP_SUCCESS)
	    return( LDAP_ALREADY_EXISTS );

	pRes = (ldapmemcacheRes*)NSLDAPI_CALLOC(1, sizeof(ldapmemcacheRes));
	if (pRes == NULL)
	    return( LDAP_NO_MEMORY );

	pRes->ldmemcr_crc_key = key;
	pRes->ldmemcr_req_id = *((ldapmemcacheReqId*)pData2);
	pRes->ldmemcr_basedn = (basedn ? nsldapi_strdup(basedn) : NULL);

	size += sizeof(ldapmemcacheRes) + strlen(basedn) + 1;
	nRes = memcache_adj_size(cache, size, MEMCACHE_SIZE_ENTRIES,
	                         MEMCACHE_SIZE_ADD);
	if (nRes == LDAP_SUCCESS)
	    nRes = htable_put(cache->ldmemc_resTmp, pData2, (void*)pRes);
	if (nRes == LDAP_SUCCESS)
	    memcache_add_to_list(cache, pRes, LIST_TMP);
	else
	    memcache_free_entry(cache, pRes);
    }
    /* Append entries to an existing cache header. */
    else if ((mode == MEMCACHE_ACCESS_APPEND) ||
	     (mode == MEMCACHE_ACCESS_APPEND_LAST)) {

	LDAPMessage *pMsg = (LDAPMessage*)pData2;
	LDAPMessage *pCopy = NULL;
	ldapmemcacheRes *pRes = NULL;
	void* hashResult = NULL;

	nRes = htable_get(cache->ldmemc_resTmp, pData1, &hashResult);
	if (nRes != LDAP_SUCCESS)
	    return nRes;

	pRes = (ldapmemcacheRes*) hashResult;
	nRes = memcache_dup_message(pMsg, pMsg->lm_msgid, 0, &pCopy, &size);
	if (nRes != LDAP_SUCCESS) {
	    nRes = htable_remove(cache->ldmemc_resTmp, pData1, NULL);
	    assert(nRes == LDAP_SUCCESS);
	    memcache_free_from_list(cache, pRes, LIST_TMP);
	    memcache_free_entry(cache, pRes);
	    return nRes;
	}

	nRes = memcache_adj_size(cache, size, MEMCACHE_SIZE_ENTRIES,
	                         MEMCACHE_SIZE_ADD);
	if (nRes != LDAP_SUCCESS) {
	    ldap_msgfree(pCopy);
	    nRes = htable_remove(cache->ldmemc_resTmp, pData1, NULL);
	    assert(nRes == LDAP_SUCCESS);
	    memcache_free_from_list(cache, pRes, LIST_TMP);
	    memcache_free_entry(cache, pRes);
	    return nRes;
	}

	memcache_add_res_to_list(pRes, pCopy, size);

	if (mode == MEMCACHE_ACCESS_APPEND)
	    return( LDAP_SUCCESS );

	nRes = htable_remove(cache->ldmemc_resTmp, pData1, NULL);
	assert(nRes == LDAP_SUCCESS);
	memcache_free_from_list(cache, pRes, LIST_TMP);
	(pRes->ldmemcr_req_id).ldmemcrid_ld = NULL;
	(pRes->ldmemcr_req_id).ldmemcrid_msgid = -1;
	pRes->ldmemcr_timestamp = (unsigned long)time(NULL);

	if ((nRes = htable_put(cache->ldmemc_resLookup, 
	                       (void*)&(pRes->ldmemcr_crc_key),
                               (void*)pRes)) == LDAP_SUCCESS) {
	    memcache_add_to_list(cache, pRes, LIST_TTL);
	    memcache_add_to_list(cache, pRes, LIST_LRU);
	} else {
	    memcache_free_entry(cache, pRes);
	}
    }
    /* Search for cached entries for a particular search. */
    else if (mode == MEMCACHE_ACCESS_FIND) {

	ldapmemcacheRes **ppRes = (ldapmemcacheRes**)pData2;

	nRes = htable_get(cache->ldmemc_resLookup, pData1, (void**)ppRes);
	if (nRes != LDAP_SUCCESS)
	    return nRes;

	if (!memcache_expired(cache, *ppRes, (unsigned long)time(0))) {
	    memcache_free_from_list(cache, *ppRes, LIST_LRU);
	    memcache_add_to_list(cache, *ppRes, LIST_LRU);
	    return( LDAP_SUCCESS );
	}

	nRes = htable_remove(cache->ldmemc_resLookup, pData1, NULL);
	assert(nRes == LDAP_SUCCESS);
	memcache_free_from_list(cache, *ppRes, LIST_TTL);
	memcache_free_from_list(cache, *ppRes, LIST_LRU);
	memcache_free_entry(cache, *ppRes);
	nRes = LDAP_NO_SUCH_OBJECT;
	*ppRes = NULL;
    }
    /* Remove cached entries in the temporary cache. */
    else if (mode == MEMCACHE_ACCESS_DELETE) {

	void* hashResult = NULL;

	if ((nRes = htable_remove(cache->ldmemc_resTmp, pData1,
	                          &hashResult)) == LDAP_SUCCESS) {
	    ldapmemcacheRes *pCurRes = (ldapmemcacheRes*) hashResult;
	    memcache_free_from_list(cache, pCurRes, LIST_TMP);
	    memcache_free_entry(cache, pCurRes);
	}
    }
    /* Wipe out the temporary cache. */
    else if (mode == MEMCACHE_ACCESS_DELETE_ALL) {

	nRes = htable_removeall(cache->ldmemc_resTmp, (void*)cache);
    }
    /* Remove expired entries from primary cache. */
    else if (mode == MEMCACHE_ACCESS_UPDATE) {

	ldapmemcacheRes *pCurRes = cache->ldmemc_resTail[LIST_TTL];
	unsigned long curTime = (unsigned long)time(NULL);

	for (; pCurRes; pCurRes = cache->ldmemc_resTail[LIST_TTL]) {

	    if (!memcache_expired(cache, pCurRes, curTime))
		break;

	    nRes = htable_remove(cache->ldmemc_resLookup, 
		          (void*)&(pCurRes->ldmemcr_crc_key), NULL);
	    assert(nRes == LDAP_SUCCESS);
	    memcache_free_from_list(cache, pCurRes, LIST_TTL);
	    memcache_free_from_list(cache, pCurRes, LIST_LRU);
	    memcache_free_entry(cache, pCurRes);
	}
    }
    /* Wipe out the primary cache. */
    else if (mode == MEMCACHE_ACCESS_FLUSH_ALL) {

	ldapmemcacheRes *pCurRes = cache->ldmemc_resHead[LIST_TTL];

	nRes = htable_removeall(cache->ldmemc_resLookup, (void*)cache);

	for (; pCurRes; pCurRes = cache->ldmemc_resHead[LIST_TTL]) {
	    memcache_free_from_list(cache, pCurRes, LIST_LRU);
	    cache->ldmemc_resHead[LIST_TTL] = 
		      cache->ldmemc_resHead[LIST_TTL]->ldmemcr_next[LIST_TTL];
	    memcache_free_entry(cache, pCurRes);
	}
	cache->ldmemc_resTail[LIST_TTL] = NULL;
    }
    /* Remove cached entries in both primary and temporary cache. */
    else if (mode == MEMCACHE_ACCESS_FLUSH) {

	int i, list_id, bDone;
	int scope = (int)pData2;
	char *dn = (char*)pData1;
	char *dnTmp;
	BerElement ber;
	LDAPMessage *pMsg;
	ldapmemcacheRes *pRes;

	if (cache->ldmemc_basedns) {
	    for (i = 0; cache->ldmemc_basedns[i]; i++) {
		if ((memcache_compare_dn(cache->ldmemc_basedns[i], dn, 
			    LDAP_SCOPE_SUBTREE) == LDAP_COMPARE_TRUE) ||
		    (memcache_compare_dn(dn, cache->ldmemc_basedns[i], 
			    LDAP_SCOPE_SUBTREE) == LDAP_COMPARE_TRUE))
		    break;
	    }
	    if (cache->ldmemc_basedns[i] == NULL)
		return( LDAP_SUCCESS );
	}

	for (i = 0; i < 2; i++) {

	    list_id = (i == 0 ? LIST_TTL : LIST_TMP);

	    for (pRes = cache->ldmemc_resHead[list_id]; pRes != NULL; 
		 pRes = pRes->ldmemcr_next[list_id]) {

		if ((memcache_compare_dn(pRes->ldmemcr_basedn, dn, 
				 LDAP_SCOPE_SUBTREE) != LDAP_COMPARE_TRUE) &&
		    (memcache_compare_dn(dn, pRes->ldmemcr_basedn, 
				 LDAP_SCOPE_SUBTREE) != LDAP_COMPARE_TRUE))
		    continue;

		for (pMsg = pRes->ldmemcr_resHead, bDone = 0; 
		     !bDone && pMsg; pMsg = pMsg->lm_chain) {

		    if (!NSLDAPI_IS_SEARCH_ENTRY( pMsg->lm_msgtype ))
			continue;

		    ber = *(pMsg->lm_ber);
		    if (ber_scanf(&ber, "{a", &dnTmp) != LBER_ERROR) {
			bDone = (memcache_compare_dn(dnTmp, dn, scope) == 
							    LDAP_COMPARE_TRUE);
			ldap_memfree(dnTmp);
		    }
		}

		if (!bDone)
		    continue;

		if (list_id == LIST_TTL) {
		    nRes = htable_remove(cache->ldmemc_resLookup, 
			  	 (void*)&(pRes->ldmemcr_crc_key), NULL);
		    assert(nRes == LDAP_SUCCESS);
		    memcache_free_from_list(cache, pRes, LIST_TTL);
		    memcache_free_from_list(cache, pRes, LIST_LRU);
		} else {
		    nRes = htable_remove(cache->ldmemc_resTmp, 
				  (void*)&(pRes->ldmemcr_req_id), NULL);
		    assert(nRes == LDAP_SUCCESS);
		    memcache_free_from_list(cache, pRes, LIST_TMP);
		}
		memcache_free_entry(cache, pRes);
	    }
	}
    }
    /* Flush least recently used entries from cache */
    else if (mode == MEMCACHE_ACCESS_FLUSH_LRU) {

	ldapmemcacheRes *pRes = cache->ldmemc_resTail[LIST_LRU];

	if (pRes == NULL)
	    return LDAP_NO_SUCH_OBJECT;

	LDAPDebug( LDAP_DEBUG_TRACE,
		"memcache_access FLUSH_LRU: removing key 0x%8.8lx\n",
		pRes->ldmemcr_crc_key, 0, 0 );
	nRes = htable_remove(cache->ldmemc_resLookup,
	              (void*)&(pRes->ldmemcr_crc_key), NULL);
	assert(nRes == LDAP_SUCCESS);
	memcache_free_from_list(cache, pRes, LIST_TTL);
	memcache_free_from_list(cache, pRes, LIST_LRU);
	memcache_free_entry(cache, pRes);
    }
    /* Unknown command */
    else {
	nRes = LDAP_PARAM_ERROR;
    }

    return nRes;
}


#ifdef LDAP_DEBUG
static void
memcache_report_statistics( LDAPMemCache *cache )
{
    unsigned long	hitrate;

    if ( cache->ldmemc_stats.ldmemcstat_tries == 0 ) {
	hitrate = 0;
    } else {
	hitrate = ( 100L * cache->ldmemc_stats.ldmemcstat_hits ) /
	    cache->ldmemc_stats.ldmemcstat_tries;
    }
    LDAPDebug( LDAP_DEBUG_STATS, "memcache 0x%x:\n", cache, 0, 0 );
    LDAPDebug( LDAP_DEBUG_STATS, "    tries: %ld  hits: %ld  hitrate: %ld%%\n",
	    cache->ldmemc_stats.ldmemcstat_tries,
	    cache->ldmemc_stats.ldmemcstat_hits, hitrate );
    if ( cache->ldmemc_size <= 0 ) {	/* no size limit */
	LDAPDebug( LDAP_DEBUG_STATS, "    memory bytes used: %ld\n",
		cache->ldmemc_size_used, 0, 0 );
    } else {
	LDAPDebug( LDAP_DEBUG_STATS, "    memory bytes used: %ld free: %ld\n",
		cache->ldmemc_size_used,
		cache->ldmemc_size - cache->ldmemc_size_used, 0 );
    }
}
#endif /* LDAP_DEBUG */

/************************ Hash Table Functions *****************************/

/* Calculates size (# of entries) of hash table given the size limit for
   the cache. */
static int
htable_calculate_size(int sizelimit)
{
    int i, j;
    int size = (int)(((double)sizelimit / 
	                (double)(sizeof(BerElement) + EXTRA_SIZE)) / 1.5);

    /* Get a prime # */
    size = (size & 0x1 ? size : size + 1);
    for (i = 3, j = size / 2; i < j; i++) {
	if ((size % i) == 0) {
	    size += 2;
	    i = 3;
	    j = size / 2;
	}
    }

    return size;
}

/* Returns the size in bytes of the given hash table. */
static int
htable_sizeinbytes(HashTable *pTable)
{
    if (!pTable)
	return 0;

    return (pTable->size * sizeof(HashTableNode));
}

/* Inserts an item into the hash table. */
static int
htable_put(HashTable *pTable, void *key, void *pData)
{
    int index = pTable->hashfunc(pTable->size, key);

    if (index >= 0 && index < pTable->size)
	return pTable->putdata(&(pTable->table[index].pData), key, pData);

    return( LDAP_OPERATIONS_ERROR );
}

/* Retrieves an item from the hash table. */
static int
htable_get(HashTable *pTable, void *key, void **ppData)
{
    int index = pTable->hashfunc(pTable->size, key);

    *ppData = NULL;

    if (index >= 0 && index < pTable->size)
	return pTable->getdata(pTable->table[index].pData, key, ppData);

    return( LDAP_OPERATIONS_ERROR );
}

/* Performs a miscellaneous operation on a hash table entry. */
static int
htable_misc(HashTable *pTable, void *key, void *pData)
{
    if (pTable->miscfunc) {
	int index = pTable->hashfunc(pTable->size, key);
	if (index >= 0 && index < pTable->size)
	    return pTable->miscfunc(&(pTable->table[index].pData), key, pData);
    }

    return( LDAP_OPERATIONS_ERROR );
}

/* Removes an item from the hash table. */
static int
htable_remove(HashTable *pTable, void *key, void **ppData)
{
    int index = pTable->hashfunc(pTable->size, key);

    if (ppData)
	*ppData = NULL;

    if (index >= 0 && index < pTable->size)
	return pTable->removedata(&(pTable->table[index].pData), key, ppData);

    return( LDAP_OPERATIONS_ERROR );
}

/* Removes everything in the hash table. */
static int
htable_removeall(HashTable *pTable, void *pData)
{
    int i;

    for (i = 0; i < pTable->size; i++)
	pTable->clrtablenode(&(pTable->table[i].pData), pData);

    return( LDAP_SUCCESS );
}

/* Creates a new hash table. */
static int
htable_create(int size_limit, HashFuncPtr hashf,
			 PutDataPtr putDataf, GetDataPtr getDataf,
			 RemoveDataPtr removeDataf, ClrTableNodePtr clrNodef,
			 MiscFuncPtr miscOpf, HashTable **ppTable)
{
    size_limit = htable_calculate_size(size_limit);

    if ((*ppTable = (HashTable*)NSLDAPI_CALLOC(1, sizeof(HashTable))) == NULL)
	return( LDAP_NO_MEMORY );

    (*ppTable)->table = (HashTableNode*)NSLDAPI_CALLOC(size_limit, 
	                                       sizeof(HashTableNode));
    if ((*ppTable)->table == NULL) {
	NSLDAPI_FREE(*ppTable);
	*ppTable = NULL;
	return( LDAP_NO_MEMORY );
    }

    (*ppTable)->size = size_limit;
    (*ppTable)->hashfunc = hashf;
    (*ppTable)->putdata = putDataf;
    (*ppTable)->getdata = getDataf;
    (*ppTable)->miscfunc = miscOpf;
    (*ppTable)->removedata = removeDataf;
    (*ppTable)->clrtablenode = clrNodef;

    return( LDAP_SUCCESS );
}

/* Destroys a hash table. */
static int
htable_free(HashTable *pTable)
{
    NSLDAPI_FREE(pTable->table);
    NSLDAPI_FREE(pTable);
    return( LDAP_SUCCESS );
}

/**************** Hash table callbacks for temporary cache ****************/

/* Hash function */
static int
msgid_hashf(int table_size, void *key)
{
    unsigned code = (unsigned)((ldapmemcacheReqId*)key)->ldmemcrid_ld;
    return (((code << 20) + (code >> 12)) % table_size);
}

/* Called by hash table to insert an item. */
static int
msgid_putdata(void **ppTableData, void *key, void *pData)
{
    ldapmemcacheReqId *pReqId = (ldapmemcacheReqId*)key;
    ldapmemcacheRes *pRes = (ldapmemcacheRes*)pData;
    ldapmemcacheRes **ppHead = (ldapmemcacheRes**)ppTableData;
    ldapmemcacheRes *pCurRes = *ppHead;
    ldapmemcacheRes *pPrev = NULL;

    for (; pCurRes; pCurRes = pCurRes->ldmemcr_htable_next) {
	if ((pCurRes->ldmemcr_req_id).ldmemcrid_ld == pReqId->ldmemcrid_ld)
	    break;
	pPrev = pCurRes;
    }

    if (pCurRes) {
	for (; pCurRes; pCurRes = pCurRes->ldmemcr_next[LIST_TTL]) { 
	    if ((pCurRes->ldmemcr_req_id).ldmemcrid_msgid == 
		                               pReqId->ldmemcrid_msgid)
		return( LDAP_ALREADY_EXISTS );
	    pPrev = pCurRes;
	}
	pPrev->ldmemcr_next[LIST_TTL] = pRes;
	pRes->ldmemcr_prev[LIST_TTL] = pPrev;
	pRes->ldmemcr_next[LIST_TTL] = NULL;
    } else {
	if (pPrev)
	    pPrev->ldmemcr_htable_next = pRes;
	else
	    *ppHead = pRes;
	pRes->ldmemcr_htable_next = NULL;
    }

    return( LDAP_SUCCESS );
}

/* Called by hash table to retrieve an item. */
static int
msgid_getdata(void *pTableData, void *key, void **ppData)
{
    ldapmemcacheReqId *pReqId = (ldapmemcacheReqId*)key;
    ldapmemcacheRes *pCurRes = (ldapmemcacheRes*)pTableData;

    *ppData = NULL;
    
    for (; pCurRes; pCurRes = pCurRes->ldmemcr_htable_next) {
	if ((pCurRes->ldmemcr_req_id).ldmemcrid_ld == pReqId->ldmemcrid_ld)
	    break;
    }

    if (!pCurRes)
	return( LDAP_NO_SUCH_OBJECT );

    for (; pCurRes; pCurRes = pCurRes->ldmemcr_next[LIST_TTL]) { 
	if ((pCurRes->ldmemcr_req_id).ldmemcrid_msgid == 
	                                  pReqId->ldmemcrid_msgid) {
	    *ppData = (void*)pCurRes;
	    return( LDAP_SUCCESS );
	}
    }

    return( LDAP_NO_SUCH_OBJECT );
}

/* Called by hash table to remove an item. */
static int
msgid_removedata(void **ppTableData, void *key, void **ppData)
{
    ldapmemcacheRes *pHead = *((ldapmemcacheRes**)ppTableData);
    ldapmemcacheRes *pCurRes = NULL;
    ldapmemcacheRes *pPrev = NULL;
    ldapmemcacheReqId *pReqId = (ldapmemcacheReqId*)key;

    if (ppData)
	*ppData = NULL;

    for (; pHead; pHead = pHead->ldmemcr_htable_next) {
	if ((pHead->ldmemcr_req_id).ldmemcrid_ld == pReqId->ldmemcrid_ld)
	    break;
	pPrev = pHead;
    }

    if (!pHead)
	return( LDAP_NO_SUCH_OBJECT );

    for (pCurRes = pHead; pCurRes; pCurRes = pCurRes->ldmemcr_next[LIST_TTL]) { 
	if ((pCurRes->ldmemcr_req_id).ldmemcrid_msgid == 
	                                        pReqId->ldmemcrid_msgid)
	    break;
    }

    if (!pCurRes)
	return( LDAP_NO_SUCH_OBJECT );

    if (ppData) {
	pCurRes->ldmemcr_next[LIST_TTL] = NULL;
	pCurRes->ldmemcr_prev[LIST_TTL] = NULL;
	pCurRes->ldmemcr_htable_next = NULL;
	*ppData = (void*)pCurRes;
    }

    if (pCurRes != pHead) {
	if (pCurRes->ldmemcr_prev[LIST_TTL])
	    pCurRes->ldmemcr_prev[LIST_TTL]->ldmemcr_next[LIST_TTL] = 
	                                     pCurRes->ldmemcr_next[LIST_TTL];
	if (pCurRes->ldmemcr_next[LIST_TTL])
	    pCurRes->ldmemcr_next[LIST_TTL]->ldmemcr_prev[LIST_TTL] = 
	                                     pCurRes->ldmemcr_prev[LIST_TTL];
	return( LDAP_SUCCESS );
    }

    if (pPrev) {
	if (pHead->ldmemcr_next[LIST_TTL]) {
	    pPrev->ldmemcr_htable_next = pHead->ldmemcr_next[LIST_TTL];
	    pHead->ldmemcr_next[LIST_TTL]->ldmemcr_htable_next = 
			                   pHead->ldmemcr_htable_next;
	} else {
	    pPrev->ldmemcr_htable_next = pHead->ldmemcr_htable_next;
	}
    } else {
	if (pHead->ldmemcr_next[LIST_TTL]) {
	    *((ldapmemcacheRes**)ppTableData) = pHead->ldmemcr_next[LIST_TTL];
	    pHead->ldmemcr_next[LIST_TTL]->ldmemcr_htable_next = 
			                   pHead->ldmemcr_htable_next;
	} else {
	    *((ldapmemcacheRes**)ppTableData) = pHead->ldmemcr_htable_next;
	}
    }
    
    return( LDAP_SUCCESS );
}

/* Called by hash table to remove all cached entries associated to searches
   being performed using the given ldap handle. */
static int
msgid_clear_ld_items(void **ppTableData, void *key, void *pData)
{
    LDAPMemCache *cache = (LDAPMemCache*)pData;
    ldapmemcacheRes *pHead = *((ldapmemcacheRes**)ppTableData);
    ldapmemcacheRes *pPrev = NULL;
    ldapmemcacheRes *pCurRes = NULL;
    ldapmemcacheReqId *pReqId = (ldapmemcacheReqId*)key;
    
    for (; pHead; pHead = pHead->ldmemcr_htable_next) {
	if ((pHead->ldmemcr_req_id).ldmemcrid_ld == pReqId->ldmemcrid_ld)
	    break;
	pPrev = pHead;
    }

    if (!pHead)
	return( LDAP_NO_SUCH_OBJECT );

    if (pPrev)
	pPrev->ldmemcr_htable_next = pHead->ldmemcr_htable_next;
    else
	*((ldapmemcacheRes**)ppTableData) = pHead->ldmemcr_htable_next;

    for (pCurRes = pHead; pHead; pCurRes = pHead) {
	pHead = pHead->ldmemcr_next[LIST_TTL];
	memcache_free_from_list(cache, pCurRes, LIST_TMP);
	memcache_free_entry(cache, pCurRes);
    }

    return( LDAP_SUCCESS );
}

/* Called by hash table for removing all items in the table. */
static void
msgid_clearnode(void **ppTableData, void *pData)
{
    LDAPMemCache *cache = (LDAPMemCache*)pData;
    ldapmemcacheRes **ppHead = (ldapmemcacheRes**)ppTableData;
    ldapmemcacheRes *pSubHead = *ppHead;
    ldapmemcacheRes *pCurRes = NULL;

    for (; *ppHead; pSubHead = *ppHead) {
	ppHead = &((*ppHead)->ldmemcr_htable_next);
	for (pCurRes = pSubHead; pSubHead; pCurRes = pSubHead) {
	    pSubHead = pSubHead->ldmemcr_next[LIST_TTL];
	    memcache_free_from_list(cache, pCurRes, LIST_TMP);
	    memcache_free_entry(cache, pCurRes);
	}
    }
}

/********************* Hash table for primary cache ************************/

/* Hash function */
static int
attrkey_hashf(int table_size, void *key)
{
    return ((*((unsigned long*)key)) % table_size);
}

/* Called by hash table to insert an item. */
static int
attrkey_putdata(void **ppTableData, void *key, void *pData)
{
    unsigned long attrkey = *((unsigned long*)key);
    ldapmemcacheRes **ppHead = (ldapmemcacheRes**)ppTableData;
    ldapmemcacheRes *pRes = *ppHead;

    for (; pRes; pRes = pRes->ldmemcr_htable_next) {
	if (pRes->ldmemcr_crc_key == attrkey)
	    return( LDAP_ALREADY_EXISTS );
    }

    pRes = (ldapmemcacheRes*)pData;
    pRes->ldmemcr_htable_next = *ppHead;
    *ppHead = pRes;

    return( LDAP_SUCCESS );
}

/* Called by hash table to retrieve an item. */
static int
attrkey_getdata(void *pTableData, void *key, void **ppData)
{
    unsigned long attrkey = *((unsigned long*)key);
    ldapmemcacheRes *pRes = (ldapmemcacheRes*)pTableData;
    
    for (; pRes; pRes = pRes->ldmemcr_htable_next) {
	if (pRes->ldmemcr_crc_key == attrkey) {
	    *ppData = (void*)pRes;
	    return( LDAP_SUCCESS );
	}
    }

    *ppData = NULL;

    return( LDAP_NO_SUCH_OBJECT );
}

/* Called by hash table to remove an item. */
static int
attrkey_removedata(void **ppTableData, void *key, void **ppData)
{
    unsigned long attrkey = *((unsigned long*)key);
    ldapmemcacheRes **ppHead = (ldapmemcacheRes**)ppTableData;
    ldapmemcacheRes *pRes = *ppHead;
    ldapmemcacheRes *pPrev = NULL;
    
    for (; pRes; pRes = pRes->ldmemcr_htable_next) {
	if (pRes->ldmemcr_crc_key == attrkey) {
	    if (ppData)
		*ppData = (void*)pRes;
	    if (pPrev)
		pPrev->ldmemcr_htable_next = pRes->ldmemcr_htable_next;
	    else
		*ppHead = pRes->ldmemcr_htable_next;
	    pRes->ldmemcr_htable_next = NULL;
	    return( LDAP_SUCCESS );
	}
	pPrev = pRes;
    }

    if (ppData)
	*ppData = NULL;

    return( LDAP_NO_SUCH_OBJECT );
}

/* Called by hash table for removing all items in the table. */
static void
attrkey_clearnode(void **ppTableData, void *pData)
{
    ldapmemcacheRes **ppHead = (ldapmemcacheRes**)ppTableData;
    ldapmemcacheRes *pRes = *ppHead;

    (void)pData;

    for (; *ppHead; pRes = *ppHead) {
	ppHead = &((*ppHead)->ldmemcr_htable_next);
	pRes->ldmemcr_htable_next = NULL;
    }
}

/***************************** CRC algorithm ********************************/

/* From http://www.faqs.org/faqs/compression-faq/part1/section-25.html */

/*
 * Build auxiliary table for parallel byte-at-a-time CRC-32.
 */
#define NSLDAPI_CRC32_POLY 0x04c11db7     /* AUTODIN II, Ethernet, & FDDI */

static nsldapi_uint_32 crc32_table[256] = { 
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 
    0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7, 
    0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75, 
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 
    0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039, 
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef, 
    0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d, 
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 
    0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0, 
    0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072, 
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 
    0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08, 
    0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba, 
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 
    0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6, 
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050, 
    0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2, 
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34, 
    0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1, 
    0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53, 
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 
    0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9, 
    0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b, 
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 
    0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71, 
    0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3, 
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 
    0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e, 
    0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec, 
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 
    0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676, 
    0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4, 
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 
    0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668, 
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4 };

/* Initialized first time "crc32()" is called. If you prefer, you can
 * statically initialize it at compile time. [Another exercise.]
 */

static unsigned long
crc32_convert(char *buf, int len)
{
    unsigned char *p;
	nsldapi_uint_32	crc;

    crc = 0xffffffff;       /* preload shift register, per CRC-32 spec */
    for (p = (unsigned char *)buf; len > 0; ++p, --len)
	crc = ((crc << 8) ^ crc32_table[(crc >> 24) ^ *p]) & 0xffffffff;

    return (unsigned long) ~crc;    /* transmit complement, per CRC-32 spec */
}
