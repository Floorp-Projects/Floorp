/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the mozilla.org LDAP XPCOM SDK.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>
 *                 Leif Hedstrom <leif@netscape.com>
 *                 Mark C. Smith <mcs@netscape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nspr.h"
#include "ldap.h"
/* XXXdmose errno.h should go away, probably after ldap C SDK 4.1 lands */
#include "errno.h"

/* one of these exists per thread */
struct nsLDAPLderrno {

    /* must be native (ie non-NSPR) types, because of the callback decls */
    int   mErrno;
    char   *mMatched;
    char   *mErrMsg;
};

static PRUintn kLDAPErrnoData = 0;
struct ldap_thread_fns kLDAPThreadFuncs;
struct ldap_extra_thread_fns kLDAPExtraThreadFuncs;

/**
 * initialize the thread-specific data for this thread (if necessary)
 *
 * @return 1 on success; 0 on failure
 */
int
nsLDAPThreadDataInit(void)
{
    void   *errnoData;

   /* if we don't have an index for this data, get one */
   
   if (kLDAPErrnoData == 0) {
       if (PR_NewThreadPrivateIndex(&kLDAPErrnoData, &PR_Free) 
           != PR_SUCCESS) {
#ifdef DEBUG
           PR_fprintf(PR_STDERR, "nsLDAPThreadDataInit(): error "
                      "allocating kLDAPErrorData thread-private index.\n");
#endif
           return 0;
       }
   }

   /* if the data for this thread has already been allocated, we're done */

   errnoData = PR_GetThreadPrivate(kLDAPErrnoData);
   if ( errnoData != NULL ) {
       return 1;
   }

   /* get some memory for the data for this thread */

   errnoData = PR_Calloc( 1, sizeof(struct nsLDAPLderrno));
   if (!errnoData) {
#ifdef DEBUG
       PR_fprintf(PR_STDERR, "nsLDAPThreadDataInit(): PR_calloc failed\n");
#endif
       return 0;
   }

   if ( PR_SetThreadPrivate(kLDAPErrnoData, errnoData) != PR_SUCCESS ) {
#ifdef DEBUG
       PR_fprintf(PR_STDERR, "nsLDAPThreadDataInit(): PR_SetThreadPrivate "
                  "failed\n");
#endif
       return 0;
   }

   return 1;
}

/**
 * Function for setting the (thread-specific) LDAP error. 
 */
static void
nsLDAPSetLderrno(int aErrno, char *aMatched, char *aErrMsg, void *aDummy)
{

    struct nsLDAPLderrno *nle;

    nle = PR_GetThreadPrivate(kLDAPErrnoData);
    if (!nle) {
#ifdef DEBUG    
        PR_fprintf(PR_STDERR, "nsLDAPSetLDAPErrno(): PR_GetThreadPrivate "
                   "failed\n");
#endif
        return;
    }

    nle->mErrno = aErrno;

    /* free any previous setting and replace it with the new one */
    if ( nle->mMatched != NULL ) {
        ldap_memfree( nle->mMatched );
    }
    nle->mMatched = aMatched;

    /* free any previous setting and replace it with the new one */
    if ( nle->mErrMsg != NULL ) {
      ldap_memfree( nle->mErrMsg );
    }

    nle->mErrMsg = aErrMsg;
}

/* Function for getting an LDAP error. */
static int
nsLDAPGetLderrno( char **aMatched, char **aErrMsg, void *aDummy )

{
   struct nsLDAPLderrno *nle;

   nle = PR_GetThreadPrivate(kLDAPErrnoData);
   if (!nle) {
#ifdef DEBUG
       PR_fprintf(PR_STDERR, "nsLDAPSetLDAPErrno(): PR_GetThreadPrivate "
                  "failed\n");
#endif
       return LDAP_OTHER;
   }

   if ( aMatched != NULL ) {
       *aMatched = nle->mMatched;
   }

   if ( aErrMsg != NULL ) {
       *aErrMsg = nle->mErrMsg;
   }

   return( nle->mErrno );
}

/*
 * these two functions work with pthreads.  they might work with other
 * threads too.  but maybe not.  ultimately, when LDAP C SDK 4.1
 * lands, we'll have NSPR functions used by the SDK, so that we can
 * use NSPR's errno, not the system one. (and that should be
 * cross-platform).
 */
static void
nsLDAPSetErrno( int aErr )
{
   errno = aErr;
}

static int
nsLDAPGetErrno( void )
{
   return errno;
}


/*
 * This function will be used by the C-SDK to get the thread ID. We
 * need this to avoid calling into the locking function multiple
 * times on the same lock.
 */
static void *
nsLDAPGetThreadID( void )
{
    return( (void *)PR_GetCurrentThread());
}

int
nsLDAPThreadFuncsInit(LDAP *aLDAP)
{
    kLDAPThreadFuncs.ltf_mutex_alloc = (void *(*)(void))PR_NewLock;
    kLDAPThreadFuncs.ltf_mutex_free = (void (*)(void *))PR_DestroyLock;
    kLDAPThreadFuncs.ltf_mutex_lock = (int (*)(void *))PR_Lock;
    kLDAPThreadFuncs.ltf_mutex_unlock = (int (*)(void *))PR_Unlock;
    kLDAPThreadFuncs.ltf_get_errno = nsLDAPGetErrno;
    kLDAPThreadFuncs.ltf_set_errno = nsLDAPSetErrno;
    kLDAPThreadFuncs.ltf_get_lderrno = nsLDAPGetLderrno;
    kLDAPThreadFuncs.ltf_set_lderrno = nsLDAPSetLderrno;

   /* Don't pass any extra parameter to the functions for 
      getting and setting libldap function call errors */
    kLDAPThreadFuncs.ltf_lderrno_arg = NULL;

    if (ldap_set_option(aLDAP, LDAP_OPT_THREAD_FN_PTRS, 
                        (void *)&kLDAPThreadFuncs) != 0) {
        return 0;
    }

    /* set extended thread function pointers */
    kLDAPExtraThreadFuncs.ltf_threadid_fn = nsLDAPGetThreadID;
    if ( ldap_set_option(aLDAP, LDAP_OPT_EXTRA_THREAD_FN_PTRS,
                         (void *)&kLDAPExtraThreadFuncs ) != 0 ) {
		return 0;
    }

    return 1;
}
