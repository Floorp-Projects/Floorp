dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl altoptions.m4 - An alternative way of specifying command-line options.
dnl    These macros are needed to support a menu-based configurator.
dnl    This file also includes the macro, AM_READ_MYCONFIG, for reading
dnl    the 'myconfig.m4' file.

dnl Send comments, improvements, bugs to Steve Lamm (slamm@netscape.com).


dnl MOZ_ARG_ENABLE_BOOL(           NAME, HELP, IF-YES [, IF-NO [, ELSE]])
dnl MOZ_ARG_DISABLE_BOOL(          NAME, HELP, IF-NO [, IF-YES [, ELSE]])
dnl MOZ_ARG_ENABLE_STRING(         NAME, HELP, IF-SET [, ELSE])
dnl MOZ_ARG_ENABLE_BOOL_OR_STRING( NAME, HELP, IF-YES, IF-NO, IF-SET[, ELSE]]])
dnl MOZ_ARG_WITH_BOOL(             NAME, HELP, IF-YES [, IF-NO [, ELSE])
dnl MOZ_ARG_WITHOUT_BOOL(          NAME, HELP, IF-NO [, IF-YES [, ELSE])
dnl MOZ_ARG_WITH_STRING(           NAME, HELP, IF-SET [, ELSE])
dnl MOZ_ARG_HEADER(Comment)
dnl MOZ_CHECK_PTHREADS(            NAME, IF-YES [, ELSE ])
dnl MOZ_READ_MYCONFIG() - Read in 'myconfig.sh' file


dnl MOZ_TWO_STRING_TEST(NAME, VAL, STR1, IF-STR1, STR2, IF-STR2 [, ELSE])
AC_DEFUN([MOZ_TWO_STRING_TEST],
[if test "[$2]" = "[$3]"; then
    ifelse([$4], , :, [$4])
  elif test "[$2]" = "[$5]"; then
    ifelse([$6], , :, [$6])
  else
    ifelse([$7], ,
      [AC_MSG_ERROR([Option, [$1], does not take an argument ([$2]).])],
      [$7])
  fi])

dnl MOZ_ARG_ENABLE_BOOL(NAME, HELP, IF-YES [, IF-NO [, ELSE]])
AC_DEFUN([MOZ_ARG_ENABLE_BOOL],
[AC_ARG_ENABLE([$1], [$2], 
 [MOZ_TWO_STRING_TEST([$1], [$enableval], yes, [$3], no, [$4])],
 [$5])])

dnl MOZ_ARG_DISABLE_BOOL(NAME, HELP, IF-NO [, IF-YES [, ELSE]])
AC_DEFUN([MOZ_ARG_DISABLE_BOOL],
[AC_ARG_ENABLE([$1], [$2],
 [MOZ_TWO_STRING_TEST([$1], [$enableval], no, [$3], yes, [$4])],
 [$5])])

dnl MOZ_ARG_ENABLE_STRING(NAME, HELP, IF-SET [, ELSE])
AC_DEFUN([MOZ_ARG_ENABLE_STRING],
[AC_ARG_ENABLE([$1], [$2], [$3], [$4])])

dnl MOZ_ARG_ENABLE_BOOL_OR_STRING(NAME, HELP, IF-YES, IF-NO, IF-SET[, ELSE]]])
AC_DEFUN([MOZ_ARG_ENABLE_BOOL_OR_STRING],
[ifelse([$5], , 
 [errprint([Option, $1, needs an "IF-SET" argument.
])
  m4exit(1)],
 [AC_ARG_ENABLE([$1], [$2],
  [MOZ_TWO_STRING_TEST([$1], [$enableval], yes, [$3], no, [$4], [$5])],
  [$6])])])

dnl MOZ_ARG_WITH_BOOL(NAME, HELP, IF-YES [, IF-NO [, ELSE])
AC_DEFUN([MOZ_ARG_WITH_BOOL],
[AC_ARG_WITH([$1], [$2],
 [MOZ_TWO_STRING_TEST([$1], [$withval], yes, [$3], no, [$4])],
 [$5])])

dnl MOZ_ARG_WITHOUT_BOOL(NAME, HELP, IF-NO [, IF-YES [, ELSE])
AC_DEFUN([MOZ_ARG_WITHOUT_BOOL],
[AC_ARG_WITH([$1], [$2],
 [MOZ_TWO_STRING_TEST([$1], [$withval], no, [$3], yes, [$4])],
 [$5])])

dnl MOZ_ARG_WITH_STRING(NAME, HELP, IF-SET [, ELSE])
AC_DEFUN([MOZ_ARG_WITH_STRING],
[AC_ARG_WITH([$1], [$2], [$3], [$4])])

dnl MOZ_ARG_HEADER(Comment)
dnl This is used by webconfig to group options
define(MOZ_ARG_HEADER, [# $1])

dnl
dnl Apparently, some systems cannot properly check for the pthread
dnl library unless <pthread.h> is included so we need to test
dnl using it
dnl
dnl MOZ_CHECK_PTHREADS(lib, success, failure)
AC_DEFUN([MOZ_CHECK_PTHREADS],
[
AC_MSG_CHECKING([for pthread_create in -l$1])
echo "
    #include <pthread.h>
    #include <stdlib.h>
    void *foo(void *v) { int a = 1;  } 
    int main() { 
        pthread_t t;
        if (!pthread_create(&t, 0, &foo, 0)) {
            pthread_join(t, 0);
        }
        exit(0);
    }" > dummy.c ;
    echo "${CC-cc} -o dummy${ac_exeext} dummy.c $CFLAGS $CPPFLAGS -l[$1] $LDFLAGS $LIBS" 1>&5;
    ${CC-cc} -o dummy${ac_exeext} dummy.c $CFLAGS $CPPFLAGS -l[$1] $LDFLAGS $LIBS 2>&5;
    _res=$? ;
    rm -f dummy.c dummy${ac_exeext} ;
    if test "$_res" = "0"; then
        AC_MSG_RESULT([yes])
        [$2]
    else
        AC_MSG_RESULT([no])
        [$3]
    fi
])

dnl MOZ_READ_MYCONFIG() - Read in 'myconfig.sh' file
AC_DEFUN([MOZ_READ_MOZCONFIG],
[AC_REQUIRE([AC_INIT_BINSH])dnl
# Read in '.mozconfig' script to set the initial options.
# See the mozconfig2configure script for more details.
_AUTOCONF_TOOLS_DIR=`dirname [$]0`/[$1]/build/autoconf
. $_AUTOCONF_TOOLS_DIR/mozconfig2configure])
