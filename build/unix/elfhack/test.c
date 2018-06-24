/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEF
DEF(This)
DEF(is)
DEF(a)
DEF(test)
DEF(of)
DEF(string)
DEF(array)
DEF(for)
DEF(use)
DEF(with)
DEF(elfhack)
DEF(to)
DEF(see)
DEF(whether)
DEF(it)
DEF(breaks)
DEF(anything)
DEF(but)
DEF(one)
DEF(needs)
DEF(quite)
DEF(some)
DEF(strings)
DEF(before)
DEF(the)
DEF(program)
DEF(can)
DEF(do)
DEF(its)
DEF(work)
DEF(efficiently)
DEF(Without)
DEF(enough)
DEF(data)
DEF(relocation)
DEF(sections)
DEF(are)
DEF(not)
DEF(sufficiently)
DEF(large)
DEF(and)
DEF(injected)
DEF(code)
DEF(wouldnt)
DEF(fit)
DEF(Said)
DEF(otherwise)
DEF(we)
DEF(need)
DEF(more)
DEF(words)
DEF(than)
DEF(up)
DEF(here)
DEF(so)
DEF(that)
DEF(relocations)
DEF(take)
DEF(significant)
DEF(bytes)
DEF(amounts)
DEF(which)
DEF(isnt)
DEF(exactly)
DEF(easily)
DEF(achieved)
DEF(like)
DEF(this)
DEF(Actually)
DEF(I)
DEF(must)
DEF(cheat)
DEF(by)
DEF(including)
DEF(these)
DEF(phrases)
DEF(several)
DEF(times)

#else
#pragma GCC visibility push(default)
#include <stdlib.h>
#include <stdio.h>

#define DEF(w) static const char str_ ## w[] = #w;
#include "test.c"
#undef DEF

const char *strings[] = {
#define DEF(w) str_ ## w,
#include "test.c"
#include "test.c"
#include "test.c"
};

/* Create a hole between two zones of relative relocations */
const int hole[] = {
    42, 42, 42, 42
};

const char *strings2[] = {
#include "test.c"
#include "test.c"
#include "test.c"
#include "test.c"
#include "test.c"
#undef DEF
};

static int ret = 1;

int print_status() {
    fprintf(stderr, "%s\n", ret ? "FAIL" : "PASS");
    return ret;
}

/* On ARM, this creates a .tbss section before .init_array, which
 * elfhack could then pick instead of .init_array.
 * Also, when .tbss is big enough, elfhack may wrongfully consider
 * following sections as part of the PT_TLS segment.
 * Finally, gold makes TLS segments end on an aligned virtual address,
 * even when the underlying section ends before that, and elfhack
 * sanity checks may yield an error. */
__thread int foo;
__thread long long int bar[512];

/* We need a .bss that can hold at least 2 pointers. The static in
 * end_test() plus this variable should do. */
size_t dummy;

void end_test() {
    static size_t count = 0;
    /* Only exit when both constructors have been called */
    if (++count == 2)
        ret = 0;
}

void test() {
    int i = 0, j = 0;
#define DEF_(a,i,w) \
    if (a[i++] != str_ ## w) return;
#define DEF(w) DEF_(strings,i,w)
#include "test.c"
#include "test.c"
#include "test.c"
#undef DEF
#define DEF(w) DEF_(strings2,j,w)
#include "test.c"
#include "test.c"
#include "test.c"
#include "test.c"
#include "test.c"
#undef DEF
    if (i != sizeof(strings)/sizeof(strings[0]) &&
        j != sizeof(strings2)/sizeof(strings2[0]))
        fprintf(stderr, "WARNING: Test doesn't cover the whole array\n");
    end_test();
}

#pragma GCC visibility pop
#endif
