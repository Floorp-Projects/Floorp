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

dnl MOZ_READ_MYCONFIG() - Read in 'myconfig.sh' file
AC_DEFUN([MOZ_READ_MOZCONFIG],
[AC_REQUIRE([AC_INIT_BINSH])dnl
inserted=
dnl Shell is hard, so here is what the following does:
dnl - Reset $@ (command line arguments)
dnl - Add the configure options from mozconfig to $@ one by one
dnl - Add the original command line arguments after that, one by one
dnl
dnl There are several tricks involved:
dnl - It is not possible to preserve the whitespaces in $@ by assigning to
dnl   another variable, so the two first steps above need to happen in the first
dnl   iteration of the third step.
dnl - We always want the configure options to be added, so the loop must be
dnl   iterated at least once, so we add a dummy argument first, and discard it.
dnl - something | while read line ... makes the while run in a subshell, meaning
dnl   that anything it does is not propagated to the main shell, so we can't do
dnl   set -- foo there. As a consequence, what the while loop reading mach
dnl   environment output does is output a set of shell commands for the main shell
dnl   to eval.
dnl - Extra care is due when lines from mach environment output contain special
dnl   shell characters, so we use ' for quoting and ensure no ' end up in between
dnl   the quoting mark unescaped.
dnl Some of the above is directly done in mach environment --format=configure.
failed_eval() {
  echo "Failed eval'ing the following:"
  $(dirname [$]0)/[$1]/mach environment --format=configure
  exit 1
}

set -- dummy "[$]@"
for ac_option
do
  if test -z "$inserted"; then
    set --
    eval "$($(dirname [$]0)/[$1]/mach environment --format=configure)" || failed_eval
    inserted=1
  else
    set -- "[$]@" "$ac_option"
  fi
done
])
