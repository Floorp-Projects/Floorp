dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl For use in AC_SUBST replacement
define([MOZ_DIVERSION_SUBST], 11)

dnl Replace AC_SUBST to store values in a format suitable for python.
dnl The necessary comma after the tuple can't be put here because it
dnl can mess around with things like:
dnl    AC_SOMETHING(foo,AC_SUBST(),bar)
define([AC_SUBST],
[ifdef([AC_SUBST_SET_$1], [m4_fatal([Cannot use AC_SUBST and AC_SUBST_SET on the same variable ($1)])],
[ifdef([AC_SUBST_LIST_$1], [m4_fatal([Cannot use AC_SUBST and AC_SUBST_LIST on the same variable ($1)])],
[ifdef([AC_SUBST_$1], ,
[define([AC_SUBST_$1], )dnl
AC_DIVERT_PUSH(MOZ_DIVERSION_SUBST)dnl
    (''' $1 ''', r''' [$]$1 ''')
AC_DIVERT_POP()dnl
])])])])

dnl Like AC_SUBST, but makes the value available as a set in python,
dnl with values got from the value of the environment variable, split on
dnl whitespaces.
define([AC_SUBST_SET],
[ifdef([AC_SUBST_$1], [m4_fatal([Cannot use AC_SUBST and AC_SUBST_SET on the same variable ($1)])],
[ifdef([AC_SUBST_LIST$1], [m4_fatal([Cannot use AC_SUBST_LIST and AC_SUBST_SET on the same variable ($1)])],
[ifdef([AC_SUBST_SET_$1], ,
[define([AC_SUBST_SET_$1], )dnl
AC_DIVERT_PUSH(MOZ_DIVERSION_SUBST)dnl
    (''' $1 ''', unique_list(r''' [$]$1 '''.split()))
AC_DIVERT_POP()dnl
])])])])

dnl Like AC_SUBST, but makes the value available as a list in python,
dnl with values got from the value of the environment variable, split on
dnl whitespaces.
define([AC_SUBST_LIST],
[ifdef([AC_SUBST_$1], [m4_fatal([Cannot use AC_SUBST and AC_SUBST_LIST on the same variable ($1)])],
[ifdef([AC_SUBST_SET_$1], [m4_fatal([Cannot use AC_SUBST_SET and AC_SUBST_LIST on the same variable ($1)])],
[ifdef([AC_SUBST_LIST_$1], ,
[define([AC_SUBST_LIST_$1], )dnl
AC_DIVERT_PUSH(MOZ_DIVERSION_SUBST)dnl
    (''' $1 ''', list(r''' [$]$1 '''.split()))
AC_DIVERT_POP()dnl
])])])])

dnl Ignore AC_SUBSTs for variables we don't have use for but that autoconf
dnl itself exports.
define([AC_SUBST_CFLAGS], )
define([AC_SUBST_CPPFLAGS], )
define([AC_SUBST_CXXFLAGS], )
define([AC_SUBST_FFLAGS], )
define([AC_SUBST_DEFS], )
define([AC_SUBST_LDFLAGS], )
define([AC_SUBST_LIBS], )

