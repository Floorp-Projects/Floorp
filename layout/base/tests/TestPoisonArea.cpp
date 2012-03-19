/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is frame poisoning tests.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Zachary Weinberg <zweinberg@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

/* Code in this file needs to be kept in sync with code in nsPresArena.cpp.
 *
 * We want to use a fixed address for frame poisoning so that it is readily
 * identifiable in crash dumps.  Whether such an address is available
 * without any special setup depends on the system configuration.
 *
 * All current 64-bit CPUs (with the possible exception of PowerPC64)
 * reserve the vast majority of the virtual address space for future
 * hardware extensions; valid addresses must be below some break point
 * between 2**48 and 2**54, depending on exactly which chip you have.  Some
 * chips (notably amd64) also allow the use of the *highest* 2**48 -- 2**54
 * addresses.  Thus, if user space pointers are 64 bits wide, we can just
 * use an address outside this range, and no more is required.  To
 * accommodate the chips that allow very high addresses to be valid, the
 * value chosen is close to 2**63 (that is, in the middle of the space).
 *
 * In most cases, a purely 32-bit operating system must reserve some
 * fraction of the address space for its own use.  Contemporary 32-bit OSes
 * tend to take the high gigabyte or so (0xC000_0000 on up).  If we can
 * prove that high addresses are reserved to the kernel, we can use an
 * address in that region.  Unfortunately, not all 32-bit OSes do this;
 * OSX 10.4 might not, and it is unclear what mobile OSes are like
 * (some 32-bit CPUs make it very easy for the kernel to exist in its own
 * private address space).
 *
 * Furthermore, when a 32-bit user space process is running on a 64-bit
 * kernel, the operating system has no need to reserve any of the space that
 * the process can see, and generally does not do so.  This is the scenario
 * of greatest concern, since it covers all contemporary OSX iterations
 * (10.5+) as well as Windows Vista and 7 on newer amd64 hardware.  Linux on
 * amd64 is generally run as a pure 64-bit environment, but its 32-bit
 * compatibility mode also has this property.
 *
 * Thus, when user space pointers are 32 bits wide, we need to validate
 * our chosen address, and possibly *make* it a good poison address by
 * allocating a page around it and marking it inaccessible.  The algorithm
 * for this is:
 *
 *  1. Attempt to make the page surrounding the poison address a reserved,
 *     inaccessible memory region using OS primitives.  On Windows, this is
 *     done with VirtualAlloc(MEM_RESERVE); on Unix, mmap(PROT_NONE).
 *
 *  2. If mmap/VirtualAlloc failed, there are two possible reasons: either
 *     the region is reserved to the kernel and no further action is
 *     required, or there is already usable memory in this area and we have
 *     to pick a different address.  The tricky part is knowing which case
 *     we have, without attempting to access the region.  On Windows, we
 *     rely on GetSystemInfo()'s reported upper and lower bounds of the
 *     application memory area.  On Unix, there is nothing devoted to the
 *     purpose, but seeing if madvise() fails is close enough (it *might*
 *     disrupt someone else's use of the memory region, but not by as much
 *     as anything else available).
 *
 * Be aware of these gotchas:
 *
 * 1. We cannot use mmap() with MAP_FIXED.  MAP_FIXED is defined to
 *    _replace_ any existing mapping in the region, if necessary to satisfy
 *    the request.  Obviously, as we are blindly attempting to acquire a
 *    page at a constant address, we must not do this, lest we overwrite
 *    someone else's allocation.
 *
 * 2. For the same reason, we cannot blindly use mprotect() if mmap() fails.
 *
 * 3. madvise() may fail when applied to a 'magic' memory region provided as
 *    a kernel/user interface.  Fortunately, the only such case I know about
 *    is the "vsyscall" area (not to be confused with the "vdso" area) for
 *    *64*-bit processes on Linux - and we don't even run this code for
 *    64-bit processes.
 *
 * 4. VirtualQuery() does not produce any useful information if
 *    applied to kernel memory - in fact, it doesn't write its output
 *    at all.  Thus, it is not used here.
 */

// MAP_ANON(YMOUS) is not in any standard, and the C99 PRI* macros are
// not in C++98.  Add defines as necessary.
#define __STDC_FORMAT_MACROS
#define _GNU_SOURCE 1
#define _DARWIN_C_SOURCE 1

#include <stddef.h>

