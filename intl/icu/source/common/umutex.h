/*
**********************************************************************
*   Copyright (C) 1997-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
* File UMUTEX.H
*
* Modification History:
*
*   Date        Name        Description
*   04/02/97  aliu        Creation.
*   04/07/99  srl         rewrite - C interface, multiple mutices
*   05/13/99  stephen     Changed to umutex (from cmutex)
******************************************************************************
*/

#ifndef UMUTEX_H
#define UMUTEX_H

#include "unicode/utypes.h"
#include "unicode/uclean.h"
#include "putilimp.h"

/* For _ReadWriteBarrier(). */
#if defined(_MSC_VER) && _MSC_VER >= 1500
# include <intrin.h>
#endif

/* For CRITICAL_SECTION */
#if U_PLATFORM_HAS_WIN32_API
#if 0  
/* TODO(andy): Why doesn't windows.h compile in all files? It does in some.
 *             The intent was to include windows.h here, and have struct UMutex
 *             have an embedded CRITICAL_SECTION when building on Windows.
 *             The workaround is to put some char[] storage in UMutex instead,
 *             avoiding the need to include windows.h everwhere this header is included.
 */
# define WIN32_LEAN_AND_MEAN
# define VC_EXTRALEAN
# define NOUSER
# define NOSERVICE
# define NOIME
# define NOMCX
# include <windows.h>
#endif  /* 0 */
#define U_WINDOWS_CRIT_SEC_SIZE 64
#endif  /* win32 */

#if U_PLATFORM_IS_DARWIN_BASED
#include <libkern/OSAtomic.h>
#define USE_MAC_OS_ATOMIC_INCREMENT 1
#endif

/*
 * If we do not compile with dynamic_annotations.h then define
 * empty annotation macros.
 *  See http://code.google.com/p/data-race-test/wiki/DynamicAnnotations
 */
#ifndef ANNOTATE_HAPPENS_BEFORE
# define ANNOTATE_HAPPENS_BEFORE(obj)
# define ANNOTATE_HAPPENS_AFTER(obj)
# define ANNOTATE_UNPROTECTED_READ(x) (x)
#endif

#ifndef UMTX_FULL_BARRIER
# if U_HAVE_GCC_ATOMICS
#  define UMTX_FULL_BARRIER __sync_synchronize();
# elif defined(_MSC_VER) && _MSC_VER >= 1500
    /* From MSVC intrin.h. Use _ReadWriteBarrier() only on MSVC 9 and higher. */
#  define UMTX_FULL_BARRIER _ReadWriteBarrier();
# elif U_PLATFORM_IS_DARWIN_BASED
#  define UMTX_FULL_BARRIER OSMemoryBarrier();
# else
#  define UMTX_FULL_BARRIER \
    { \
        umtx_lock(NULL); \
        umtx_unlock(NULL); \
    }
# endif
#endif

#ifndef UMTX_ACQUIRE_BARRIER
# define UMTX_ACQUIRE_BARRIER UMTX_FULL_BARRIER
#endif

#ifndef UMTX_RELEASE_BARRIER
# define UMTX_RELEASE_BARRIER UMTX_FULL_BARRIER
#endif

/**
 * \def UMTX_CHECK
 * Encapsulates a safe check of an expression
 * for use with double-checked lazy inititialization.
 * Either memory barriers or mutexes are required, to prevent both the hardware
 * and the compiler from reordering operations across the check.
 * The expression must involve only a  _single_ variable, typically
 *    a possibly null pointer or a boolean that indicates whether some service
 *    is initialized or not.
 * The setting of the variable involved in the test must be the last step of
 *    the initialization process.
 *
 * @internal
 */
#define UMTX_CHECK(pMutex, expression, result) \
    { \
        (result)=(expression); \
        UMTX_ACQUIRE_BARRIER; \
    }
/*
 * TODO: Replace all uses of UMTX_CHECK and surrounding code
 * with SimpleSingleton or TriStateSingleton, and remove UMTX_CHECK.
 */

