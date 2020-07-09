/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(_WIN32) && defined(__i386__)
// Mark the object as SAFESEH-enabled.
.def @feat.00;
.scl 3;
.type 0;
.endef
.global @feat.00
.set @feat.00, 1
#endif

.global ICU_DATA_SYMBOL
#ifdef __APPLE__
.data
.const
#else
.section .rodata
#endif
.balign 16
ICU_DATA_SYMBOL:
    .incbin ICU_DATA_FILE
