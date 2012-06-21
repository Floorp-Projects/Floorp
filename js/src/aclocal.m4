dnl
dnl Local autoconf macros used with mozilla
dnl The contents of this file are under the Public Domain.
dnl

builtin(include, build/autoconf/pkg.m4)dnl
builtin(include, build/autoconf/nspr.m4)dnl
builtin(include, build/autoconf/altoptions.m4)dnl
builtin(include, build/autoconf/moznbytetype.m4)dnl
builtin(include, build/autoconf/mozprog.m4)dnl
builtin(include, build/autoconf/mozheader.m4)dnl
builtin(include, build/autoconf/mozcommonheader.m4)dnl
builtin(include, build/autoconf/acwinpaths.m4)dnl
builtin(include, build/autoconf/lto.m4)dnl
builtin(include, build/autoconf/gcc-pr49911.m4)dnl
builtin(include, build/autoconf/frameptr.m4)dnl
builtin(include, build/autoconf/compiler-opts.m4)dnl
builtin(include, build/autoconf/expandlibs.m4)dnl
builtin(include, build/autoconf/arch.m4)dnl
builtin(include, build/autoconf/android.m4)dnl

MOZ_PROG_CHECKMSYS()
