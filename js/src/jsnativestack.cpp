/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 et sw=4 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "jstypes.h"
#include "jsnativestack.h"

#ifdef XP_WIN
# include "jswin.h"

#elif defined(XP_OS2)
# define INCL_DOSPROCESS
# include <os2.h>

#elif defined(XP_MACOSX) || defined(DARWIN) || defined(XP_UNIX)
# include <pthread.h>

# if defined(__FreeBSD__) || defined(__OpenBSD__)
#  include <pthread_np.h>
# endif

#else
# error "Unsupported platform"

#endif

namespace js {

#if defined(XP_WIN)

void *
GetNativeStackBaseImpl()
{
# if defined(_M_IX86) && defined(_MSC_VER)
    /*
     * offset 0x18 from the FS segment register gives a pointer to
     * the thread information block for the current thread
     */
    NT_TIB* pTib;
    __asm {
        MOV EAX, FS:[18h]
        MOV pTib, EAX
    }
    return static_cast<void*>(pTib->StackBase);

# elif defined(_M_X64)
    PNT_TIB64 pTib = reinterpret_cast<PNT_TIB64>(NtCurrentTeb());
    return reinterpret_cast<void*>(pTib->StackBase);

# elif defined(_WIN32) && defined(__GNUC__)
    NT_TIB* pTib;
    asm ("movl %%fs:0x18, %0\n" : "=r" (pTib));
    return static_cast<void*>(pTib->StackBase);

# endif
}

#elif defined(SOLARIS)

#include <ucontext.h>

JS_STATIC_ASSERT(JS_STACK_GROWTH_DIRECTION < 0);

void *
GetNativeStackBaseImpl()
{
    stack_t st;
    stack_getbounds(&st);
    return static_cast<char*>(st.ss_sp) + st.ss_size;
}

#elif defined(AIX)

#include <ucontext.h>

JS_STATIC_ASSERT(JS_STACK_GROWTH_DIRECTION < 0);

void *
GetNativeStackBaseImpl()
{
    ucontext_t context;
    getcontext(&context);
    return static_cast<char*>(context.uc_stack.ss_sp) +
        context.uc_stack.ss_size;
}

#elif defined(XP_OS2)

void *
GetNativeStackBaseImpl()
{
    PTIB  ptib;
    PPIB  ppib;

    DosGetInfoBlocks(&ptib, &ppib);
    return ptib->tib_pstacklimit;
}

#else /* XP_UNIX */

void *
GetNativeStackBaseImpl()
{
    pthread_t thread = pthread_self();
# if defined(XP_MACOSX) || defined(DARWIN)
    return pthread_get_stackaddr_np(thread);

# else
    pthread_attr_t sattr;
    pthread_attr_init(&sattr);
#  if defined(__OpenBSD__)
    stack_t ss;
#  elif defined(PTHREAD_NP_H) || defined(_PTHREAD_NP_H_) || defined(NETBSD)
    /* e.g. on FreeBSD 4.8 or newer, neundorf@kde.org */
    pthread_attr_get_np(thread, &sattr);
#  else
    /*
     * FIXME: this function is non-portable;
     * other POSIX systems may have different np alternatives
     */
    pthread_getattr_np(thread, &sattr);
#  endif

    void *stackBase = 0;
    size_t stackSize = 0;
#  ifdef DEBUG
    int rc =
#  endif
# if defined(__OpenBSD__)
        pthread_stackseg_np(pthread_self(), &ss);
    stackBase = (void*)((size_t) ss.ss_sp - ss.ss_size);
    stackSize = ss.ss_size;
# else
        pthread_attr_getstack(&sattr, &stackBase, &stackSize);
# endif
    JS_ASSERT(!rc);
    JS_ASSERT(stackBase);
    pthread_attr_destroy(&sattr);

#  if JS_STACK_GROWTH_DIRECTION > 0
    return stackBase;
#  else
    return static_cast<char*>(stackBase) + stackSize;
#  endif
# endif
}

#endif /* !XP_WIN */

} /* namespace js */
