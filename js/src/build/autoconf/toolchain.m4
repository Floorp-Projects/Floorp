dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_TOOL_VARIABLES],
[
GNU_AS=
GNU_LD=
GNU_CC=
GNU_CXX=
CC_VERSION='N/A'
CXX_VERSION='N/A'
if test "$GCC" = "yes"; then
    GNU_CC=1
    CC_VERSION=`$CC -v 2>&1 | grep 'gcc version'`
fi
if test "$GXX" = "yes"; then
    GNU_CXX=1
    CXX_VERSION=`$CXX -v 2>&1 | grep 'gcc version'`
fi
if test "`echo | $AS -o conftest.out -v 2>&1 | grep -c GNU`" != "0"; then
    GNU_AS=1
fi
rm -f conftest.out
if test "`echo | $LD -v 2>&1 | grep -c GNU`" != "0"; then
    GNU_LD=1
fi
if test "$GNU_CC"; then
    if `$CC -print-prog-name=ld` -v 2>&1 | grep -c GNU >/dev/null; then
        GCC_USE_GNU_LD=1
    fi
fi

INTEL_CC=
INTEL_CXX=
if test "$GCC" = yes; then
   if test "`$CC -help 2>&1 | grep -c 'Intel(R) C++ Compiler'`" != "0"; then
     INTEL_CC=1
   fi
fi

if test "$GXX" = yes; then
   if test "`$CXX -help 2>&1 | grep -c 'Intel(R) C++ Compiler'`" != "0"; then
     INTEL_CXX=1
   fi
fi

CLANG_CC=
CLANG_CXX=
if test "$GCC" = yes; then
   if test "`$CC -v 2>&1 | grep -c 'clang version'`" != "0"; then
     CLANG_CC=1
   fi
fi

if test "$GXX" = yes; then
   if test "`$CXX -v 2>&1 | grep -c 'clang version'`" != "0"; then
     CLANG_CXX=1
   fi
fi
AC_SUBST(CLANG_CXX)
])
