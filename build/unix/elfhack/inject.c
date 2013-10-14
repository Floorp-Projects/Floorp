/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
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

int init(int argc, char **argv, char **env)
{
    Elf32_Rel *rel;
    Elf_Addr *ptr, *start;
    for (rel = relhack; rel->r_offset; rel++) {
        start = (Elf_Addr *)((intptr_t)&elf_header + rel->r_offset);
        for (ptr = start; ptr < &start[rel->r_info]; ptr++)
            *ptr += (intptr_t)&elf_header;
    }

#ifndef NOINIT
    original_init(argc, argv, env);
#endif
    // Ensure there is no tail-call optimization, avoiding the use of the
    // B.W instruction in Thumb for the call above.
    return 0;
}
