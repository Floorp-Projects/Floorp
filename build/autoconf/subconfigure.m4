dnl We are not running in a real autoconf environment. So we're using real m4
dnl here, not the crazier environment that autoconf provides.

dnl Autoconf expects [] for quotes; give it to them
changequote([, ])

dnl AC_DEFUN is provided to use instead of define in autoconf. Provide it too.
define([AC_DEFUN], [define($1, [$2])])

dnl AC_ARG_ENABLE(FEATURE, HELP-STRING, IF-TRUE[, IF-FALSE])
dnl We have to ignore the help string due to how help works in autoconf...
AC_DEFUN([MOZ_AC_ARG_ENABLE],
[#] Check whether --enable-[$1] or --disable-[$1] was given.
[if test "[${enable_]patsubst([$1], -, _)+set}" = set; then
  enableval="[$enable_]patsubst([$1], -, _)"
  $3
ifelse([$4], , , [else
  $4
])dnl
fi
])

dnl AC_MSG_ERROR(error-description)
AC_DEFUN([AC_MSG_ERROR], [{ echo "configure: error: $1" 1>&2; exit 1; }])

AC_DEFUN([AC_MSG_WARN],  [ echo "configure: warning: $1" 1>&2 ])

dnl Add the variable to the list of substitution variables
AC_DEFUN([AC_SUBST],
[
_subconfigure_ac_subst_args="$_subconfigure_ac_subst_args $1"
])

dnl Override for AC_DEFINE.
AC_DEFUN([AC_DEFINE],
[
cat >>confdefs.h <<\EOF
[#define] $1 ifelse($#, 2, [$2], $#, 3, [$2], 1)
EOF
cat >> confdefs.pytmp <<\EOF
    (''' $1 ''', ifelse($#, 2, [r''' $2 '''], $#, 3, [r''' $2 '''], ' 1 '))
EOF
])

dnl AC_OUTPUT_SUBDIRS(subdirectory)
AC_DEFUN([AC_OUTPUT_SUBDIRS], [do_output_subdirs "$1"])
