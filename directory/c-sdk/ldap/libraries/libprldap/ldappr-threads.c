/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * Thread callback functions for libldap that use the NSPR (Netscape
 * Portable Runtime) thread API.
 *
 */

#include "ldappr-int.h"

/*
 * Macros:
 */
/*
 * Grow thread private data arrays 10 elements at a time.
 */
#define PRLDAP_TPD_ARRAY_INCREMENT	10

/*
 * Structures and types:
 */
/*
 * Structure used by libldap thread callbacks to maintain error information.
 */
typedef struct prldap_errorinfo {
    int		plei_magic;	/* must be first in the structure */
    int		plei_lderrno;
    char	*plei_matched;
    char	*plei_errmsg;
} PRLDAP_ErrorInfo;

#define PRLDAP_ERRORINFO_MAGIC	0x4D4F5A45	/* 'MOZE' */


/*
 * Structure used to maintain thread-private data. At the present time,
 * only error info. is thread-private.  One of these structures is allocated
 * for each thread.
 */
typedef struct prldap_tpd_header {
    int			ptpdh_tpd_count;	/* # of data items allocated */
    void		**ptpdh_dataitems;	/* array of data items */
} PRLDAP_TPDHeader;

/*
 * Structure used by associate a PRLDAP thread-private data index with an
 * LDAP session handle. One of these exists for each active LDAP session
 * handle.
 */
typedef struct prldap_tpd_map {
    LDAP			*prtm_ld;	/* non-NULL if in use */
    PRUintn			prtm_index;	/* index into TPD array */
    struct prldap_tpd_map	*prtm_next;
} PRLDAP_TPDMap;



/*
 * Static Variables:
 */
/*
 * prldap_map_list points to all of the PRLDAP_TPDMap structures
 * we have ever allocated.  We recycle them as we open and close LDAP
 * sessions.
 */
static PRLDAP_TPDMap *prldap_map_list = NULL;


/*
 * The prldap_map_mutex is used to protect access to the prldap_map_list.
 */
static PRLock	*prldap_map_mutex = NULL;

/*
 * The prldap_tpd_maxindex value is used to track the largest TPD array
 * index we have used.
 */
static PRInt32	prldap_tpd_maxindex = -1;

/*
 * prldap_tpdindex is an NSPR thread private data index we use to
 * maintain our own thread-private data. It is initialized inside
 * prldap_init_tpd().
 */
static PRUintn	prldap_tpdindex = 0;

/*
 * The prldap_callonce_init_tpd structure is used by NSPR to ensure
 * that prldap_init_tpd() is called at most once.
 */
static PRCallOnceType prldap_callonce_init_tpd = { 0, 0, 0 };


/*
 * Private function prototypes:
 */
static void prldap_set_ld_error( int err, char *matched, char *errmsg,
	void *errorarg );
static int prldap_get_ld_error( char **matchedp, char **errmsgp,
	void *errorarg );
static void *prldap_mutex_alloc( void );
static void prldap_mutex_free( void *mutex );
static int prldap_mutex_lock( void *mutex );
static int prldap_mutex_unlock( void *mutex );
static void *prldap_get_thread_id( void );
static PRStatus prldap_init_tpd( void );
static PRLDAP_TPDMap *prldap_allocate_map( LDAP *ld );
static void prldap_return_map( PRLDAP_TPDMap *map );
static PRUintn prldap_new_tpdindex( void );
static int prldap_set_thread_private( PRInt32 tpdindex, void *priv );
static void *prldap_get_thread_private( PRInt32 tpdindex );
static PRLDAP_TPDHeader *prldap_tsd_realloc( PRLDAP_TPDHeader *tsdhdr,
	int maxindex );
static void prldap_tsd_destroy( void *priv );


/*
 * Install NSPR thread functions into ld (if ld is NULL, they are installed
 * as the default functions for new LDAP * handles).
 *
 * Returns 0 if all goes well and -1 if not.
 */