/*
 * Code within ICU that accesses shared static or global data should
 * instantiate a Mutex object while doing so.  The unnamed global mutex
 * is used throughout ICU, so keep locking short and sweet.
 *
 * For example:
 *
 * void Function(int arg1, int arg2)
 * {
 *   static Object* foo;     // Shared read-write object
 *   umtx_lock(NULL);        // Lock the ICU global mutex
 *   foo->Method();
 *   umtx_unlock(NULL);
 * }
 *
 * an alternative C++ mutex API is defined in the file common/mutex.h
 */

/*
 * UMutex - Mutexes for use by ICU implementation code.
 *          Must be declared as static or globals. They cannot appear as members
 *          of other objects.
 *          UMutex structs must be initialized.
 *          Example:
 *            static UMutex = U_MUTEX_INITIALIZER;
 *          The declaration of struct UMutex is platform dependent.
 */


#if U_PLATFORM_HAS_WIN32_API

/*  U_INIT_ONCE mimics the windows API INIT_ONCE, which exists on Windows Vista and newer.
 *  When ICU no longer needs to support older Windows platforms (XP) that do not have
 * a native INIT_ONCE, switch this implementation over to wrap the native Windows APIs.
 */
typedef struct U_INIT_ONCE {
    long               fState;
    void              *fContext;
} U_INIT_ONCE;
#define U_INIT_ONCE_STATIC_INIT {0, NULL}

typedef struct UMutex {
    U_INIT_ONCE       fInitOnce;
    UMTX              fUserMutex;
    UBool             fInitialized;  /* Applies to fUserMutex only. */
    /* CRITICAL_SECTION  fCS; */  /* See note above. Unresolved problems with including
                                   * Windows.h, which would allow using CRITICAL_SECTION
                                   * directly here. */
    char              fCS[U_WINDOWS_CRIT_SEC_SIZE];
} UMutex;

/* Initializer for a static UMUTEX. Deliberately contains no value for the
 *  CRITICAL_SECTION.
 */
#define U_MUTEX_INITIALIZER {U_INIT_ONCE_STATIC_INIT, NULL, FALSE}

#elif U_PLATFORM_IMPLEMENTS_POSIX
#include <pthread.h>

struct UMutex {
    pthread_mutex_t  fMutex;
    UMTX             fUserMutex;
    UBool            fInitialized;
};
#define U_MUTEX_INITIALIZER  {PTHREAD_MUTEX_INITIALIZER, NULL, FALSE}

#else
/* Unknow platform type. */
struct UMutex {
    void *fMutex;
};
#define U_MUTEX_INITIALIZER {NULL}
#error Unknown Platform.

#endif

#if (U_PLATFORM != U_PF_CYGWIN && U_PLATFORM != U_PF_MINGW) || defined(CYGWINMSVC)
typedef struct UMutex UMutex;
#endif
    
/* Lock a mutex.
 * @param mutex The given mutex to be locked.  Pass NULL to specify
 *              the global ICU mutex.  Recursive locks are an error
 *              and may cause a deadlock on some platforms.
 */
U_CAPI void U_EXPORT2 umtx_lock(UMutex* mutex); 

/* Unlock a mutex.
 * @param mutex The given mutex to be unlocked.  Pass NULL to specify
 *              the global ICU mutex.
 */
U_CAPI void U_EXPORT2 umtx_unlock (UMutex* mutex);

/*
 * Atomic Increment and Decrement of an int32_t value.
 *
 * Return Values:
 *   If the result of the operation is zero, the return zero.
 *   If the result of the operation is not zero, the sign of returned value
 *      is the same as the sign of the result, but the returned value itself may
 *      be different from the result of the operation.
 */
U_CAPI int32_t U_EXPORT2 umtx_atomic_inc(int32_t *);
U_CAPI int32_t U_EXPORT2 umtx_atomic_dec(int32_t *);

#endif /*_CMUTEX*/
/*eof*/
