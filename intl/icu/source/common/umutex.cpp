/*
******************************************************************************
*
*   Copyright (C) 1997-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File umutex.cpp
*
* Modification History:
*
*   Date        Name        Description
*   04/02/97    aliu        Creation.
*   04/07/99    srl         updated
*   05/13/99    stephen     Changed to umutex (from cmutex).
*   11/22/99    aliu        Make non-global mutex autoinitialize [j151]
******************************************************************************
*/

#include "unicode/utypes.h"
#include "uassert.h"
#include "ucln_cmn.h"

/*
 * ICU Mutex wrappers.  Wrap operating system mutexes, giving the rest of ICU a
 * platform independent set of mutex operations.  For internal ICU use only.
 */

#if U_PLATFORM_HAS_WIN32_API
    /* Prefer native Windows APIs even if POSIX is implemented (i.e., on Cygwin). */
#   undef POSIX
#elif U_PLATFORM_IMPLEMENTS_POSIX
#   define POSIX
#else
#   undef POSIX
#endif

#if defined(POSIX)
# include <pthread.h> /* must be first, so that we get the multithread versions of things. */
#endif /* POSIX */

#if U_PLATFORM_HAS_WIN32_API
# define WIN32_LEAN_AND_MEAN
# define VC_EXTRALEAN
# define NOUSER
# define NOSERVICE
# define NOIME
# define NOMCX
# include <windows.h>
#endif

#include "umutex.h"
#include "cmemory.h"

#if U_PLATFORM_HAS_WIN32_API
#define SYNC_COMPARE_AND_SWAP(dest, oldval, newval) \
            InterlockedCompareExchangePointer(dest, newval, oldval)

#elif defined(POSIX)
#if (U_HAVE_GCC_ATOMICS == 1)
#define SYNC_COMPARE_AND_SWAP(dest, oldval, newval) \
            __sync_val_compare_and_swap(dest, oldval, newval)
#else
#define SYNC_COMPARE_AND_SWAP(dest, oldval, newval) \
            mutexed_compare_and_swap(dest, newval, oldval)
#endif

#else   
// Unknown platform.  Note that user can still set mutex functions at run time.
#define SYNC_COMPARE_AND_SWAP(dest, oldval, newval) \
            mutexed_compare_and_swap(dest, newval, oldval)
#endif

static void *mutexed_compare_and_swap(void **dest, void *newval, void *oldval);

// The ICU global mutex. Used when ICU implementation code passes NULL for the mutex pointer.
static UMutex   globalMutex = U_MUTEX_INITIALIZER;

// Implementation mutex. Used for compare & swap when no intrinsic is available, and
// for safe initialization of user defined mutexes.
static UMutex   implMutex = U_MUTEX_INITIALIZER;      

// List of all user mutexes that have been initialized.
// Used to allow us to destroy them when cleaning up ICU.
// Normal platform mutexes are not kept track of in this way - they survive until the process is shut down.
// Normal platfrom mutexes don't allocate storage, so not cleaning them up won't trigger memory leak complaints.
//
// Note: putting this list in allocated memory would be awkward to arrange, because memory allocations
//       are used as a flag to indicate that ICU has been initialized, and setting other ICU 
//       override functions will no longer work.
//
static const int MUTEX_LIST_LIMIT = 100;
static UMutex *gMutexList[MUTEX_LIST_LIMIT];
static int gMutexListSize = 0;


/*
 *  User mutex implementation functions.  If non-null, call back to these rather than
 *  directly using the system (Posix or Windows) APIs.  See u_setMutexFunctions().
 *    (declarations are in uclean.h)
 */
static UMtxInitFn    *pMutexInitFn    = NULL;
static UMtxFn        *pMutexDestroyFn = NULL;
static UMtxFn        *pMutexLockFn    = NULL;
static UMtxFn        *pMutexUnlockFn  = NULL;
static const void    *gMutexContext   = NULL;