int
prldap_install_thread_functions( LDAP *ld, int shared )
{
    struct ldap_thread_fns		tfns;
    struct ldap_extra_thread_fns	xtfns;

    if ( PR_CallOnce( &prldap_callonce_init_tpd, prldap_init_tpd )
		!= PR_SUCCESS ) {
	ldap_set_lderrno( ld, LDAP_LOCAL_ERROR, NULL, NULL );
	return( -1 );
    }

    /* set thread function pointers */
    memset( &tfns, '\0', sizeof(struct ldap_thread_fns) );
    tfns.ltf_get_errno = prldap_get_system_errno;
    tfns.ltf_set_errno = prldap_set_system_errno;
    if ( shared ) {
	tfns.ltf_mutex_alloc = prldap_mutex_alloc;
	tfns.ltf_mutex_free = prldap_mutex_free;
	tfns.ltf_mutex_lock = prldap_mutex_lock;
	tfns.ltf_mutex_unlock = prldap_mutex_unlock;
	tfns.ltf_get_lderrno = prldap_get_ld_error;
	tfns.ltf_set_lderrno = prldap_set_ld_error;
	if ( ld != NULL ) {
	    /*
	     * If this is a real ld (i.e., we are not setting the global
	     * defaults) allocate thread private data for error information.
	     * If ld is NULL we do not do this here but it is done in
	     * prldap_thread_new_handle().
	     */
	    if (( tfns.ltf_lderrno_arg = (void *)prldap_allocate_map( ld ))
		    == NULL ) {
		return( -1 );
	    }
	}
    }

    if ( ldap_set_option( ld, LDAP_OPT_THREAD_FN_PTRS,
	    (void *)&tfns ) != 0 ) {
	prldap_return_map( (PRLDAP_TPDMap *)tfns.ltf_lderrno_arg );
	return( -1 );
    }

    /* set extended thread function pointers */
    memset( &xtfns, '\0', sizeof(struct ldap_extra_thread_fns) );
    xtfns.ltf_threadid_fn = prldap_get_thread_id;
    if ( ldap_set_option( ld, LDAP_OPT_EXTRA_THREAD_FN_PTRS,
	    (void *)&xtfns ) != 0 ) {
	return( -1 );
    }

    return( 0 );
}


static void *
prldap_mutex_alloc( void )
{
    return( (void *)PR_NewLock());
}


static void
prldap_mutex_free( void *mutex )
{
    PR_DestroyLock( (PRLock *)mutex );
}


static int
prldap_mutex_lock( void *mutex )
{
    PR_Lock( (PRLock *)mutex );
    return( 0 );
}


static int
prldap_mutex_unlock( void *mutex )
{
    if ( PR_Unlock( (PRLock *)mutex ) == PR_FAILURE ) {
	return( -1 );
    }

    return( 0 );
}


static void *
prldap_get_thread_id( void )
{
    return( (void *)PR_GetCurrentThread());
}


static int
prldap_get_ld_error( char **matchedp, char **errmsgp, void *errorarg )
{
    PRLDAP_TPDMap	*map;
    PRLDAP_ErrorInfo	*eip;

    if (( map = (PRLDAP_TPDMap *)errorarg ) != NULL && ( eip =
	    (PRLDAP_ErrorInfo *)prldap_get_thread_private(
	    map->prtm_index )) != NULL ) {
	if ( matchedp != NULL ) {
	    *matchedp = eip->plei_matched;
	}
	if ( errmsgp != NULL ) {
	    *errmsgp = eip->plei_errmsg;
	}
	return( eip->plei_lderrno );
    } else {
	if ( matchedp != NULL ) {
	    *matchedp = NULL;
	}
	if ( errmsgp != NULL ) {
	    *errmsgp = NULL;
	}
	return( LDAP_LOCAL_ERROR );	/* punt */
    }
}


