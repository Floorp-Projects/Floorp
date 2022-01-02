dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_CONFIG_CLANG_PLUGIN], [

if test -n "$ENABLE_CLANG_PLUGIN"; then
    dnl For some reason the llvm-config downloaded from clang.llvm.org for clang3_8
    dnl produces a -isysroot flag for a sysroot which might not ship when passed
    dnl --cxxflags. We use sed to remove this argument so that builds work on OSX
    dnl
    dnl For a similar reason, we remove any -gcc-toolchain arguments, since the
    dnl directories specified by such arguments might not exist on the current
    dnl machine.
    LLVM_CXXFLAGS=`$LLVM_CONFIG --cxxflags | sed -e 's/-isysroot [[^ ]]*//' -e 's/-gcc-toolchain [[^ ]]*//'`

    LLVM_LDFLAGS=`$LLVM_CONFIG --ldflags | tr '\n' ' '`

    if test "${HOST_OS_ARCH}" = "Darwin"; then
        dnl We need to make sure that we use the symbols coming from the clang
        dnl binary. In order to do this, we need to pass -flat_namespace and
        dnl -undefined suppress to the linker. This makes sure that we link the
        dnl symbols into the flat namespace provided by clang, and thus get
        dnl access to all of the symbols which are undefined in our dylib as we
        dnl are building it right now, and also that we don't fail the build
        dnl due to undefined symbols (which will be provided by clang).
        CLANG_LDFLAGS="-Wl,-flat_namespace -Wl,-undefined,suppress"
        dnl We are loaded into clang, so we don't need to link to very many things,
        dnl we just need to link to clangASTMatchers because it is not used by clang
        CLANG_LDFLAGS="$CLANG_LDFLAGS `$LLVM_CONFIG --prefix`/lib/libclangASTMatchers.a"
        dnl We need to remove -L/path/to/clang/lib from LDFLAGS to ensure that we
        dnl don't accidentally link against the libc++ there which is a newer
        dnl version that what our build machines have installed.
        LLVM_LDFLAGS=`echo "$LLVM_LDFLAGS" | sed -E 's/-L[[^ ]]+\/clang\/lib//'`
    elif test "${HOST_OS_ARCH}" = "WINNT"; then
        CLANG_LDFLAGS="clangASTMatchers.lib clang.lib"
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
        dnl We'll also want to replace `-std:` with `-Xclang -std=` so that
        dnl LLVM_CXXFLAGS can correctly override the `-Xclang -std=` set by
        dnl toolchain.configure.
        LLVM_CXXFLAGS=`echo "$LLVM_CXXFLAGS"|sed -e 's/ \(-Xclang \|\)-std[[:=]]/ -Xclang -std=/'`

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

    CLANG_PLUGIN_FLAGS="-Xclang -load -Xclang $CLANG_PLUGIN -Xclang -add-plugin -Xclang moz-check"

    AC_DEFINE(MOZ_CLANG_PLUGIN)
fi

if test -n "$ENABLE_MOZSEARCH_PLUGIN"; then
    if test -z "${ENABLE_CLANG_PLUGIN}"; then
        AC_MSG_ERROR([Can't use mozsearch plugin without --enable-clang-plugin.])
    fi

    CLANG_PLUGIN_FLAGS="$CLANG_PLUGIN_FLAGS -Xclang -add-plugin -Xclang mozsearch-index"

    dnl Parameters are: srcdir, outdir (path where output JSON is stored), objdir.
    CLANG_PLUGIN_FLAGS="$CLANG_PLUGIN_FLAGS -Xclang -plugin-arg-mozsearch-index -Xclang $_topsrcdir"
    CLANG_PLUGIN_FLAGS="$CLANG_PLUGIN_FLAGS -Xclang -plugin-arg-mozsearch-index -Xclang $_objdir/mozsearch_index"
    CLANG_PLUGIN_FLAGS="$CLANG_PLUGIN_FLAGS -Xclang -plugin-arg-mozsearch-index -Xclang $_objdir"

    AC_DEFINE(MOZ_MOZSEARCH_PLUGIN)
fi

AC_SUBST_LIST(CLANG_PLUGIN_FLAGS)
AC_SUBST_LIST(LLVM_CXXFLAGS)
AC_SUBST_LIST(LLVM_LDFLAGS)
AC_SUBST_LIST(CLANG_LDFLAGS)

AC_SUBST(ENABLE_CLANG_PLUGIN)
AC_SUBST(ENABLE_CLANG_PLUGIN_ALPHA)
AC_SUBST(ENABLE_MOZSEARCH_PLUGIN)

])
