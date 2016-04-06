;; This Source Code Form is subject to the terms of the Mozilla Public
;; License, v. 2.0. If a copy of the MPL was not distributed with this
;; file, You can obtain one at http://mozilla.org/MPL/2.0/.

%ifdef PREFIX
    %define DATA_SYMBOL _ %+ ICU_DATA_SYMBOL
%else
    %define DATA_SYMBOL ICU_DATA_SYMBOL
%endif

%ifidn __OUTPUT_FORMAT__,elf
    %define FORMAT_ELF 1
%elifidn __OUTPUT_FORMAT__,elf32
    %define FORMAT_ELF 1
%elifidn __OUTPUT_FORMAT__,elf64
    %define FORMAT_ELF 1
%else
    %define FORMAT_ELF 0
%endif

%if FORMAT_ELF
    global DATA_SYMBOL:data hidden
    ; This is needed for ELF, otherwise the GNU linker assumes the stack is executable by default.
    [SECTION .note.GNU-stack noalloc noexec nowrite progbits]
%else
    global DATA_SYMBOL
%endif

SECTION .rodata align=16
DATA_SYMBOL:
        incbin ICU_DATA_FILE
