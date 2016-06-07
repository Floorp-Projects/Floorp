/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include <istream>
#include <string>
#include <stdarg.h>
#include <stdio.h>
#include <mozilla/Assertions.h>

/* GLIBCXX_3.4.8  is from gcc 4.1.1 (111691)
   GLIBCXX_3.4.9  is from gcc 4.2.0 (111690)
   GLIBCXX_3.4.10 is from gcc 4.3.0 (126287)
   GLIBCXX_3.4.11 is from gcc 4.4.0 (133006)
   GLIBCXX_3.4.12 is from gcc 4.4.1 (147138)
   GLIBCXX_3.4.13 is from gcc 4.4.2 (151127)
   GLIBCXX_3.4.14 is from gcc 4.5.0 (151126)
   GLIBCXX_3.4.15 is from gcc 4.6.0 (160071)
   GLIBCXX_3.4.16 is from gcc 4.6.1 (172240)
   GLIBCXX_3.4.17 is from gcc 4.7.0 (174383)
   GLIBCXX_3.4.18 is from gcc 4.8.0 (190787)
   GLIBCXX_3.4.19 is from gcc 4.8.1 (199309)
   GLIBCXX_3.4.20 is from gcc 4.9.0 (199307)
   GLIBCXX_3.4.21 is from gcc 5.0 (210290)

This file adds the necessary compatibility tricks to avoid symbols with
version GLIBCXX_3.4.16 and bigger, keeping binary compatibility with
libstdc++ 4.6.1.

*/

#define GLIBCXX_VERSION(a, b, c) (((a) << 16) | ((b) << 8) | (c))

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 20)
namespace std {

    /* We shouldn't be throwing exceptions at all, but it sadly turns out
       we call STL (inline) functions that do. */
    void __throw_out_of_range_fmt(char const* fmt, ...)
    {
        va_list ap;
        char buf[1024]; // That should be big enough.

        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        buf[sizeof(buf) - 1] = 0;
        va_end(ap);

        __throw_range_error(buf);
    }

}
#endif

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 20)
/* Technically, this symbol is not in GLIBCXX_3.4.20, but in CXXABI_1.3.8,
   but that's equivalent, version-wise. Those calls are added by the compiler
   itself on `new Class[n]` calls. */
extern "C" void
__cxa_throw_bad_array_new_length()
{
    MOZ_CRASH();
}
#endif

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 21)
/* While we generally don't build with exceptions, we have some host tools
 * that do use them. libstdc++ from GCC 5.0 added exception constructors with
 * char const* argument. Older versions only have a constructor with
 * std::string. */
namespace std {
    runtime_error::runtime_error(char const* s)
    : runtime_error(std::string(s))
    {
    }
}
#endif