#ifndef _WIN32
#include <inttypes.h>
#else
#define PRIxPTR "Ix"
typedef unsigned int uint32_t;
// MSVC defines uintptr_t in <crtdefs.h> which is brought in implicitly
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__OS2__)
#include <sys/types.h>
#include <unistd.h>
#include <setjmp.h>
#define INCL_DOS
#include <os2.h>
#else
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <sys/mman.h>
#ifndef MAP_ANON
#ifdef MAP_ANONYMOUS
#define MAP_ANON MAP_ANONYMOUS
#else
#error "Don't know how to get anonymous memory"
#endif
#endif
#endif

#define SIZxPTR ((int)(sizeof(uintptr_t)*2))

/* This program assumes that a whole number of return instructions fit into
 * 32 bits, and that 32-bit alignment is sufficient for a branch destination.
 * For architectures where this is not true, fiddling with RETURN_INSTR_TYPE
 * can be enough.
 */

#if defined __i386__ || defined __x86_64__ ||   \
  defined __i386 || defined __x86_64 ||         \
  defined _M_IX86 || defined _M_AMD64
#define RETURN_INSTR 0xC3C3C3C3  /* ret; ret; ret; ret */

#elif defined __arm__ || defined _M_ARM
#define RETURN_INSTR 0xE12FFF1E /* bx lr */

// PPC has its own style of CPU-id #defines.  There is no Windows for
// PPC as far as I know, so no _M_ variant.
#elif defined _ARCH_PPC || defined _ARCH_PWR || defined _ARCH_PWR2
#define RETURN_INSTR 0x4E800020 /* blr */

#elif defined __sparc || defined __sparcv9
#define RETURN_INSTR 0x81c3e008 /* retl */

#elif defined __alpha
#define RETURN_INSTR 0x6bfa8001 /* ret */

#elif defined __hppa
#define RETURN_INSTR 0xe840c002 /* bv,n r0(rp) */

#elif defined __mips
#define RETURN_INSTR 0x03e00008 /* jr ra */

#ifdef __MIPSEL
/* On mipsel, jr ra needs to be followed by a nop.
   0x03e00008 as a 64 bits integer just does that */
#define RETURN_INSTR_TYPE uint64_t
#endif

#elif defined __s390__
#define RETURN_INSTR 0x07fe0000 /* br %r14 */

#elif defined __ia64
struct ia64_instr { uint32_t i[4]; };
static const ia64_instr _return_instr =
  {{ 0x00000011, 0x00000001, 0x80000200, 0x00840008 }}; /* br.ret.sptk.many b0 */

#define RETURN_INSTR _return_instr
#define RETURN_INSTR_TYPE ia64_instr

#else
#error "Need return instruction for this architecture"
#endif

#ifndef RETURN_INSTR_TYPE
#define RETURN_INSTR_TYPE uint32_t
#endif

// Miscellaneous Windows/Unix portability gumph

#ifdef _WIN32
// Uses of this function deliberately leak the string.
static LPSTR
StrW32Error(DWORD errcode)
{
  LPSTR errmsg;
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                 FORMAT_MESSAGE_FROM_SYSTEM |
                 FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 (LPSTR) &errmsg, 0, NULL);

  // FormatMessage puts an unwanted newline at the end of the string
  size_t n = strlen(errmsg)-1;
  while (errmsg[n] == '\r' || errmsg[n] == '\n') n--;
  errmsg[n+1] = '\0';
  return errmsg;
}
#define LastErrMsg() (StrW32Error(GetLastError()))

// Because we use VirtualAlloc in MEM_RESERVE mode, the "page size" we want
// is the allocation granularity.
static SYSTEM_INFO _sinfo;
#undef PAGESIZE
#define PAGESIZE (_sinfo.dwAllocationGranularity)


static void *
ReserveRegion(uintptr_t request, bool accessible)
{
  return VirtualAlloc((void *)request, PAGESIZE,
                      accessible ? MEM_RESERVE|MEM_COMMIT : MEM_RESERVE,
                      accessible ? PAGE_EXECUTE_READWRITE : PAGE_NOACCESS);
}

static void
ReleaseRegion(void *page)
{
  VirtualFree(page, PAGESIZE, MEM_RELEASE);
}