dnl Wrap AC_DEFINE to store values in a format suitable for python.
dnl autoconf's AC_DEFINE still needs to be used to fill confdefs.h,
dnl which is #included during some compile checks.
dnl The necessary comma after the tuple can't be put here because it
dnl can mess around with things like:
dnl    AC_SOMETHING(foo,AC_DEFINE(),bar)
define([_MOZ_AC_DEFINE], defn([AC_DEFINE]))
define([AC_DEFINE],
[cat >> confdefs.pytmp <<\EOF
    (''' $1 ''', ifelse($#, 2, [r''' $2 '''], $#, 3, [r''' $2 '''], ' 1 '))
EOF
ifelse($#, 2, _MOZ_AC_DEFINE([$1], [$2]), $#, 3, _MOZ_AC_DEFINE([$1], [$2], [$3]),_MOZ_AC_DEFINE([$1]))dnl
])

dnl Wrap AC_DEFINE_UNQUOTED to store values in a format suitable for
dnl python.
define([_MOZ_AC_DEFINE_UNQUOTED], defn([AC_DEFINE_UNQUOTED]))
define([AC_DEFINE_UNQUOTED],
[cat >> confdefs.pytmp <<EOF
    (''' $1 ''', ifelse($#, 2, [r''' $2 '''], $#, 3, [r''' $2 '''], ' 1 '))
EOF
ifelse($#, 2, _MOZ_AC_DEFINE_UNQUOTED($1, $2), $#, 3, _MOZ_AC_DEFINE_UNQUOTED($1, $2, $3),_MOZ_AC_DEFINE_UNQUOTED($1))dnl
])

dnl Replace AC_OUTPUT to create and call a python config.status
define([MOZ_CREATE_CONFIG_STATUS],
[dnl Top source directory in Windows format (as opposed to msys format).
WIN_TOP_SRC=
case "$host_os" in
mingw*)
    WIN_TOP_SRC=`cd $srcdir; pwd -W`
    ;;
esac
AC_SUBST(WIN_TOP_SRC)

dnl Used in all Makefile.in files
top_srcdir=$srcdir
AC_SUBST(top_srcdir)

dnl Picked from autoconf 2.13
trap '' 1 2 15
AC_CACHE_SAVE

trap 'rm -f $CONFIG_STATUS conftest*; exit 1' 1 2 15
: ${CONFIG_STATUS=./config.data}

dnl We're going to need [ ] for python syntax.
changequote(<<<, >>>)dnl
echo creating $CONFIG_STATUS

cat > $CONFIG_STATUS <<EOF
def unique_list(l):
    result = []
    for i in l:
        if l not in result:
            result.append(i)
    return result

dnl All defines and substs are stored with an additional space at the beginning
dnl and at the end of the string, to avoid any problem with values starting or
dnl ending with quotes.
defines = [
EOF

dnl confdefs.pytmp contains AC_DEFINEs, in the expected format, but
dnl lacks the final comma (see above).
sed 's/$/,/' confdefs.pytmp >> $CONFIG_STATUS
rm confdefs.pytmp confdefs.h

cat >> $CONFIG_STATUS <<\EOF
]

substs = [
EOF

dnl The MOZ_DIVERSION_SUBST output diversion contains AC_SUBSTs, in the
dnl expected format, but lacks the final comma (see above).
sed 's/$/,/' >> $CONFIG_STATUS <<EOF
undivert(MOZ_DIVERSION_SUBST)dnl
EOF

dnl Add in the output from the subconfigure script
for ac_subst_arg in $_subconfigure_ac_subst_args; do
  variable='$'$ac_subst_arg
  echo "    (''' $ac_subst_arg ''', r''' `eval echo $variable` ''')," >> $CONFIG_STATUS
done

cat >> $CONFIG_STATUS <<\EOF
]

dnl List of AC_DEFINEs that aren't to be exposed in ALLDEFINES
non_global_defines = [
EOF

if test -n "$_NON_GLOBAL_ACDEFINES"; then
  for var in $_NON_GLOBAL_ACDEFINES; do
    echo "    '$var'," >> $CONFIG_STATUS
  done
fi

cat >> $CONFIG_STATUS <<EOF
]

flags = [
undivert(MOZ_DIVERSION_ARGS)dnl
]
EOF

changequote([, ])
])

define([m4_fatal],[
errprint([$1
])
m4exit(1)
])

define([AC_OUTPUT], [ifelse($#_$1, 1_, [MOZ_CREATE_CONFIG_STATUS()
MOZ_RUN_CONFIG_STATUS()],
[m4_fatal([Use CONFIGURE_SUBST_FILES in moz.build files to create substituted files.])]
)])

define([AC_CONFIG_HEADER],
[m4_fatal([Use CONFIGURE_DEFINE_FILES in moz.build files to produce header files.])
])
