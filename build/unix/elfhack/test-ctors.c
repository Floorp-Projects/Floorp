/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "test.c"

/* Recent binutils would put .ctors content into a .init_array section */
__attribute__((section(".manual_ctors"), used))
static void (*ctors[])() = { (void (*)())-1, end_test, test, NULL };

__attribute__((section(".init")))
void _init() {
    void (**func)() = &ctors[sizeof(ctors) / sizeof(void (*)()) - 1];
    while (*(--func) != (void (*)())-1) {
        (*func)();
    }
}