static void
prldap_set_ld_error( int err, char *matched, char *errmsg, void *errorarg )
{
    PRLDAP_TPDMap	*map;
    PRLDAP_ErrorInfo	*eip;

    if (( map = (PRLDAP_TPDMap *)errorarg ) != NULL ) {
	if (( eip = (PRLDAP_ErrorInfo *)prldap_get_thread_private(
		map->prtm_index )) == NULL ) {
	    /*
	     * Error info. has not yet been allocated for this thread.
	     * Do so now.  Note that we free this memory only for the
	     * thread that calls prldap_thread_dispose_handle(), which
	     * should be the one that called ldap_unbind() -- see
	     * prldap_return_map().  Not freeing the memory used by
	     * other threads is deemed acceptable since it will be
	     * recycled and used by other LDAP sessions.  All of the
	     * thread-private memory is freed when a thread exits
	     * (inside the prldap_tsd_destroy() function).
	     */
	    eip = (PRLDAP_ErrorInfo *)PR_Calloc( 1,
		    sizeof( PRLDAP_ErrorInfo ));
	    if ( eip == NULL ) {
		return;	/* punt */
	    }
	    eip->plei_magic = PRLDAP_ERRORINFO_MAGIC;
	    (void)prldap_set_thread_private( map->prtm_index, eip );
	}

	eip->plei_lderrno = err;

	if ( eip->plei_matched != NULL ) {
	    ldap_memfree( eip->plei_matched );
	}
	eip->plei_matched = matched;

	if ( eip->plei_errmsg != NULL ) {
	    ldap_memfree( eip->plei_errmsg );
	}
	eip->plei_errmsg = errmsg;
    }
}


/*
 * Utility function to free a PRLDAP_ErrorInfo structure and everything
 * it contains.
 */
static void
prldap_free_errorinfo( PRLDAP_ErrorInfo *eip )
{
    if ( NULL != eip && PRLDAP_ERRORINFO_MAGIC == eip->plei_magic ) {
	if ( eip->plei_matched != NULL ) {
	    ldap_memfree( eip->plei_matched );
	}
	if ( eip->plei_errmsg != NULL ) {
	    ldap_memfree( eip->plei_errmsg );
	}

	PR_Free( eip );
    }
}


/*
 * Called when a new LDAP * session handle is allocated.
 * Allocate thread-private data for error information, but only if
 * it has not already been allocated and the get_ld_error callback has
 * been installed.  If ld is not NULL when prldap_install_thread_functions()
 * is called, we will have already allocated the thread-private data there.
 */
int
prldap_thread_new_handle( LDAP *ld, void *sessionarg )
{
    struct ldap_thread_fns	tfns;

    if ( ldap_get_option( ld, LDAP_OPT_THREAD_FN_PTRS, (void *)&tfns ) != 0 ) {
	return( LDAP_LOCAL_ERROR );
    }

    if ( tfns.ltf_lderrno_arg == NULL && tfns.ltf_get_lderrno != NULL ) {
	if (( tfns.ltf_lderrno_arg = (void *)prldap_allocate_map( ld )) == NULL
		|| ldap_set_option( ld, LDAP_OPT_THREAD_FN_PTRS,
		(void *)&tfns ) != 0 ) {
	    return( LDAP_LOCAL_ERROR );
	}
    }

    return( LDAP_SUCCESS );
}


/*
 * Called when an LDAP * session handle is being destroyed.
 * Clean up our thread private data map.
 */
void
prldap_thread_dispose_handle( LDAP *ld, void *sessionarg )
{
    struct ldap_thread_fns	tfns;

    if ( ldap_get_option( ld, LDAP_OPT_THREAD_FN_PTRS,
	    (void *)&tfns ) == 0 &&
	    tfns.ltf_lderrno_arg != NULL ) {
	prldap_return_map( (PRLDAP_TPDMap *)tfns.ltf_lderrno_arg );
    }
}


static PRStatus
prldap_init_tpd( void )
{
    if (( prldap_map_mutex = PR_NewLock()) == NULL || PR_NewThreadPrivateIndex(
		&prldap_tpdindex, prldap_tsd_destroy ) != PR_SUCCESS ) {
	return( PR_FAILURE );
    }

    prldap_map_list = NULL;

    return( PR_SUCCESS );
}


