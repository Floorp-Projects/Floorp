dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_CONFIG_CLANG_PLUGIN], [

MOZ_ARG_ENABLE_BOOL(clang-plugin,
[  --enable-clang-plugin   Enable building with the mozilla clang plugin ],
   ENABLE_CLANG_PLUGIN=1,
   ENABLE_CLANG_PLUGIN= )
if test -n "$ENABLE_CLANG_PLUGIN"; then
    if test -z "${CLANG_CC}${CLANG_CL}"; then
        AC_MSG_ERROR([Can't use clang plugin without clang.])
    fi

    AC_MSG_CHECKING([for llvm-config])
    if test -z "$LLVMCONFIG"; then
      if test -n "$CLANG_CL"; then
          CXX_COMPILER="$(dirname "$CXX")/clang"
      else
          CXX_COMPILER="${CXX}"
      fi
      LLVMCONFIG=`$CXX_COMPILER -print-prog-name=llvm-config`
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
    dnl Use tr instead of xargs in order to avoid problems with path separators on Windows.
    LLVM_LDFLAGS=`$LLVMCONFIG --system-libs | tr '\n' ' '`
    LLVM_LDFLAGS="$LLVM_LDFLAGS `$LLVMCONFIG --ldflags --libs core mc analysis asmparser mcparser bitreader option | tr '\n' ' '`"

    if test "${HOST_OS_ARCH}" = "Darwin"; then
        CLANG_LDFLAGS="-lclangFrontend -lclangDriver -lclangSerialization"
        CLANG_LDFLAGS="$CLANG_LDFLAGS -lclangParse -lclangSema -lclangAnalysis"
        CLANG_LDFLAGS="$CLANG_LDFLAGS -lclangEdit -lclangAST -lclangLex"
        CLANG_LDFLAGS="$CLANG_LDFLAGS -lclangBasic -lclangASTMatchers"
    elif test "${HOST_OS_ARCH}" = "WINNT"; then
        CLANG_LDFLAGS="clangFrontend.lib clangDriver.lib clangSerialization.lib"
        CLANG_LDFLAGS="$CLANG_LDFLAGS clangParse.lib clangSema.lib clangAnalysis.lib"
        CLANG_LDFLAGS="$CLANG_LDFLAGS clangEdit.lib clangAST.lib clangLex.lib"
        CLANG_LDFLAGS="$CLANG_LDFLAGS clangBasic.lib clangASTMatchers.lib"
    else
        CLANG_LDFLAGS="-lclangASTMatchers"
    fi

    if test -n "$CLANG_CL"; then
        dnl The llvm-config coming with clang-cl may give us arguments in the
        dnl /ARG form, which in msys will be interpreted as a path name.  So we
        dnl need to split the args and convert the leading slashes that we find
        dnl into a dash.
        LLVM_REPLACE_CXXFLAGS=''
        for arg in $LLVM_CXXFLAGS; do
            dnl The following expression replaces a leading slash with a dash.
            dnl Also replace any backslashes with forward slash.
            arg=`echo "$arg"|sed -e 's/^\//-/' -e 's/\\\\/\//g'`
            LLVM_REPLACE_CXXFLAGS="$LLVM_REPLACE_CXXFLAGS $arg"
        done
        LLVM_CXXFLAGS="$LLVM_REPLACE_CXXFLAGS"

        LLVM_REPLACE_LDFLAGS=''
        for arg in $LLVM_LDFLAGS; do
            dnl The following expression replaces a leading slash with a dash.
            dnl Also replace any backslashes with forward slash.
            arg=`echo "$arg"|sed -e 's/^\//-/' -e 's/\\\\/\//g'`
            LLVM_REPLACE_LDFLAGS="$LLVM_REPLACE_LDFLAGS $arg"
        done
        LLVM_LDFLAGS="$LLVM_REPLACE_LDFLAGS"

        CLANG_REPLACE_LDFLAGS=''
        for arg in $CLANG_LDFLAGS; do
            dnl The following expression replaces a leading slash with a dash.
            dnl Also replace any backslashes with forward slash.
            arg=`echo "$arg"|sed -e 's/^\//-/' -e 's/\\\\/\//g'`
            CLANG_REPLACE_LDFLAGS="$CLANG_REPLACE_LDFLAGS $arg"
        done
        CLANG_LDFLAGS="$CLANG_REPLACE_LDFLAGS"
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
