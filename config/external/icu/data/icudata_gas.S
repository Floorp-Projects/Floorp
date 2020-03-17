# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
#ifdef PREFIX
#  define DATA_SYMBOL _##ICU_DATA_SYMBOL
#else
#  define DATA_SYMBOL ICU_DATA_SYMBOL
#endif
.global DATA_SYMBOL
.data
.balign 16
DATA_SYMBOL:
        .incbin ICU_DATA_FILE
