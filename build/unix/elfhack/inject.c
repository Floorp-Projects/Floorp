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
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Addr Elf64_Addr
#else
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Addr Elf32_Addr
#endif

extern __attribute__((visibility("hidden"))) void original_init(int argc, char **argv, char **env);

extern __attribute__((visibility("hidden"))) Elf32_Rel relhack[];
extern __attribute__((visibility("hidden"))) Elf_Ehdr elf_header;

extern __attribute__((visibility("hidden"))) int (*mprotect_cb)(void *addr, size_t len, int prot);
extern __attribute__((visibility("hidden"))) long (*sysconf_cb)(int name);
extern __attribute__((visibility("hidden"))) char relro_start[];
extern __attribute__((visibility("hidden"))) char relro_end[];

static inline __attribute__((always_inline))
void do_relocations(void)
{
    Elf32_Rel *rel;
    Elf_Addr *ptr, *start;
    for (rel = relhack; rel->r_offset; rel++) {
        start = (Elf_Addr *)((intptr_t)&elf_header + rel->r_offset);
        for (ptr = start; ptr < &start[rel->r_info]; ptr++)
            *ptr += (intptr_t)&elf_header;
    }
}

__attribute__((section(".text._init_noinit")))
int init_noinit(int argc, char **argv, char **env)
{
    do_relocations();
    return 0;
}

__attribute__((section(".text._init")))
int init(int argc, char **argv, char **env)
{
    do_relocations();
    original_init(argc, argv, env);
    // Ensure there is no tail-call optimization, avoiding the use of the
    // B.W instruction in Thumb for the call above.
    return 0;
}

static inline __attribute__((always_inline))
void do_relocations_with_relro(void)
{
    long page_size = sysconf_cb(_SC_PAGESIZE);
    uintptr_t aligned_relro_start = ((uintptr_t) relro_start) & ~(page_size - 1);
    // The relro segment may not end at a page boundary. If that's the case, the
    // remainder of the page needs to stay read-write, so the last page is never
    // set read-only. Thus the aligned relro end is page-rounded down.
    uintptr_t aligned_relro_end = ((uintptr_t) relro_end) & ~(page_size - 1);
    // By the time the injected code runs, the relro segment is read-only. But
    // we want to apply relocations in it, so we set it r/w first. We'll restore
    // it to read-only in relro_post.
    mprotect_cb((void *)aligned_relro_start,
                aligned_relro_end - aligned_relro_start,
                PROT_READ | PROT_WRITE);

    do_relocations();

    mprotect_cb((void *)aligned_relro_start,
                aligned_relro_end - aligned_relro_start,
                PROT_READ);
    // mprotect_cb and sysconf_cb are allocated in .bss, so we need to restore
    // them to a NULL value.
    mprotect_cb = NULL;
    sysconf_cb = NULL;
}

__attribute__((section(".text._init_noinit_relro")))
int init_noinit_relro(int argc, char **argv, char **env)
{
    do_relocations_with_relro();
    return 0;
}

__attribute__((section(".text._init_relro")))
int init_relro(int argc, char **argv, char **env)
{
    do_relocations_with_relro();
    original_init(argc, argv, env);
    return 0;
}
