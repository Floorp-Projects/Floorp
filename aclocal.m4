dnl
dnl Local autoconf macros used with mozilla
dnl The contents of this file are under the Public Domain.
dnl 

builtin(include, build/autoconf/glib.m4)dnl
builtin(include, build/autoconf/gtk.m4)dnl
builtin(include, build/autoconf/libIDL.m4)dnl
builtin(include, build/autoconf/libIDL-2.m4)dnl
builtin(include, build/autoconf/nspr.m4)dnl
builtin(include, build/autoconf/libart.m4)dnl
builtin(include, build/autoconf/pkg.m4)dnl
builtin(include, build/autoconf/freetype2.m4)dnl
builtin(include, build/autoconf/codeset.m4)dnl
dnl
define(MOZ_TOPSRCDIR,.)dnl MOZ_TOPSRCDIR is used in altoptions.m4
builtin(include, build/autoconf/altoptions.m4)dnl

