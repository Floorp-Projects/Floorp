dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_CONFIG_SANITIZE], [

dnl ========================================================
dnl = Use Address Sanitizer
dnl ========================================================
if test -n "$MOZ_ASAN"; then
    if test "$CC_TYPE" = clang-cl ; then
        # Look for the ASan runtime binary
        if test "$TARGET_CPU" = "x86_64"; then
          MOZ_CLANG_RT_ASAN_LIB=clang_rt.asan_dynamic-x86_64.dll
        else
          MOZ_CLANG_RT_ASAN_LIB=clang_rt.asan_dynamic-i386.dll
        fi
        # We use MOZ_PATH_PROG in order to get a Windows style path.
        MOZ_PATH_PROG(MOZ_CLANG_RT_ASAN_LIB_PATH, $MOZ_CLANG_RT_ASAN_LIB)
        if test -z "$MOZ_CLANG_RT_ASAN_LIB_PATH"; then
            AC_MSG_ERROR([Couldn't find $MOZ_CLANG_RT_ASAN_LIB.  It should be available in the same location as clang-cl.])
        fi
        AC_SUBST(MOZ_CLANG_RT_ASAN_LIB_PATH)
        # Suppressing errors in recompiled code.
        if test "$OS_ARCH" = "WINNT"; then
            CFLAGS="-fsanitize-blacklist=$_topsrcdir/build/sanitizers/asan_blacklist_win.txt $CFLAGS"
            CXXFLAGS="-fsanitize-blacklist=$_topsrcdir/build/sanitizers/asan_blacklist_win.txt $CXXFLAGS"
        fi
    fi
    ASAN_FLAGS="-fsanitize=address"
    if test "$OS_TARGET" = Linux; then
        # -fno-sanitize-address-globals-dead-stripping is used to work around
        # https://github.com/rust-lang/rust/issues/113404
        # It forces clang not to use __asan_register_elf_globals/__asan_globals_registered,
        # avoiding the conflict with rust.
        ASAN_FLAGS="$ASAN_FLAGS -fno-sanitize-address-globals-dead-stripping"
    fi
    CFLAGS="$ASAN_FLAGS $CFLAGS"
    CXXFLAGS="$ASAN_FLAGS $CXXFLAGS"
    if test "$CC_TYPE" != clang-cl ; then
        LDFLAGS="-fsanitize=address -rdynamic $LDFLAGS"
    fi
fi

dnl ========================================================
dnl = Test for whether the compiler is compatible with the
dnl = given sanitize options.
dnl ========================================================
AC_TRY_LINK(,,,AC_MSG_ERROR([compiler is incompatible with sanitize options]))

])
