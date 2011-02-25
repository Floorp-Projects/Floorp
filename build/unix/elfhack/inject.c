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
 * The Original Code is elfhack.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Hommey <mh@glandium.org>
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
 * ***** END LICENSE BLOCK ***** */

#include <stdint.h>
#include <elf.h>

/* The Android NDK headers define those */
#undef Elf_Ehdr
#undef Elf_Addr

#if BITS == 32
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Addr Elf32_Addr
#else
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Addr Elf64_Addr
#endif

extern __attribute__((visibility("hidden"))) void original_init(int argc, char **argv, char **env);

extern __attribute__((visibility("hidden"))) Elf32_Rel relhack[];
extern __attribute__((visibility("hidden"))) Elf_Ehdr elf_header;

void init(int argc, char **argv, char **env)
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
}
