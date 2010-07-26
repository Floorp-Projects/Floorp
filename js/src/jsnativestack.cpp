/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 et sw=4 tw=80:
 */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include <stdlib.h>
#include "jstypes.h"
#include "jsnativestack.h"

#ifdef XP_WIN
# include <windows.h>

#elif defined(XP_OS2)
# define INCL_DOSPROCESS
# include <os2.h>

#elif defined(XP_MACOSX) || defined(DARWIN) || defined(XP_UNIX)
# include <pthread.h>

# if defined(__FreeBSD__)
#  include <pthread_np.h>
# endif

#else
# error "Unsupported platform"

#endif

namespace js {

#if defined(XP_WIN) && defined(WINCE)

inline bool
isPageWritable(void *page)
{
    MEMORY_BASIC_INFORMATION memoryInformation;
    jsuword result = VirtualQuery(page, &memoryInformation, sizeof(memoryInformation));

    /* return false on error, including ptr outside memory */
    if (result != sizeof(memoryInformation))
        return false;

    jsuword protect = memoryInformation.Protect & ~(PAGE_GUARD | PAGE_NOCACHE);
    return protect == PAGE_READWRITE ||
           protect == PAGE_WRITECOPY ||
           protect == PAGE_EXECUTE_READWRITE ||
           protect == PAGE_EXECUTE_WRITECOPY;
}

void *
GetNativeStackBase()
{
    /* find the address of this stack frame by taking the address of a local variable */
    bool isGrowingDownward = JS_STACK_GROWTH_DIRECTION < 0;
    void *thisFrame = (void *)(&isGrowingDownward);

    static jsuword pageSize = 0;
    if (!pageSize) {
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);
        pageSize = systemInfo.dwPageSize;
    }

    /* scan all of memory starting from this frame, and return the last writeable page found */
    register char *currentPage = (char *)((jsuword)thisFrame & ~(pageSize - 1));
    if (isGrowingDownward) {
        while (currentPage > 0) {
            /* check for underflow */
            if (currentPage >= (char *)pageSize)
                currentPage -= pageSize;
            else
                currentPage = 0;
            if (!isPageWritable(currentPage))
                return currentPage + pageSize;
        }
        return 0;
    } else {
        while (true) {
            /* guaranteed to complete because isPageWritable returns false at end of memory */
            currentPage += pageSize;
            if (!isPageWritable(currentPage))
                return currentPage;
        }
    }
}

#elif defined(XP_WIN)

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
#  if defined(PTHREAD_NP_H) || defined(_PTHREAD_NP_H_) || defined(NETBSD)
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
        pthread_attr_getstack(&sattr, &stackBase, &stackSize);
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