static bool
ProbeRegion(uintptr_t page)
{
  if (page >= (uintptr_t)_sinfo.lpMaximumApplicationAddress &&
      page + PAGESIZE >= (uintptr_t)_sinfo.lpMaximumApplicationAddress) {
    return true;
  } else {
    return false;
  }
}

static bool
MakeRegionExecutable(void *)
{
  return false;
}

#undef MAP_FAILED
#define MAP_FAILED 0

#elif defined(__OS2__)

// page size is always 4k
#undef PAGESIZE
#define PAGESIZE 0x1000
static unsigned long rc = 0;

char * LastErrMsg()
{
  char * errmsg = (char *)malloc(16);
  sprintf(errmsg, "rc= %ld", rc);
  rc = 0;
  return errmsg;
}

static void *
ReserveRegion(uintptr_t request, bool accessible)
{
  // OS/2 doesn't support allocation at an arbitrary address,
  // so return an address that is known to be invalid.
  if (request) {
    return (void*)0xFFFD0000;
  }
  void * mem = 0;
  rc = DosAllocMem(&mem, PAGESIZE,
                   (accessible ? PAG_COMMIT : 0) | PAG_READ | PAG_WRITE);
  return rc ? 0 : mem;
}

static void
ReleaseRegion(void *page)
{
  return;
}

static bool
ProbeRegion(uintptr_t page)
{
  // There's no reliable way to probe an address in the system
  // arena other than by touching it and seeing if a trap occurs.
  return false;
}

static bool
MakeRegionExecutable(void *page)
{
  rc = DosSetMem(page, PAGESIZE, PAG_READ | PAG_WRITE | PAG_EXECUTE);
  return rc ? true : false;
}

typedef struct _XCPT {
  EXCEPTIONREGISTRATIONRECORD regrec;
  jmp_buf                     jmpbuf;
} XCPT;

static unsigned long _System
ExceptionHandler(PEXCEPTIONREPORTRECORD pReport,
                 PEXCEPTIONREGISTRATIONRECORD pRegRec,
                 PCONTEXTRECORD pContext, PVOID pVoid)
{
  if (pReport->fHandlerFlags == 0) {
    longjmp(((XCPT*)pRegRec)->jmpbuf, pReport->ExceptionNum);
  }
  return XCPT_CONTINUE_SEARCH;
}

#undef MAP_FAILED
#define MAP_FAILED 0

#else // Unix

#define LastErrMsg() (strerror(errno))

static unsigned long _pagesize;
#define PAGESIZE _pagesize

static void *
ReserveRegion(uintptr_t request, bool accessible)
{
  return mmap((caddr_t)request, PAGESIZE,
              accessible ? PROT_READ|PROT_WRITE : PROT_NONE,
              MAP_PRIVATE|MAP_ANON, -1, 0);
}

static void
ReleaseRegion(void *page)
{
  munmap((caddr_t)page, PAGESIZE);
}

static bool
ProbeRegion(uintptr_t page)
{
  if (madvise((caddr_t)page, PAGESIZE, MADV_NORMAL)) {
    return true;
  } else {
    return false;
  }
}

static int
MakeRegionExecutable(void *page)
{
  return mprotect((caddr_t)page, PAGESIZE, PROT_READ|PROT_WRITE|PROT_EXEC);
}

#endif

static uintptr_t
ReservePoisonArea()
{
  if (sizeof(uintptr_t) == 8) {
    // Use the hardware-inaccessible region.
    // We have to avoid 64-bit constants and shifts by 32 bits, since this
    // code is compiled in 32-bit mode, although it is never executed there.
    uintptr_t result = (((uintptr_t(0x7FFFFFFFu) << 31) << 1 |
                         uintptr_t(0xF0DEAFFFu)) &
                        ~uintptr_t(PAGESIZE-1));
    printf("INFO | poison area assumed at 0x%.*" PRIxPTR "\n", SIZxPTR, result);
    return result;
  } else {
    // First see if we can allocate the preferred poison address from the OS.
    uintptr_t candidate = (0xF0DEAFFF & ~(PAGESIZE-1));
    void *result = ReserveRegion(candidate, false);
    if (result == (void *)candidate) {
      // success - inaccessible page allocated
      printf("INFO | poison area allocated at 0x%.*" PRIxPTR
             " (preferred addr)\n", SIZxPTR, (uintptr_t)result);
      return candidate;
    }

    // That didn't work, so see if the preferred address is within a range
    // of permanently inacessible memory.
    if (ProbeRegion(candidate)) {
      // success - selected page cannot be usable memory
      if (result != MAP_FAILED)
        ReleaseRegion(result);
      printf("INFO | poison area assumed at 0x%.*" PRIxPTR
             " (preferred addr)\n", SIZxPTR, candidate);
      return candidate;
    }

    // The preferred address is already in use.  Did the OS give us a
    // consolation prize?
    if (result != MAP_FAILED) {
      printf("INFO | poison area allocated at 0x%.*" PRIxPTR
             " (consolation prize)\n", SIZxPTR, (uintptr_t)result);
      return (uintptr_t)result;
    }

    // It didn't, so try to allocate again, without any constraint on
    // the address.
    result = ReserveRegion(0, false);
    if (result != MAP_FAILED) {
      printf("INFO | poison area allocated at 0x%.*" PRIxPTR
             " (fallback)\n", SIZxPTR, (uintptr_t)result);
      return (uintptr_t)result;
    }

    printf("ERROR | no usable poison area found\n");
    return 0;
  }
}