// Clean up (undo) the effects of u_setMutexFunctions().
//
static void usrMutexCleanup() {
    if (pMutexDestroyFn != NULL) {
        for (int i = 0; i < gMutexListSize; i++) {
            UMutex *m = gMutexList[i];
            U_ASSERT(m->fInitialized);
            (*pMutexDestroyFn)(gMutexContext, &m->fUserMutex);
            m->fInitialized = FALSE;
        }
        (*pMutexDestroyFn)(gMutexContext, &globalMutex.fUserMutex);
        (*pMutexDestroyFn)(gMutexContext, &implMutex.fUserMutex);
    }
    gMutexListSize  = 0;
    pMutexInitFn    = NULL;
    pMutexDestroyFn = NULL;
    pMutexLockFn    = NULL;
    pMutexUnlockFn  = NULL;
    gMutexContext   = NULL;
}


/*
 * User mutex lock.
 *
 * User mutexes need to be initialized before they can be used. We use the impl mutex
 * to synchronize the initialization check. This could be sped up on platforms that
 * support alternate ways to safely check the initialization flag.
 *
 */
static void usrMutexLock(UMutex *mutex) {
    UErrorCode status = U_ZERO_ERROR;
    if (!(mutex == &implMutex || mutex == &globalMutex)) {
        umtx_lock(&implMutex);
        if (!mutex->fInitialized) {
            (*pMutexInitFn)(gMutexContext, &mutex->fUserMutex, &status);
            U_ASSERT(U_SUCCESS(status));
            mutex->fInitialized = TRUE;
            U_ASSERT(gMutexListSize < MUTEX_LIST_LIMIT);
            if (gMutexListSize < MUTEX_LIST_LIMIT) {
                gMutexList[gMutexListSize] = mutex;
                ++gMutexListSize;
            }
        }
        umtx_unlock(&implMutex);
    }
    (*pMutexLockFn)(gMutexContext, &mutex->fUserMutex);
}
        


#if defined(POSIX)

//
// POSIX implementation of UMutex.
//
// Each UMutex has a corresponding pthread_mutex_t.
// All are statically initialized and ready for use.
// There is no runtime mutex initialization code needed.

U_CAPI void  U_EXPORT2
umtx_lock(UMutex *mutex) {
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    if (pMutexLockFn) {
        usrMutexLock(mutex);
    } else {
        #if U_DEBUG
            // #if to avoid unused variable warnings in non-debug builds.
            int sysErr = pthread_mutex_lock(&mutex->fMutex);
            U_ASSERT(sysErr == 0);
        #else
            pthread_mutex_lock(&mutex->fMutex);
        #endif
    }
}


U_CAPI void  U_EXPORT2
umtx_unlock(UMutex* mutex)
{
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    if (pMutexUnlockFn) {
        (*pMutexUnlockFn)(gMutexContext, &mutex->fUserMutex);
    } else {
        #if U_DEBUG
            // #if to avoid unused variable warnings in non-debug builds.
            int sysErr = pthread_mutex_unlock(&mutex->fMutex);
            U_ASSERT(sysErr == 0);
        #else
            pthread_mutex_unlock(&mutex->fMutex);
        #endif
    }
}

#elif U_PLATFORM_HAS_WIN32_API
//
// Windows implementation of UMutex.
//
// Each UMutex has a corresponding Windows CRITICAL_SECTION.
// CRITICAL_SECTIONS must be initialized before use. This is done
//   with a InitOnceExcuteOnce operation.
//
// InitOnceExecuteOnce was introduced with Windows Vista. For now ICU
// must support Windows XP, so we roll our own. ICU will switch to the
// native Windows InitOnceExecuteOnce when possible.

typedef UBool (*U_PINIT_ONCE_FN) (
  U_INIT_ONCE     *initOnce,
  void            *parameter,
  void            **context
);

