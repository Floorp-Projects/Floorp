dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_CONFIG_CLANG_PLUGIN], [

MOZ_ARG_ENABLE_BOOL(clang-plugin,
[  --enable-clang-plugin   Enable building with the mozilla clang plugin ],
   ENABLE_CLANG_PLUGIN=1,
   ENABLE_CLANG_PLUGIN= )
if test -n "$ENABLE_CLANG_PLUGIN"; then
    if test -z "$CLANG_CC"; then
        AC_MSG_ERROR([Can't use clang plugin without clang.])
    fi

    AC_MSG_CHECKING([for llvm-config])
    if test -z "$LLVMCONFIG"; then
      LLVMCONFIG=`$CXX -print-prog-name=llvm-config`
    fi

    if test -z "$LLVMCONFIG"; then
      LLVMCONFIG=`which llvm-config`
    fi

    if test ! -x "$LLVMCONFIG"; then
      AC_MSG_RESULT([not found])
      AC_MSG_ERROR([Cannot find an llvm-config binary for building a clang plugin])
    fi

    AC_MSG_RESULT([$LLVMCONFIG])

    if test -z "$LLVMCONFIG"; then
        AC_MSG_ERROR([Cannot find an llvm-config binary for building a clang plugin])
    fi
    LLVM_CXXFLAGS=`$LLVMCONFIG --cxxflags`
    dnl The clang package we use on OSX is old, and its llvm-config doesn't
    dnl recognize --system-libs, so ask for that separately.  llvm-config's
    dnl failure here is benign, so we can ignore it if it happens.
    LLVM_LDFLAGS=`$LLVMCONFIG --system-libs | xargs`
    LLVM_LDFLAGS="$LLVM_LDFLAGS `$LLVMCONFIG --ldflags --libs core mc analysis asmparser mcparser bitreader option | xargs`"

    if test "${HOST_OS_ARCH}" = "Darwin"; then
        CLANG_LDFLAGS="-lclangFrontend -lclangDriver -lclangSerialization"
        CLANG_LDFLAGS="$CLANG_LDFLAGS -lclangParse -lclangSema -lclangAnalysis"
        CLANG_LDFLAGS="$CLANG_LDFLAGS -lclangEdit -lclangAST -lclangLex"
        CLANG_LDFLAGS="$CLANG_LDFLAGS -lclangBasic -lclangASTMatchers"
    else
        CLANG_LDFLAGS="-lclangASTMatchers"
    fi

    dnl Check for the new ASTMatcher API names.  Since this happened in the
    dnl middle of the 3.8 cycle, our CLANG_VERSION_FULL is impossible to use
    dnl correctly, so we have to detect this at configure time.
    AC_CACHE_CHECK(for new ASTMatcher names,
                   ac_cv_have_new_ASTMatcher_names,
        [
            AC_LANG_SAVE
            AC_LANG_CPLUSPLUS
            _SAVE_CXXFLAGS="$CXXFLAGS"
            CXXFLAGS="${LLVM_CXXFLAGS}"
            AC_TRY_COMPILE([#include "clang/ASTMatchers/ASTMatchers.h"],
                           [clang::ast_matchers::cxxConstructExpr();],
                           ac_cv_have_new_ASTMatcher_names="yes",
                           ac_cv_have_new_ASTMatcher_names="no")
            CXXFLAGS="$_SAVE_CXXFLAGS"
            AC_LANG_RESTORE
        ])
    if test "$ac_cv_have_new_ASTMatcher_names" = "yes"; then
      LLVM_CXXFLAGS="$LLVM_CXXFLAGS -DHAVE_NEW_ASTMATCHER_NAMES"
    fi

    AC_DEFINE(MOZ_CLANG_PLUGIN)
fi

AC_SUBST(LLVM_CXXFLAGS)
AC_SUBST(LLVM_LDFLAGS)
AC_SUBST(CLANG_LDFLAGS)

AC_SUBST(ENABLE_CLANG_PLUGIN)

])
