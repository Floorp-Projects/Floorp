/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005,2007 Red Hat, Inc.
 * Copyright © 2007 Mathias Hasselmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Mathias Hasselmann <mathias.hasselmann@gmx.de>
 *	Behdad Esfahbod <behdad@behdad.org>
 */

#ifndef CAIRO_MUTEX_TYPE_PRIVATE_H
#define CAIRO_MUTEX_TYPE_PRIVATE_H

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <cairo-features.h>

CAIRO_BEGIN_DECLS


/* A fully qualified no-operation statement */
#define CAIRO_MUTEX_NOOP	do {/*no-op*/} while (0)
/* And one that evaluates it's argument once */
#define CAIRO_MUTEX_NOOP1(expr)        do { if (expr) ; } while (0)


/* Cairo mutex implementation:
 *
 * Any new mutex implementation needs to do the following:
 *
 * - Condition on the right header or feature.  Headers are
 *   preferred as eg. you still can use win32 mutex implementation
 *   on a win32 system even if you do not compile the win32
 *   surface/backend.
 *
 * - typedef #cairo_mutex_t to the proper mutex type on your target
 *   system.  Note that you may or may not need to use a pointer,
 *   depending on what kinds of initialization your mutex
 *   implementation supports.  No trailing semicolon needed.
 *   You should be able to compile the following snippet (don't try
 *   running it):
 *
 *	cairo_mutex_t _cairo_some_mutex;
 *
 * - #define CAIRO_MUTEX_LOCK(mutex) and CAIRO_MUTEX_UNLOCK(mutex) to
 *   proper statement to lock/unlock the mutex object passed in.
 *   You can (and should) assume that the mutex is already
 *   initialized, and is-not-already-locked/is-locked,
 *   respectively.  Use the "do { ... } while (0)" idiom if necessary.
 *   No trailing semicolons are needed (in any macro you define here).
 *   You should be able to compile the following snippet:
 *
 *	cairo_mutex_t _cairo_some_mutex;
 *
 *      if (1)
 *          %CAIRO_MUTEX_LOCK (_cairo_some_mutex);
 *      else
 *          %CAIRO_MUTEX_UNLOCK (_cairo_some_mutex);
 *
 * - #define %CAIRO_MUTEX_NIL_INITIALIZER to something that can
 *   initialize the #cairo_mutex_t type you defined.  Most of the
 *   time one of 0, %NULL, or {} works.  At this point
 *   you should be able to compile the following snippet:
 *
 *	cairo_mutex_t _cairo_some_mutex = CAIRO_MUTEX_NIL_INITIALIZER;
 *
 *      if (1)
 *          %CAIRO_MUTEX_LOCK (_cairo_some_mutex);
 *      else
 *          %CAIRO_MUTEX_UNLOCK (_cairo_some_mutex);
 *
 * - If the above code is not enough to initialize a mutex on
 *   your platform, #define CAIRO_MUTEX_INIT(mutex) to statement
 *   to initialize the mutex (allocate resources, etc).  Such that
 *   you should be able to compile AND RUN the following snippet:
 *
 *	cairo_mutex_t _cairo_some_mutex = CAIRO_MUTEX_NIL_INITIALIZER;
 *
 *      %CAIRO_MUTEX_INIT (_cairo_some_mutex);
 *
 *      if (1)
 *          %CAIRO_MUTEX_LOCK (_cairo_some_mutex);
 *      else
 *          %CAIRO_MUTEX_UNLOCK (_cairo_some_mutex);
 *
 * - If you define CAIRO_MUTEX_INIT(mutex), cairo will use it to
 *   initialize all static mutex'es.  If for any reason that should
 *   not happen (eg. %CAIRO_MUTEX_INIT is just a faster way than
 *   what cairo does using %CAIRO_MUTEX_NIL_INITIALIZER), then
 *   #define CAIRO_MUTEX_INITIALIZE() CAIRO_MUTEX_NOOP
 *
 * - If your system supports freeing a mutex object (deallocating
 *   resources, etc), then #define CAIRO_MUTEX_FINI(mutex) to do
 *   that.
 *
 * - If you define CAIRO_MUTEX_FINI(mutex), cairo will use it to
 *   define a finalizer function to finalize all static mutex'es.
 *   However, it's up to you to call CAIRO_MUTEX_FINALIZE() at
 *   proper places, eg. when the system is unloading the cairo library.
 *   So, if for any reason finalizing static mutex'es is not needed
 *   (eg. you never call %CAIRO_MUTEX_FINALIZE), then
 *   #define CAIRO_MUTEX_FINALIZE() CAIRO_MUTEX_NOOP
 *
 * - That is all.  If for any reason you think the above API is
 *   not enough to implement #cairo_mutex_t on your system, please
 *   stop and write to the cairo mailing list about it.  DO NOT
 *   poke around cairo-mutex-private.h for possible solutions.
 */