/*
 * Function: prldap_allocate_map()
 * Description: allocate a thread-private data map to use for a new
 *	LDAP session handle.
 * Returns: a pointer to the TPD map or NULL if none available.
 */
static PRLDAP_TPDMap *
prldap_allocate_map( LDAP *ld )
{
    PRLDAP_TPDMap	*map, *prevmap;

    PR_Lock( prldap_map_mutex );

    /*
     * first look for a map that is already allocated but free to be re-used
     */
    prevmap = NULL;
    for ( map = prldap_map_list; map != NULL; map = map->prtm_next ) {
	if ( map->prtm_ld == NULL ) {
	    break;
	}
	prevmap = map;
    }

    /*
     * if none was found (map == NULL), try to allocate a new one and add it
     * to the end of our global list.
     */
    if ( map == NULL ) {
	PRUintn	tpdindex;

	tpdindex = prldap_new_tpdindex();
	map = (PRLDAP_TPDMap *)PR_Malloc( sizeof( PRLDAP_TPDMap ));
	if ( map != NULL ) {
	    map->prtm_index = tpdindex;
	    map->prtm_next = NULL;
	    if ( prevmap == NULL ) {
		prldap_map_list = map;
	    } else {
		prevmap->prtm_next = map;
	    }
	}
    }

    if ( map != NULL ) {
	map->prtm_ld = ld;	/* now marked as "in use" */

	/*
	 * If old thread-private error information exists, reset it. It may
	 * have been left behind by an old LDAP session that was used by
	 * this thread but disposed of by a different thread.
	 */
	if ( NULL != prldap_get_thread_private( map->prtm_index )) {
	    prldap_set_ld_error( LDAP_SUCCESS, NULL, NULL, map );
	}
    }

    PR_Unlock( prldap_map_mutex );

    return( map );
}


/*
 * Function: prldap_return_map()
 * Description: return a thread-private data map to the pool of ones
 *	available for re-use.
 */
static void
prldap_return_map( PRLDAP_TPDMap *map )
{
    PRLDAP_ErrorInfo	*eip;

    PR_Lock( prldap_map_mutex );

    /*
     * Dispose of thread-private LDAP error information.  Note that this
     * only disposes of the memory consumed on THIS thread, but that is
     * okay.  See the comment in prldap_set_ld_error() for the reason why.
     */
    if (( eip = (PRLDAP_ErrorInfo *)prldap_get_thread_private(
		map->prtm_index )) != NULL &&
		prldap_set_thread_private( map->prtm_index, NULL ) == 0 ) {
	prldap_free_errorinfo( eip );
    }

    /* mark map as available for re-use */
    map->prtm_ld = NULL;

    PR_Unlock( prldap_map_mutex );
}


/*
 * Function: prldap_new_tpdindex()
 * Description: allocate a thread-private data index.
 * Returns: the new index.
 */
static PRUintn
prldap_new_tpdindex( void )
{
    PRUintn	tpdindex;

    tpdindex = (PRUintn)PR_AtomicIncrement( &prldap_tpd_maxindex );
    return( tpdindex );
}


/*
 * Function: prldap_set_thread_private()
 * Description: store a piece of thread-private data.
 * Returns: 0 if successful and -1 if not.
 */
static int
prldap_set_thread_private( PRInt32 tpdindex, void *priv )
{
    PRLDAP_TPDHeader	*tsdhdr;

    if ( tpdindex > prldap_tpd_maxindex ) {
	return( -1 );	/* bad index */ 
    }

    tsdhdr = (PRLDAP_TPDHeader *)PR_GetThreadPrivate( prldap_tpdindex );
    if ( tsdhdr == NULL || tpdindex >= tsdhdr->ptpdh_tpd_count ) {
	tsdhdr = prldap_tsd_realloc( tsdhdr, tpdindex );
	if ( tsdhdr == NULL ) {
	    return( -1 );	/* realloc failed */
	}
    }

    tsdhdr->ptpdh_dataitems[ tpdindex ] = priv;
    return( 0 );
}


