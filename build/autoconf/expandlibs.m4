dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_EXPAND_LIBS],
[
dnl ========================================================
dnl =
dnl = Check what kind of list files are supported by the
dnl = linker
dnl =
dnl ========================================================

AC_CACHE_CHECK(what kind of list files are supported by the linker,
    moz_cv_expand_libs_list_style,
    [echo "int main() {return 0;}" > conftest.${ac_ext}
     dnl Because BFD ld doesn't work with LTO + linker scripts, we
     dnl must pass the LTO CFLAGS to the compile command, and the LTO
     dnl LDFLAGS to all subsequent link commands.
     dnl https://sourceware.org/bugzilla/show_bug.cgi?id=23600
     if AC_TRY_COMMAND(${CC-cc} -o conftest.${OBJ_SUFFIX} -c $MOZ_LTO_CFLAGS $CFLAGS $CPPFLAGS conftest.${ac_ext} 1>&5) && test -s conftest.${OBJ_SUFFIX}; then
         echo "INPUT(conftest.${OBJ_SUFFIX})" > conftest.list
         if test "$CC_TYPE" = "clang-cl"; then
             link="$LINKER -OUT:conftest${ac_exeext}"
         else
             link="${CC-cc} -o conftest${ac_exeext}"
         fi
         if AC_TRY_COMMAND($link $MOZ_LTO_LDFLAGS $LDFLAGS conftest.list $LIBS 1>&5) && test -s conftest${ac_exeext}; then
             moz_cv_expand_libs_list_style=linkerscript
         else
             echo "conftest.${OBJ_SUFFIX}" > conftest.list
             dnl -filelist is for the OS X linker.  We need to try -filelist
             dnl first because clang understands @file, but may pass an
             dnl oversized argument list to the linker depending on the
             dnl contents of @file.
             if AC_TRY_COMMAND($link $MOZ_LTO_LDFLAGS $LDFLAGS [-Wl,-filelist,conftest.list] $LIBS 1>&5) && test -s conftest${ac_exeext}; then
                 moz_cv_expand_libs_list_style=filelist
             elif AC_TRY_COMMAND($link $MOZ_LTO_LDFLAGS $LDFLAGS @conftest.list $LIBS 1>&5) && test -s conftest${ac_exeext}; then
                 moz_cv_expand_libs_list_style=list
             else
                 AC_ERROR([Couldn't find one that works])
             fi
         fi
     else
         dnl We really don't expect to get here, but just in case
         AC_ERROR([couldn't compile a simple C file])
     fi
     rm -rf conftest*])

EXPAND_LIBS_LIST_STYLE=$moz_cv_expand_libs_list_style
AC_SUBST(EXPAND_LIBS_LIST_STYLE)

])