#if CAIRO_NO_MUTEX

/* No mutexes */

  typedef int cairo_mutex_t;

# define CAIRO_MUTEX_INITIALIZE() CAIRO_MUTEX_NOOP
# define CAIRO_MUTEX_LOCK(mutex) CAIRO_MUTEX_NOOP1(mutex)
# define CAIRO_MUTEX_UNLOCK(mutex) CAIRO_MUTEX_NOOP1(mutex)
# define CAIRO_MUTEX_NIL_INITIALIZER 0

#elif HAVE_PTHREAD_H /*******************************************************/

# include <pthread.h>

  typedef pthread_mutex_t cairo_mutex_t;

# define CAIRO_MUTEX_LOCK(mutex) pthread_mutex_lock (&(mutex))
# define CAIRO_MUTEX_UNLOCK(mutex) pthread_mutex_unlock (&(mutex))
# define CAIRO_MUTEX_FINI(mutex) pthread_mutex_destroy (&(mutex))
# define CAIRO_MUTEX_FINALIZE() CAIRO_MUTEX_NOOP
# define CAIRO_MUTEX_NIL_INITIALIZER PTHREAD_MUTEX_INITIALIZER

#elif HAVE_WINDOWS_H /*******************************************************/

# include <windows.h>

  typedef CRITICAL_SECTION cairo_mutex_t;

# define CAIRO_MUTEX_LOCK(mutex) EnterCriticalSection (&(mutex))
# define CAIRO_MUTEX_UNLOCK(mutex) LeaveCriticalSection (&(mutex))
# define CAIRO_MUTEX_INIT(mutex) InitializeCriticalSection (&(mutex))
# define CAIRO_MUTEX_FINI(mutex) DeleteCriticalSection (&(mutex))
# define CAIRO_MUTEX_NIL_INITIALIZER { NULL, 0, 0, NULL, NULL, 0 }

#elif defined __OS2__ /******************************************************/

# define INCL_BASE
# define INCL_PM
# include <os2.h>

  typedef HMTX cairo_mutex_t;

# define CAIRO_MUTEX_LOCK(mutex) DosRequestMutexSem(mutex, SEM_INDEFINITE_WAIT)
# define CAIRO_MUTEX_UNLOCK(mutex) DosReleaseMutexSem(mutex)
# define CAIRO_MUTEX_INIT(mutex) DosCreateMutexSem (NULL, &(mutex), 0L, FALSE)
# define CAIRO_MUTEX_FINI(mutex) DosCloseMutexSem (mutex)
# define CAIRO_MUTEX_NIL_INITIALIZER 0

#elif CAIRO_HAS_BEOS_SURFACE /***********************************************/

  typedef BLocker* cairo_mutex_t;

# define CAIRO_MUTEX_LOCK(mutex) (mutex)->Lock()
# define CAIRO_MUTEX_UNLOCK(mutex) (mutex)->Unlock()
# define CAIRO_MUTEX_INIT(mutex) (mutex) = new BLocker()
# define CAIRO_MUTEX_FINI(mutex) delete (mutex)
# define CAIRO_MUTEX_NIL_INITIALIZER NULL

#else /**********************************************************************/

# error "XXX: No mutex implementation found.  Cairo will not work with multiple threads.  Define CAIRO_NO_MUTEX to 1 to acknowledge and accept this limitation and compile cairo without thread-safety support."


#endif

CAIRO_END_DECLS

#endif