UBool u_InitOnceExecuteOnce(
  U_INIT_ONCE     *initOnce,
  U_PINIT_ONCE_FN initFn,
  void            *parameter,
  void            **context) {
      for (;;) {
          long previousState = InterlockedCompareExchange( 
              &initOnce->fState,  //  Destination,
              1,                  //  Exchange Value
              0);                 //  Compare value
          if (previousState == 2) {
              // Initialization was already completed.
              if (context != NULL) {
                  *context = initOnce->fContext;
              }
              return TRUE;
          }
          if (previousState == 1) {
              // Initialization is in progress in some other thread.
              // Loop until it completes.
              Sleep(1);
              continue;
          }
           
          // Initialization needed. Execute the callback function to do it.
          U_ASSERT(previousState == 0);
          U_ASSERT(initOnce->fState == 1);
          UBool success = (*initFn)(initOnce, parameter, &initOnce->fContext);
          U_ASSERT(success); // ICU is not supporting the failure case.

          // Assign the state indicating that initialization has completed.
          // Using InterlockedCompareExchange to do it ensures that all
          // threads will have a consistent view of memory.
          previousState = InterlockedCompareExchange(&initOnce->fState, 2, 1);
          U_ASSERT(previousState == 1);
          // Next loop iteration will see the initialization and return.
      }
};

static UBool winMutexInit(U_INIT_ONCE *initOnce, void *param, void **context) {
    UMutex *mutex = static_cast<UMutex *>(param);
    U_ASSERT(sizeof(CRITICAL_SECTION) <= sizeof(mutex->fCS));
    InitializeCriticalSection((CRITICAL_SECTION *)mutex->fCS);
    return TRUE;
}

/*
 *   umtx_lock
 */
U_CAPI void  U_EXPORT2
umtx_lock(UMutex *mutex) {
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    if (pMutexLockFn) {
        usrMutexLock(mutex);
    } else {
        u_InitOnceExecuteOnce(&mutex->fInitOnce, winMutexInit, mutex, NULL);
        EnterCriticalSection((CRITICAL_SECTION *)mutex->fCS);
    }
}

U_CAPI void  U_EXPORT2
umtx_unlock(UMutex* mutex)
{
    if (mutex == NULL) {
        mutex = &globalMutex;
    }
    if (pMutexUnlockFn) {
        (*pMutexUnlockFn)(gMutexContext, &mutex->fUserMutex);
    } else {
        LeaveCriticalSection((CRITICAL_SECTION *)mutex->fCS);
    }
}

#endif  // Windows Implementation


U_CAPI void U_EXPORT2 
u_setMutexFunctions(const void *context, UMtxInitFn *i, UMtxFn *d, UMtxFn *l, UMtxFn *u,
                    UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return;
    }

    /* Can not set a mutex function to a NULL value  */
    if (i==NULL || d==NULL || l==NULL || u==NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    /* If ICU is not in an initial state, disallow this operation. */
    if (cmemory_inUse()) {
        *status = U_INVALID_STATE_ERROR;
        return;
    }

    // Clean up any previously set user mutex functions.
    // It's possible to call u_setMutexFunctions() more than once without without explicitly cleaning up,
    // and the last call should take. Kind of a corner case, but it worked once, there is a test for
    // it, so we keep it working. The global and impl mutexes will have been created by the
    // previous u_setMutexFunctions(), and now need to be destroyed.

    usrMutexCleanup();
    
    /* Swap in the mutex function pointers.  */
    pMutexInitFn    = i;
    pMutexDestroyFn = d;
    pMutexLockFn    = l;
    pMutexUnlockFn  = u;
    gMutexContext   = context;
    gMutexListSize  = 0;

    /* Initialize the global and impl mutexes. Safe to do at this point because
     * u_setMutexFunctions must be done in a single-threaded envioronment. Not thread safe.
     */
    (*pMutexInitFn)(gMutexContext, &globalMutex.fUserMutex, status);
    globalMutex.fInitialized = TRUE;
    (*pMutexInitFn)(gMutexContext, &implMutex.fUserMutex, status);
    implMutex.fInitialized = TRUE;
}



/*   synchronized compare and swap function, for use when OS or compiler built-in
 *   equivalents aren't available.
 */
static void *mutexed_compare_and_swap(void **dest, void *newval, void *oldval) {
    umtx_lock(&implMutex);
    void *temp = *dest;
    if (temp == oldval) {
        *dest = newval;
    }
    umtx_unlock(&implMutex);
    
    return temp;
}



