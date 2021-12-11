dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Set of hotfixes to address issues in autoconf 2.13

dnl Divert AC_CHECK_FUNC so that the #includes it uses can't interfere
dnl with the function it tests.
dnl So, when testing e.g. posix_memalign, any #include that AC_CHECK_FUNC
dnl prints is replaced with:
dnl   #define posix_memalign innocuous_posix_memalign
dnl   #include "theinclude"
dnl   #undef posix_memalign
dnl This avoids double declaration of that function when the header normally
dnl declares it, while the test itself is just expecting the function not to be
dnl declared at all, and declares it differently (which doesn't matter for the
dnl test itself).
dnl More recent versions of autoconf are essentially doing this.
define([_AC_CHECK_FUNC],defn([AC_CHECK_FUNC]))dnl
define([AC_CHECK_FUNC], [dnl
patsubst(_AC_CHECK_FUNC($@), [#include.*$], [#define $1 innocuous_$1
\&
#undef $1])])dnl
