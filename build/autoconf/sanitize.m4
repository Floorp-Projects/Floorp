dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_CONFIG_SANITIZE], [

dnl ========================================================
dnl = Use Address Sanitizer
dnl ========================================================
if test -n "$MOZ_ASAN"; then
    if test -n "$CLANG_CL"; then
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
    if test -z "$CLANG_CL"; then
        LDFLAGS="-fsanitize=address -rdynamic $LDFLAGS"
    fi
    AC_DEFINE(MOZ_ASAN)
    MOZ_PATH_PROG(LLVM_SYMBOLIZER, llvm-symbolizer)
fi
AC_SUBST(MOZ_ASAN)

dnl ========================================================
dnl = Use Memory Sanitizer
dnl ========================================================
if test -n "$MOZ_MSAN"; then
    CFLAGS="-fsanitize=memory -fsanitize-memory-track-origins $CFLAGS"
    CXXFLAGS="-fsanitize=memory -fsanitize-memory-track-origins $CXXFLAGS"
    if test -z "$CLANG_CL"; then
        LDFLAGS="-fsanitize=memory -fsanitize-memory-track-origins -rdynamic $LDFLAGS"
    fi
    AC_DEFINE(MOZ_MSAN)
    MOZ_PATH_PROG(LLVM_SYMBOLIZER, llvm-symbolizer)
fi
AC_SUBST(MOZ_MSAN)

dnl ========================================================
dnl = Use Thread Sanitizer
dnl ========================================================
if test -n "$MOZ_TSAN"; then
    CFLAGS="-fsanitize=thread $CFLAGS"
    CXXFLAGS="-fsanitize=thread $CXXFLAGS"
    if test -z "$CLANG_CL"; then
        LDFLAGS="-fsanitize=thread -rdynamic $LDFLAGS"
    fi
    AC_DEFINE(MOZ_TSAN)
    MOZ_PATH_PROG(LLVM_SYMBOLIZER, llvm-symbolizer)
fi
AC_SUBST(MOZ_TSAN)

dnl ========================================================
dnl = Use UndefinedBehavior Sanitizer (with custom checks)
dnl ========================================================
if test -n "$MOZ_UBSAN_CHECKS"; then
    MOZ_UBSAN=1
    UBSAN_TXT="$_objdir/ubsan_blacklist.txt"
    cat $_topsrcdir/build/sanitizers/ubsan_*_blacklist.txt > $UBSAN_TXT
    UBSAN_FLAGS="-fsanitize=$MOZ_UBSAN_CHECKS -fno-sanitize-recover=$MOZ_UBSAN_CHECKS -fsanitize-blacklist=$UBSAN_TXT"
    CFLAGS="$UBSAN_FLAGS $CFLAGS"
    CXXFLAGS="$UBSAN_FLAGS $CXXFLAGS"
    if test -z "$CLANG_CL"; then
        LDFLAGS="-fsanitize=undefined -rdynamic $LDFLAGS"
    fi
    AC_DEFINE(MOZ_UBSAN)
    MOZ_PATH_PROG(LLVM_SYMBOLIZER, llvm-symbolizer)
fi
AC_SUBST(MOZ_UBSAN)

dnl ========================================================
dnl = Use UndefinedBehavior Sanitizer to find integer overflows
dnl ========================================================
if test -n "$MOZ_SIGNED_OVERFLOW_SANITIZE$MOZ_UNSIGNED_OVERFLOW_SANITIZE"; then
    MOZ_UBSAN=1
    SANITIZER_BLACKLISTS=""
    if test -n "$MOZ_SIGNED_OVERFLOW_SANITIZE"; then
        SANITIZER_BLACKLISTS="-fsanitize-blacklist=$_topsrcdir/build/sanitizers/ubsan_signed_overflow_blacklist.txt $SANITIZER_BLACKLISTS"
        CFLAGS="-fsanitize=signed-integer-overflow $CFLAGS"
        CXXFLAGS="-fsanitize=signed-integer-overflow $CXXFLAGS"
        if test -z "$CLANG_CL"; then
            LDFLAGS="-fsanitize=signed-integer-overflow -rdynamic $LDFLAGS"
        fi
        AC_DEFINE(MOZ_SIGNED_OVERFLOW_SANITIZE)
    fi
    if test -n "$MOZ_UNSIGNED_OVERFLOW_SANITIZE"; then
        SANITIZER_BLACKLISTS="-fsanitize-blacklist=$_topsrcdir/build/sanitizers/ubsan_unsigned_overflow_blacklist.txt $SANITIZER_BLACKLISTS"
        CFLAGS="-fsanitize=unsigned-integer-overflow $CFLAGS"
        CXXFLAGS="-fsanitize=unsigned-integer-overflow $CXXFLAGS"
        if test -z "$CLANG_CL"; then
            LDFLAGS="-fsanitize=unsigned-integer-overflow -rdynamic $LDFLAGS"
        fi
        AC_DEFINE(MOZ_UNSIGNED_OVERFLOW_SANITIZE)
    fi
    CFLAGS="$SANITIZER_BLACKLISTS $CFLAGS"
    CXXFLAGS="$SANITIZER_BLACKLISTS $CXXFLAGS"
    AC_DEFINE(MOZ_UBSAN)
    MOZ_PATH_PROG(LLVM_SYMBOLIZER, llvm-symbolizer)
fi
AC_SUBST(MOZ_SIGNED_OVERFLOW_SANITIZE)
AC_SUBST(MOZ_UNSIGNED_OVERFLOW_SANITIZE)
AC_SUBST(MOZ_UBSAN)

dnl =======================================================
dnl = Required for stand-alone (sanitizer-less) libFuzzer.
dnl =======================================================
if test -n "$LIBFUZZER"; then
   LDFLAGS="$LIBFUZZER_FLAGS -rdynamic $LDFLAGS"
fi

# The LLVM symbolizer is used by all sanitizers
AC_SUBST(LLVM_SYMBOLIZER)

dnl ========================================================
dnl = Test for whether the compiler is compatible with the
dnl = given sanitize options.
dnl ========================================================
AC_TRY_LINK(,,,AC_MSG_ERROR([compiler is incompatible with sanitize options]))

])