/*
 * Function: prldap_get_thread_private()
 * Description: retrieve a piece of thread-private data.  If not set,
 *	NULL is returned.
 * Returns: 0 if successful and -1 if not.
 */
static void *
prldap_get_thread_private( PRInt32 tpdindex )
{
    PRLDAP_TPDHeader	*tsdhdr;

    tsdhdr = (PRLDAP_TPDHeader *)PR_GetThreadPrivate( prldap_tpdindex );
    if ( tsdhdr == NULL ) {
	return( NULL );	/* no thread private data */
    }

    if ( tpdindex >= tsdhdr->ptpdh_tpd_count
		|| tsdhdr->ptpdh_dataitems == NULL ) {
	return( NULL );	/* fewer data items than requested index */
    }

    return( tsdhdr->ptpdh_dataitems[ tpdindex ] );
}


/*
 * Function: prldap_tsd_realloc()
 * Description: enlarge the thread-private data array.
 * Returns: the new PRLDAP_TPDHeader value (non-NULL if successful).
 * Note: tsdhdr can be NULL (allocates a new PRLDAP_TPDHeader).
 */
static PRLDAP_TPDHeader *
prldap_tsd_realloc( PRLDAP_TPDHeader *tsdhdr, int maxindex )
{
    void	*newdataitems = NULL;
    int		count;

    if ( tsdhdr == NULL ) {
	/* allocate a new thread private data header */
	if (( tsdhdr = PR_Calloc( 1, sizeof( PRLDAP_TPDHeader ))) == NULL ) {
	    return( NULL );
	}
	(void)PR_SetThreadPrivate( prldap_tpdindex, tsdhdr );
    }

    /*
     * Make the size of the new array the next highest multiple of
     * the array increment value that is greater than maxindex.
     */
    count = PRLDAP_TPD_ARRAY_INCREMENT *
		( 1 + ( maxindex / PRLDAP_TPD_ARRAY_INCREMENT ));

    /* increase the size of the data item array if necessary */
    if ( count > tsdhdr->ptpdh_tpd_count  ) {
	newdataitems = (PRLDAP_ErrorInfo *)PR_Calloc( count, sizeof( void * ));
	if ( newdataitems == NULL ) {
	    return( NULL );
	}
	if ( tsdhdr->ptpdh_dataitems != NULL ) {	/* preserve old data */
	    memcpy( newdataitems, tsdhdr->ptpdh_dataitems,
			tsdhdr->ptpdh_tpd_count * sizeof( void * ));
	    PR_Free( tsdhdr->ptpdh_dataitems );
	}

	tsdhdr->ptpdh_tpd_count = count;
	tsdhdr->ptpdh_dataitems = newdataitems;
    }

    return( tsdhdr );
}


/*
 * Function: prldap_tsd_destroy()
 * Description: Free a thread-private data array. Installed as an NSPR TPD
 *	destructor function
 * Returns: nothing.
 */
static void
prldap_tsd_destroy( void *priv )
{
    PRLDAP_TPDHeader	*tsdhdr;
    PRLDAP_ErrorInfo	*eip;
    int			i;

    tsdhdr = (PRLDAP_TPDHeader *)priv;
    if ( tsdhdr != NULL ) {
	if ( tsdhdr->ptpdh_dataitems != NULL ) {
	    for ( i = 0; i < tsdhdr->ptpdh_tpd_count; ++i ) {
		if ( tsdhdr->ptpdh_dataitems[ i ] != NULL ) {
		    eip = (PRLDAP_ErrorInfo *)tsdhdr->ptpdh_dataitems[ i ];
		    if ( PRLDAP_ERRORINFO_MAGIC == eip->plei_magic ) {
			prldap_free_errorinfo( eip );
		    } else {
			PR_Free( tsdhdr->ptpdh_dataitems[ i ] );
		    }
		    tsdhdr->ptpdh_dataitems[ i ] = NULL;
		}
	    }
	    PR_Free( tsdhdr->ptpdh_dataitems );
	    tsdhdr->ptpdh_dataitems = NULL;
	}
	PR_Free( tsdhdr );
    }
}