/* The "positive control" area confirms that we can allocate a page with the
 * proper characteristics.
 */
static uintptr_t
ReservePositiveControl()
{

  void *result = ReserveRegion(0, false);
  if (result == MAP_FAILED) {
    printf("ERROR | allocating positive control | %s\n", LastErrMsg());
    return 0;
  }
  printf("INFO | positive control allocated at 0x%.*" PRIxPTR "\n",
         SIZxPTR, (uintptr_t)result);
  return (uintptr_t)result;
}

/* The "negative control" area confirms that our probe logic does detect a
 * page that is readable, writable, or executable.
 */
static uintptr_t
ReserveNegativeControl()
{
  void *result = ReserveRegion(0, true);
  if (result == MAP_FAILED) {
    printf("ERROR | allocating negative control | %s\n", LastErrMsg());
    return 0;
  }

  // Fill the page with return instructions.
  RETURN_INSTR_TYPE *p = (RETURN_INSTR_TYPE *)result;
  RETURN_INSTR_TYPE *limit = (RETURN_INSTR_TYPE *)(((char *)result) + PAGESIZE);
  while (p < limit)
    *p++ = RETURN_INSTR;

  // Now mark it executable as well as readable and writable.
  // (mmap(PROT_EXEC) may fail when applied to anonymous memory.)

  if (MakeRegionExecutable(result)) {
    printf("ERROR | making negative control executable | %s\n", LastErrMsg());
    return 0;
  }

  printf("INFO | negative control allocated at 0x%.*" PRIxPTR "\n",
         SIZxPTR, (uintptr_t)result);
  return (uintptr_t)result;
}

static void
JumpTo(uintptr_t opaddr)
{
#ifdef __ia64
  struct func_call {
    uintptr_t func;
    uintptr_t gp;
  } call = { opaddr, };
  ((void (*)())&call)();
#else
  ((void (*)())opaddr)();
#endif
}

#ifdef _WIN32
static BOOL
IsBadExecPtr(uintptr_t ptr)
{
  BOOL ret = false;

#ifdef _MSC_VER
  __try {
    JumpTo(ptr);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    ret = true;
  }
#else
  printf("INFO | exec test not supported on MinGW build\n");
  // We do our best
  ret = IsBadReadPtr((const void*)ptr, 1);
#endif
  return ret;
}
#endif

