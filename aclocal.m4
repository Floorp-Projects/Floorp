dnl
dnl Local autoconf macros used with mozilla
dnl The contents of this file are under the Public Domain.
dnl

builtin(include, build/autoconf/hooks.m4)dnl
builtin(include, build/autoconf/config.status.m4)dnl
builtin(include, build/autoconf/toolchain.m4)dnl
builtin(include, build/autoconf/altoptions.m4)dnl
builtin(include, build/autoconf/mozprog.m4)dnl
builtin(include, build/autoconf/mozheader.m4)dnl
builtin(include, build/autoconf/compiler-opts.m4)dnl
builtin(include, build/autoconf/arch.m4)dnl
builtin(include, build/autoconf/clang-plugin.m4)dnl
builtin(include, build/autoconf/sanitize.m4)dnl

MOZ_PROG_CHECKMSYS()

# Read the user's .mozconfig script.  We can't do this in
# configure.in: autoconf puts the argument parsing code above anything
# expanded from configure.in, and we need to get the configure options
# from .mozconfig in place before that argument parsing code.
MOZ_READ_MOZCONFIG(.)
