dnl
dnl Local autoconf macros used with mozilla
dnl The contents of this file are under the Public Domain.
dnl

builtin(include, build/autoconf/hotfixes.m4)dnl
builtin(include, build/autoconf/acwinpaths.m4)dnl
builtin(include, build/autoconf/hooks.m4)dnl
builtin(include, build/autoconf/config.status.m4)dnl
builtin(include, build/autoconf/toolchain.m4)dnl
builtin(include, build/autoconf/wrapper.m4)dnl
builtin(include, build/autoconf/nspr.m4)dnl
builtin(include, build/autoconf/nspr-build.m4)dnl
builtin(include, build/autoconf/nss.m4)dnl
builtin(include, build/autoconf/pkg.m4)dnl
builtin(include, build/autoconf/codeset.m4)dnl
builtin(include, build/autoconf/altoptions.m4)dnl
builtin(include, build/autoconf/mozprog.m4)dnl
builtin(include, build/autoconf/mozheader.m4)dnl
builtin(include, build/autoconf/mozcommonheader.m4)dnl
builtin(include, build/autoconf/lto.m4)dnl
builtin(include, build/autoconf/llvm-pr8927.m4)dnl
builtin(include, build/autoconf/frameptr.m4)dnl
builtin(include, build/autoconf/compiler-opts.m4)dnl
builtin(include, build/autoconf/expandlibs.m4)dnl
builtin(include, build/autoconf/arch.m4)dnl
builtin(include, build/autoconf/android.m4)dnl
builtin(include, build/autoconf/zlib.m4)dnl
builtin(include, build/autoconf/linux.m4)dnl
builtin(include, build/autoconf/python-virtualenv.m4)dnl
builtin(include, build/autoconf/winsdk.m4)dnl
builtin(include, build/autoconf/icu.m4)dnl
builtin(include, build/autoconf/ffi.m4)dnl
builtin(include, build/autoconf/clang-plugin.m4)dnl
builtin(include, build/autoconf/alloc.m4)dnl
builtin(include, build/autoconf/ios.m4)dnl

MOZ_PROG_CHECKMSYS()

# Read the user's .mozconfig script.  We can't do this in
# configure.in: autoconf puts the argument parsing code above anything
# expanded from configure.in, and we need to get the configure options
# from .mozconfig in place before that argument parsing code.
MOZ_READ_MOZCONFIG(.)
