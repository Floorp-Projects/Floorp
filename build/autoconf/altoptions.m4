dnl The contents of this file are subject to the Netscape Public
dnl License Version 1.1 (the "License"); you may not use this file
dnl except in compliance with the License. You may obtain a copy of
dnl the License at http://www.mozilla.org/NPL/
dnl
dnl Software distributed under the License is distributed on an "AS
dnl IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
dnl implied. See the License for the specific language governing
dnl rights and limitations under the License.
dnl
dnl The Original Code is mozilla.org code.
dnl
dnl The Initial Developer of the Original Code is Netscape
dnl Communications Corporation.  Portions created by Netscape are
dnl Copyright (C) 1999 Netscape Communications Corporation. All
dnl Rights Reserved.
dnl
dnl Contributor(s): 
dnl

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
dnl MOZ_READ_MYCONFIG() - Read in 'myconfig.sh' file


dnl MOZ_TWO_STRING_TEST(NAME, STR1, IF-STR1, STR2, IF-STR2 [, ELSE])
AC_DEFUN(MOZ_TWO_STRING_TEST,
[if test "$enableval" = "[$2]"; then
    ifelse([$3], , :, [$3])
  elif test "$enableval" = "[$4]"; then
    ifelse([$5], , :, [$5])
  else
    ifelse([$6], ,
      [AC_MSG_ERROR([Option, [$1], does not take an argument ($enableval).])],
      [$6])
  fi])

dnl MOZ_ARG_ENABLE_BOOL(NAME, HELP, IF-YES [, IF-NO [, ELSE]])
AC_DEFUN(MOZ_ARG_ENABLE_BOOL,
[AC_ARG_ENABLE([$1], [$2], 
 [MOZ_TWO_STRING_TEST([$1], yes, [$3], no, [$4])],
 [$5])])

dnl MOZ_ARG_DISABLE_BOOL(NAME, HELP, IF-NO [, IF-YES [, ELSE]])
AC_DEFUN(MOZ_ARG_DISABLE_BOOL,
[AC_ARG_ENABLE([$1], [$2],
 [MOZ_TWO_STRING_TEST([$1], no, [$3], yes, [$4])],
 [$5])])

dnl MOZ_ARG_ENABLE_STRING(NAME, HELP, IF-SET [, ELSE])
AC_DEFUN(MOZ_ARG_ENABLE_STRING,
[AC_ARG_ENABLE([$1], [$2], [$3], [$4])])

dnl MOZ_ARG_ENABLE_BOOL_OR_STRING(NAME, HELP, IF-YES, IF-NO, IF-SET[, ELSE]]])
AC_DEFUN(MOZ_ARG_ENABLE_BOOL_OR_STRING,
[ifelse([$5], , 
 [errprint([Option, $1, needs an "IF-SET" argument.
])
  m4exit(1)],
 [AC_ARG_ENABLE([$1], [$2],
  [MOZ_TWO_STRING_TEST([$1], yes, [$3], no, [$4], [$5])],
  [$6])])])

dnl MOZ_ARG_WITH_BOOL(NAME, HELP, IF-YES [, IF-NO [, ELSE])
AC_DEFUN(MOZ_ARG_WITH_BOOL,
[AC_ARG_WITH([$1], [$2],
 [MOZ_TWO_STRING_TEST([$1], yes, [$3], no, [$4])],
 [$5])])

dnl MOZ_ARG_WITHOUT_BOOL(NAME, HELP, IF-NO [, IF-YES [, ELSE])
AC_DEFUN(MOZ_ARG_WITHOUT_BOOL,
[AC_ARG_WITH([$1], [$2],
 [MOZ_TWO_STRING_TEST([$1], no, [$3], yes, [$4])],
 [$5])])

dnl MOZ_ARG_WITH_STRING(NAME, HELP, IF-SET [, ELSE])
AC_DEFUN(MOZ_ARG_WITH_STRING,
[AC_ARG_WITH([$1], [$2], [$3], [$4])])

dnl MOZ_ARG_HEADER(Comment)
dnl This is used by webconfig to group options
define(MOZ_ARG_HEADER, [# $1])

dnl MOZ_READ_MYCONFIG() - Read in 'myconfig.sh' file
AC_DEFUN(MOZ_READ_MOZCONFIG,
[AC_REQUIRE([AC_INIT_BINSH])dnl
# Read in '.mozconfig' script to set the initial options.
# See the mozconfig2configure script for more details.
_AUTOCONF_TOOLS_DIR=`dirname [$]0`/[$1]/build/autoconf
. $_AUTOCONF_TOOLS_DIR/mozconfig2configure])

dnl This gets inserted at the top of the configure script
MOZ_READ_MOZCONFIG(MOZ_TOPSRCDIR)
