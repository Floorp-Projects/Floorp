dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_CONFIG_SANITIZE], [

dnl ========================================================
dnl = Link Time Optimization (LTO)
dnl ========================================================
if test -n "$MOZ_LTO"; then
    MOZ_LLVM_HACKS=1
    AC_DEFINE(MOZ_LTO)
    MOZ_PATH_PROG(LLVM_SYMBOLIZER, llvm-symbolizer)

    CFLAGS="$CFLAGS $MOZ_LTO_CFLAGS"
    CPPFLAGS="$CPPFLAGS $MOZ_LTO_CFLAGS"
    CXXFLAGS="$CXXFLAGS $MOZ_LTO_CFLAGS"
    LDFLAGS="$LDFLAGS $MOZ_LTO_LDFLAGS"
fi
AC_SUBST(MOZ_LTO)

dnl ========================================================
dnl = Use Address Sanitizer
dnl ========================================================
if test -n "$MOZ_ASAN"; then
    MOZ_LLVM_HACKS=1
    if test -n "$CLANG_CL"; then
        # Look for the ASan runtime binary
        if test "$CPU_ARCH" = "x86_64"; then
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
    CFLAGS="-fsanitize=address $CFLAGS"
    CXXFLAGS="-fsanitize=address $CXXFLAGS"
    if test -z "$CLANG_CL"; then
        LDFLAGS="-fsanitize=address $LDFLAGS"
    fi
    AC_DEFINE(MOZ_ASAN)
    MOZ_PATH_PROG(LLVM_SYMBOLIZER, llvm-symbolizer)
fi
AC_SUBST(MOZ_ASAN)

dnl ========================================================
dnl = Use Memory Sanitizer
dnl ========================================================
MOZ_ARG_ENABLE_BOOL(memory-sanitizer,
[  --enable-memory-sanitizer       Enable Memory Sanitizer (default=no)],
    MOZ_MSAN=1,
    MOZ_MSAN= )
if test -n "$MOZ_MSAN"; then
    MOZ_LLVM_HACKS=1
    CFLAGS="-fsanitize=memory -fsanitize-memory-track-origins $CFLAGS"
    CXXFLAGS="-fsanitize=memory -fsanitize-memory-track-origins $CXXFLAGS"
    if test -z "$CLANG_CL"; then
        LDFLAGS="-fsanitize=memory -fsanitize-memory-track-origins $LDFLAGS"
    fi
    AC_DEFINE(MOZ_MSAN)
    MOZ_PATH_PROG(LLVM_SYMBOLIZER, llvm-symbolizer)
fi
AC_SUBST(MOZ_MSAN)

dnl ========================================================
dnl = Use Thread Sanitizer
dnl ========================================================
MOZ_ARG_ENABLE_BOOL(thread-sanitizer,
[  --enable-thread-sanitizer       Enable Thread Sanitizer (default=no)],
   MOZ_TSAN=1,
   MOZ_TSAN= )
if test -n "$MOZ_TSAN"; then
    MOZ_LLVM_HACKS=1
    CFLAGS="-fsanitize=thread $CFLAGS"
    CXXFLAGS="-fsanitize=thread $CXXFLAGS"
    if test -z "$CLANG_CL"; then
        LDFLAGS="-fsanitize=thread $LDFLAGS"
    fi
    AC_DEFINE(MOZ_TSAN)
    MOZ_PATH_PROG(LLVM_SYMBOLIZER, llvm-symbolizer)
fi
AC_SUBST(MOZ_TSAN)

dnl ========================================================
dnl = Use UndefinedBehavior Sanitizer to find integer overflows
dnl ========================================================

MOZ_ARG_ENABLE_BOOL(signed-overflow-sanitizer,
[  --enable-signed-overflow-sanitizer       Enable UndefinedBehavior Sanitizer (Signed Integer Overflow Parts, default=no)],
   MOZ_SIGNED_OVERFLOW_SANITIZE=1,
   MOZ_SIGNED_OVERFLOW_SANITIZE= )
MOZ_ARG_ENABLE_BOOL(unsigned-overflow-sanitizer,
[  --enable-unsigned-overflow-sanitizer       Enable UndefinedBehavior Sanitizer (Unsigned Integer Overflow Parts, default=no)],
   MOZ_UNSIGNED_OVERFLOW_SANITIZE=1,
   MOZ_UNSIGNED_OVERFLOW_SANITIZE= )

if test -n "$MOZ_SIGNED_OVERFLOW_SANITIZE$MOZ_UNSIGNED_OVERFLOW_SANITIZE"; then
    MOZ_LLVM_HACKS=1
    MOZ_UBSAN=1
    SANITIZER_BLACKLISTS=""
    if test -n "$MOZ_SIGNED_OVERFLOW_SANITIZE"; then
        SANITIZER_BLACKLISTS="-fsanitize-blacklist=$_topsrcdir/build/sanitizers/ubsan_signed_overflow_blacklist.txt $SANITIZER_BLACKLISTS"
        CFLAGS="-fsanitize=signed-integer-overflow $CFLAGS"
        CXXFLAGS="-fsanitize=signed-integer-overflow $CXXFLAGS"
        if test -z "$CLANG_CL"; then
            LDFLAGS="-fsanitize=signed-integer-overflow $LDFLAGS"
        fi
        AC_DEFINE(MOZ_SIGNED_OVERFLOW_SANITIZE)
    fi
    if test -n "$MOZ_UNSIGNED_OVERFLOW_SANITIZE"; then
        SANITIZER_BLACKLISTS="-fsanitize-blacklist=$_topsrcdir/build/sanitizers/ubsan_unsigned_overflow_blacklist.txt $SANITIZER_BLACKLISTS"
        CFLAGS="-fsanitize=unsigned-integer-overflow $CFLAGS"
        CXXFLAGS="-fsanitize=unsigned-integer-overflow $CXXFLAGS"
        if test -z "$CLANG_CL"; then
            LDFLAGS="-fsanitize=unsigned-integer-overflow $LDFLAGS"
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

# The LLVM symbolizer is used by all sanitizers
AC_SUBST(LLVM_SYMBOLIZER)

dnl ========================================================
dnl = Enable hacks required for LLVM instrumentations
dnl ========================================================
MOZ_ARG_ENABLE_BOOL(llvm-hacks,
[  --enable-llvm-hacks       Enable workarounds required for several LLVM instrumentations (default=no)],
    MOZ_LLVM_HACKS=1,
    MOZ_LLVM_HACKS= )
if test -n "$MOZ_LLVM_HACKS"; then
    MOZ_NO_WLZDEFS=1
    MOZ_CFLAGS_NSS=1
fi
AC_SUBST(MOZ_NO_WLZDEFS)
AC_SUBST(MOZ_CFLAGS_NSS)

dnl ========================================================
dnl = Test for whether the compiler is compatible with the
dnl = given sanitize options.
dnl ========================================================
AC_TRY_LINK(,,,AC_MSG_ERROR([compiler is incompatible with sanitize options]))

])