/*-----------------------------------------------------------------
 *
 *  Atomic Increment and Decrement
 *     umtx_atomic_inc
 *     umtx_atomic_dec
 *
 *----------------------------------------------------------------*/

/* Pointers to user-supplied inc/dec functions.  Null if no funcs have been set.  */
static UMtxAtomicFn  *pIncFn = NULL;
static UMtxAtomicFn  *pDecFn = NULL;
static const void *gIncDecContext  = NULL;

#if defined (POSIX) && (U_HAVE_GCC_ATOMICS == 0)
static UMutex   gIncDecMutex = U_MUTEX_INITIALIZER;
#endif

U_CAPI int32_t U_EXPORT2
umtx_atomic_inc(int32_t *p)  {
    int32_t retVal;
    if (pIncFn) {
        retVal = (*pIncFn)(gIncDecContext, p);
    } else {
        #if U_PLATFORM_HAS_WIN32_API
            retVal = InterlockedIncrement((LONG*)p);
        #elif defined(USE_MAC_OS_ATOMIC_INCREMENT)
            retVal = OSAtomicIncrement32Barrier(p);
        #elif (U_HAVE_GCC_ATOMICS == 1)
            retVal = __sync_add_and_fetch(p, 1);
        #elif defined (POSIX)
            umtx_lock(&gIncDecMutex);
            retVal = ++(*p);
            umtx_unlock(&gIncDecMutex);
        #else
            /* Unknown Platform. */
            retVal = ++(*p);
        #endif
    }
    return retVal;
}

U_CAPI int32_t U_EXPORT2
umtx_atomic_dec(int32_t *p) {
    int32_t retVal;
    if (pDecFn) {
        retVal = (*pDecFn)(gIncDecContext, p);
    } else {
        #if U_PLATFORM_HAS_WIN32_API
            retVal = InterlockedDecrement((LONG*)p);
        #elif defined(USE_MAC_OS_ATOMIC_INCREMENT)
            retVal = OSAtomicDecrement32Barrier(p);
        #elif (U_HAVE_GCC_ATOMICS == 1)
            retVal = __sync_sub_and_fetch(p, 1);
        #elif defined (POSIX)
            umtx_lock(&gIncDecMutex);
            retVal = --(*p);
            umtx_unlock(&gIncDecMutex);
        #else
            /* Unknown Platform. */
            retVal = --(*p);
        #endif
    }
    return retVal;
}



U_CAPI void U_EXPORT2
u_setAtomicIncDecFunctions(const void *context, UMtxAtomicFn *ip, UMtxAtomicFn *dp,
                                UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return;
    }
    /* Can not set a mutex function to a NULL value  */
    if (ip==NULL || dp==NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    /* If ICU is not in an initial state, disallow this operation. */
    if (cmemory_inUse()) {
        *status = U_INVALID_STATE_ERROR;
        return;
    }

    pIncFn = ip;
    pDecFn = dp;
    gIncDecContext = context;

#if U_DEBUG
    {
        int32_t   testInt = 0;
        U_ASSERT(umtx_atomic_inc(&testInt) == 1);     /* Sanity Check.    Do the functions work at all? */
        U_ASSERT(testInt == 1);
        U_ASSERT(umtx_atomic_dec(&testInt) == 0);
        U_ASSERT(testInt == 0);
    }
#endif
}


/*
 *  Mutex Cleanup Function
 *      Reset the mutex function callback pointers.
 *      Called from the global ICU u_cleanup() function.
 */
U_CFUNC UBool umtx_cleanup(void) {
    /* Extra, do-nothing function call to suppress compiler warnings on platforms where
     *   mutexed_compare_and_swap is not otherwise used.  */
    void *pv = &globalMutex;
    mutexed_compare_and_swap(&pv, NULL, NULL);
    usrMutexCleanup();
           
    pIncFn          = NULL;
    pDecFn          = NULL;
    gIncDecContext  = NULL;

    return TRUE;
}