/* Test each page.  */
static bool
TestPage(const char *pagelabel, uintptr_t pageaddr, int should_succeed)
{
  const char *oplabel;
  uintptr_t opaddr;

  bool failed = false;
  for (unsigned int test = 0; test < 3; test++) {
    switch (test) {
      // The execute test must be done before the write test, because the
      // write test will clobber memory at the target address.
    case 0: oplabel = "reading"; opaddr = pageaddr + PAGESIZE/2 - 1; break;
    case 1: oplabel = "executing"; opaddr = pageaddr + PAGESIZE/2; break;
    case 2: oplabel = "writing"; opaddr = pageaddr + PAGESIZE/2 - 1; break;
    default: abort();
    }

#ifdef _WIN32
    BOOL badptr;

    switch (test) {
    case 0: badptr = IsBadReadPtr((const void*)opaddr, 1); break;
    case 1: badptr = IsBadExecPtr(opaddr); break;
    case 2: badptr = IsBadWritePtr((void*)opaddr, 1); break;
    default: abort();
    }

    if (badptr) {
      if (should_succeed) {
        printf("TEST-UNEXPECTED-FAIL | %s %s\n", oplabel, pagelabel);
        failed = true;
      } else {
        printf("TEST-PASS | %s %s\n", oplabel, pagelabel);
      }
    } else {
      // if control reaches this point the probe succeeded
      if (should_succeed) {
        printf("TEST-PASS | %s %s\n", oplabel, pagelabel);
      } else {
        printf("TEST-UNEXPECTED-FAIL | %s %s\n", oplabel, pagelabel);
        failed = true;
      }
    }
#elif defined(__OS2__)
    XCPT xcpt;
    volatile int code = setjmp(xcpt.jmpbuf);

    if (!code) {
      xcpt.regrec.prev_structure = 0;
      xcpt.regrec.ExceptionHandler = ExceptionHandler;
      DosSetExceptionHandler(&xcpt.regrec);
      unsigned char scratch;
      switch (test) {
        case 0: scratch = *(volatile unsigned char *)opaddr; break;
        case 1: ((void (*)())opaddr)(); break;
        case 2: *(volatile unsigned char *)opaddr = 0; break;
        default: abort();
      }
    }

    if (code) {
      if (should_succeed) {
        printf("TEST-UNEXPECTED-FAIL | %s %s | exception code %x\n",
               oplabel, pagelabel, code);
        failed = true;
      } else {
        printf("TEST-PASS | %s %s | exception code %x\n",
               oplabel, pagelabel, code);
      }
    } else {
      if (should_succeed) {
        printf("TEST-PASS | %s %s\n", oplabel, pagelabel);
      } else {
        printf("TEST-UNEXPECTED-FAIL | %s %s\n", oplabel, pagelabel);
        failed = true;
      }
      DosUnsetExceptionHandler(&xcpt.regrec);
    }
#else
    pid_t pid = fork();
    if (pid == -1) {
      printf("ERROR | %s %s | fork=%s\n", oplabel, pagelabel,
             LastErrMsg());
      exit(2);
    } else if (pid == 0) {
      volatile unsigned char scratch;
      switch (test) {
      case 0: scratch = *(volatile unsigned char *)opaddr; break;
      case 1: JumpTo(opaddr); break;
      case 2: *(volatile unsigned char *)opaddr = 0; break;
      default: abort();
      }
      (void)scratch;
      _exit(0);
    } else {
      int status;
      if (waitpid(pid, &status, 0) != pid) {
        printf("ERROR | %s %s | wait=%s\n", oplabel, pagelabel,
               LastErrMsg());
        exit(2);
      }

      if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        if (should_succeed) {
          printf("TEST-PASS | %s %s\n", oplabel, pagelabel);
        } else {
          printf("TEST-UNEXPECTED-FAIL | %s %s | unexpected successful exit\n",
                 oplabel, pagelabel);
          failed = true;
        }
      } else if (WIFEXITED(status)) {
        printf("ERROR | %s %s | unexpected exit code %d\n",
               oplabel, pagelabel, WEXITSTATUS(status));
        exit(2);
      } else if (WIFSIGNALED(status)) {
        if (should_succeed) {
          printf("TEST-UNEXPECTED-FAIL | %s %s | unexpected signal %d\n",
                 oplabel, pagelabel, WTERMSIG(status));
          failed = true;
        } else {
          printf("TEST-PASS | %s %s | signal %d (as expected)\n",
                 oplabel, pagelabel, WTERMSIG(status));
        }
      } else {
        printf("ERROR | %s %s | unexpected exit status %d\n",
               oplabel, pagelabel, status);
        exit(2);
      }
    }
#endif
  }
  return failed;
}

int
main()
{
#ifdef _WIN32
  GetSystemInfo(&_sinfo);
#elif !defined(__OS2__)
  _pagesize = sysconf(_SC_PAGESIZE);
#endif

  uintptr_t ncontrol = ReserveNegativeControl();
  uintptr_t pcontrol = ReservePositiveControl();
  uintptr_t poison = ReservePoisonArea();

  if (!ncontrol || !pcontrol || !poison)
    return 2;

  bool failed = false;
  failed |= TestPage("negative control", ncontrol, 1);
  failed |= TestPage("positive control", pcontrol, 0);
  failed |= TestPage("poison area", poison, 0);

  return failed ? 1 : 0;
}
