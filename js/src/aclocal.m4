dnl
dnl Local autoconf macros used with mozilla
dnl The contents of this file are under the Public Domain.
dnl 

builtin(include, build/autoconf/pkg.m4)dnl
builtin(include, build/autoconf/nspr.m4)dnl
builtin(include, build/autoconf/altoptions.m4)dnl
builtin(include, build/autoconf/moznbytetype.m4)dnl
builtin(include, build/autoconf/mozprog.m4)dnl
builtin(include, build/autoconf/acwinpaths.m4)dnl

MOZ_PROG_CHECKMSYS()
