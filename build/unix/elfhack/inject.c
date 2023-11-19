/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <elf.h>

/* The Android NDK headers define those */
#undef Elf_Ehdr
#undef Elf_Addr

#if defined(__LP64__)
#  define Elf_Ehdr Elf64_Ehdr
#  define Elf_Phdr Elf64_Phdr
#  define Elf_Addr Elf64_Addr
#  define Elf_Word Elf64_Word
#  define Elf_Dyn Elf64_Dyn
#else
#  define Elf_Phdr Elf32_Phdr
#  define Elf_Ehdr Elf32_Ehdr
#  define Elf_Addr Elf32_Addr
#  define Elf_Word Elf32_Word
#  define Elf_Dyn Elf32_Dyn
#endif

#ifdef RELRHACK
#  include "relrhack.h"
#  define mprotect_cb mprotect
#  define sysconf_cb sysconf

#else
// On ARM, PC-relative function calls have a limit in how far they can jump,
// which might not be enough for e.g. libxul.so. The easy way out would be
// to use the long_call attribute, which forces the compiler to generate code
// that can call anywhere, but clang doesn't support the attribute yet
// (https://bugs.llvm.org/show_bug.cgi?id=40623), and while the command-line
// equivalent does exist, it's currently broken
// (https://bugs.llvm.org/show_bug.cgi?id=40624). So we create a manual
// trampoline, corresponding to the code GCC generates with long_call.
#  ifdef __arm__
__attribute__((section(".text._init_trampoline"), naked)) int init_trampoline(
    int argc, char** argv, char** env) {
  __asm__ __volatile__(
      // thumb doesn't allow to use r12/ip with ldr, and thus would require an
      // additional push/pop to save/restore the modified register, which would
      // also change the call into a blx. It's simpler to switch to arm.
      ".arm\n"
      "  ldr ip, .LADDR\n"
      ".LAFTER:\n"
      "  add ip, pc, ip\n"
      "  bx ip\n"
      ".LADDR:\n"
      "  .word real_original_init-(.LAFTER+8)\n");
}
#  endif

// On aarch64, a similar problem exists, but long_call is not an option at all
// (even GCC doesn't support them on aarch64).
#  ifdef __aarch64__
__attribute__((section(".text._init_trampoline"), naked)) int init_trampoline(
    int argc, char** argv, char** env) {
  __asm__ __volatile__(
      "  adrp x8, .LADDR\n"
      "  add x8, x8, :lo12:.LADDR\n"  // adrp + add gives us the full address
                                      // for .LADDR
      "  ldr x0, [x8]\n"  // Load the address of real_original_init relative to
                          // .LADDR
      "  add x0, x8, x0\n"  // Add the address of .LADDR
      "  br x0\n"           // Branch to real_original_init
      ".LADDR:\n"
      "  .xword real_original_init-.LADDR\n");
}
#  endif

extern __attribute__((visibility("hidden"))) void original_init(int argc,
                                                                char** argv,
                                                                char** env);

extern __attribute__((visibility("hidden"))) Elf_Addr relhack[];
extern __attribute__((visibility("hidden"))) Elf_Addr relhack_end[];

extern __attribute__((visibility("hidden"))) int (*mprotect_cb)(void* addr,
                                                                size_t len,
                                                                int prot);
extern __attribute__((visibility("hidden"))) long (*sysconf_cb)(int name);
extern __attribute__((visibility("hidden"))) char relro_start[];
extern __attribute__((visibility("hidden"))) char relro_end[];
#endif

extern __attribute__((visibility("hidden"))) Elf_Ehdr __ehdr_start;

static inline __attribute__((always_inline)) void do_relocations(
    Elf_Addr* relhack, Elf_Addr* relhack_end) {
  Elf_Addr* ptr;
  for (Elf_Addr* entry = relhack; entry < relhack_end; entry++) {
    if ((*entry & 1) == 0) {
      ptr = (Elf_Addr*)((intptr_t)&__ehdr_start + *entry);
      *ptr += (intptr_t)&__ehdr_start;
    } else {
      size_t remaining = (8 * sizeof(Elf_Addr) - 1);
      Elf_Addr bits = *entry;
      do {
        bits >>= 1;
        remaining--;
        ptr++;
        if (bits & 1) {
          *ptr += (intptr_t)&__ehdr_start;
        }
      } while (bits);
      ptr += remaining;
    }
  }
}

