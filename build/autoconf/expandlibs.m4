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
    EXPAND_LIBS_LIST_STYLE,
    [echo "int main() {return 0;}" > conftest.${ac_ext}
     if AC_TRY_COMMAND(${CC-cc} -o conftest.${OBJ_SUFFIX} -c $CFLAGS $CPPFLAGS conftest.${ac_ext} 1>&5) && test -s conftest.${OBJ_SUFFIX}; then
         echo "INPUT(conftest.${OBJ_SUFFIX})" > conftest.list
         if AC_TRY_COMMAND(${CC-cc} -o conftest${ac_exeext} $CFLAGS $CPPFLAGS $LDFLAGS conftest.list $LIBS 1>&5) && test -s conftest${ac_exeext}; then
             EXPAND_LIBS_LIST_STYLE=linkerscript
         else
             echo "conftest.${OBJ_SUFFIX}" > conftest.list
             dnl -filelist is for the OS X linker.  We need to try -filelist
             dnl first because clang understands @file, but may pass an
             dnl oversized argument list to the linker depending on the
             dnl contents of @file.
             if AC_TRY_COMMAND(${CC-cc} -o conftest${ac_exeext} $CFLAGS $CPPFLAGS $LDFLAGS [-Wl,-filelist,conftest.list] $LIBS 1>&5) && test -s conftest${ac_exeext}; then
                 EXPAND_LIBS_LIST_STYLE=filelist
             elif AC_TRY_COMMAND(${CC-cc} -o conftest${ac_exeext} $CFLAGS $CPPFLAGS $LDFLAGS @conftest.list $LIBS 1>&5) && test -s conftest${ac_exeext}; then
                 EXPAND_LIBS_LIST_STYLE=list
             else
                 AC_ERROR([Couldn't find one that works])
             fi
         fi
     else
         dnl We really don't expect to get here, but just in case
         AC_ERROR([couldn't compile a simple C file])
     fi
     rm -rf conftest*])

AC_SUBST(EXPAND_LIBS_LIST_STYLE)

])
