/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <stdlib.h>
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
void relro_pre()
{
    // By the time the injected code runs, the relro segment is read-only. But
    // we want to apply relocations in it, so we set it r/w first. We'll restore
    // it to read-only in relro_post.
    mprotect_cb(relro_start, relro_end - relro_start, PROT_READ | PROT_WRITE);
}

static inline __attribute__((always_inline))
void relro_post()
{
    mprotect_cb(relro_start, relro_end - relro_start, PROT_READ);
    // mprotect_cb is a pointer allocated in .bss, so we need to restore it to
    // a NULL value.
    mprotect_cb = NULL;
}

__attribute__((section(".text._init_noinit_relro")))
int init_noinit_relro(int argc, char **argv, char **env)
{
    relro_pre();
    do_relocations();
    relro_post();
    return 0;
}

__attribute__((section(".text._init_relro")))
int init_relro(int argc, char **argv, char **env)
{
    relro_pre();
    do_relocations();
    relro_post();
    original_init(argc, argv, env);
    return 0;
}