#ifndef RELRHACK
__attribute__((section(".text._init_noinit"))) int init_noinit(int argc,
                                                               char** argv,
                                                               char** env) {
  do_relocations(relhack, relhack_end);
  return 0;
}

__attribute__((section(".text._init"))) int init(int argc, char** argv,
                                                 char** env) {
  do_relocations(relhack, relhack_end);
  original_init(argc, argv, env);
  // Ensure there is no tail-call optimization, avoiding the use of the
  // B.W instruction in Thumb for the call above.
  return 0;
}
#endif

static inline __attribute__((always_inline)) void do_relocations_with_relro(
    Elf_Addr* relhack, Elf_Addr* relhack_end, char* relro_start,
    char* relro_end) {
  long page_size = sysconf_cb(_SC_PAGESIZE);
  uintptr_t aligned_relro_start = ((uintptr_t)relro_start) & ~(page_size - 1);
  // The relro segment may not end at a page boundary. If that's the case, the
  // remainder of the page needs to stay read-write, so the last page is never
  // set read-only. Thus the aligned relro end is page-rounded down.
  uintptr_t aligned_relro_end = ((uintptr_t)relro_end) & ~(page_size - 1);
  // By the time the injected code runs, the relro segment is read-only. But
  // we want to apply relocations in it, so we set it r/w first. We'll restore
  // it to read-only in relro_post.
  mprotect_cb((void*)aligned_relro_start,
              aligned_relro_end - aligned_relro_start, PROT_READ | PROT_WRITE);

  do_relocations(relhack, relhack_end);

  mprotect_cb((void*)aligned_relro_start,
              aligned_relro_end - aligned_relro_start, PROT_READ);
#ifndef RELRHACK
  // mprotect_cb and sysconf_cb are allocated in .bss, so we need to restore
  // them to a NULL value.
  mprotect_cb = NULL;
  sysconf_cb = NULL;
#endif
}

#ifndef RELRHACK
__attribute__((section(".text._init_noinit_relro"))) int init_noinit_relro(
    int argc, char** argv, char** env) {
  do_relocations_with_relro(relhack, relhack_end, relro_start, relro_end);
  return 0;
}

__attribute__((section(".text._init_relro"))) int init_relro(int argc,
                                                             char** argv,
                                                             char** env) {
  do_relocations_with_relro(relhack, relhack_end, relro_start, relro_end);
  original_init(argc, argv, env);
  return 0;
}
#else

extern __attribute__((visibility("hidden"))) Elf_Dyn _DYNAMIC[];

static void _relrhack_init(void) {
  // Get the location of the SHT_RELR data from the PT_DYNAMIC segment.
  uintptr_t elf_header = (uintptr_t)&__ehdr_start;
  Elf_Addr* relhack = NULL;
  Elf_Word size = 0;
  for (Elf_Dyn* dyn = _DYNAMIC; dyn->d_tag != DT_NULL; dyn++) {
    if ((dyn->d_tag & ~DT_RELRHACK_BIT) == DT_RELR) {
      relhack = (Elf_Addr*)(elf_header + dyn->d_un.d_ptr);
    } else if ((dyn->d_tag & ~DT_RELRHACK_BIT) == DT_RELRSZ) {
      size = dyn->d_un.d_val;
    }
  }

  Elf_Addr* relhack_end = (Elf_Addr*)((uintptr_t)relhack + size);

  // Find the location of the PT_GNU_RELRO segment in the program headers.
  Elf_Phdr* phdr = (Elf_Phdr*)(elf_header + __ehdr_start.e_phoff);
  char* relro_start = NULL;
  char* relro_end = NULL;
  for (int i = 0; i < __ehdr_start.e_phnum; i++) {
    if (phdr[i].p_type == PT_GNU_RELRO) {
      relro_start = (char*)(elf_header + phdr[i].p_vaddr);
      relro_end = (char*)(relro_start + phdr[i].p_memsz);
      break;
    }
  }

  if (relro_start != relro_end) {
    do_relocations_with_relro(relhack, relhack_end, relro_start, relro_end);
  } else {
    do_relocations(relhack, relhack_end);
  }
}

// The Android CRT doesn't contain an init function.
#  ifndef ANDROID
extern __attribute__((visibility("hidden"))) void _init(int argc, char** argv,
                                                        char** env);
#  endif

void _relrhack_wrap_init(int argc, char** argv, char** env) {
  _relrhack_init();
#  ifndef ANDROID
  _init(argc, argv, env);
#  endif
}
#endif
