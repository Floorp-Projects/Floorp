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
[ifdef([AC_SUBST_$1], ,
[define([AC_SUBST_$1], )dnl
AC_DIVERT_PUSH(MOZ_DIVERSION_SUBST)dnl
    (''' $1 ''', r''' [$]$1 ''')
AC_DIVERT_POP()dnl
])])

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
define([AC_OUTPUT],
[dnl Top source directory in Windows format (as opposed to msys format).
WIN_TOP_SRC=
encoding=utf-8
case "$host_os" in
mingw*)
    WIN_TOP_SRC=`cd $srcdir; pwd -W`
    encoding=mbcs
    ;;
esac
AC_SUBST(WIN_TOP_SRC)

dnl Used in all Makefile.in files
top_srcdir=$srcdir
AC_SUBST(top_srcdir)

dnl Picked from autoconf 2.13
trap '' 1 2 15
AC_CACHE_SAVE

test "x$prefix" = xNONE && prefix=$ac_default_prefix
# Let make expand exec_prefix.
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

trap 'rm -f $CONFIG_STATUS conftest*; exit 1' 1 2 15
: ${CONFIG_STATUS=./config.status}

dnl We're going to need [ ] for python syntax.
changequote(<<<, >>>)dnl
echo creating $CONFIG_STATUS

extra_python_path=${COMM_BUILD:+"'mozilla', "}

cat > $CONFIG_STATUS <<EOF
#!${PYTHON}
# coding=$encoding

import os
dnl topsrcdir is the top source directory in native form, as opposed to a
dnl form suitable for make.
topsrcdir = '''${WIN_TOP_SRC:-$srcdir}'''
if not os.path.isabs(topsrcdir):
    rel = os.path.join(os.path.dirname(<<<__file__>>>), topsrcdir)
    topsrcdir = os.path.normpath(os.path.abspath(rel))

topobjdir = os.path.abspath(os.path.dirname(<<<__file__>>>))

dnl All defines and substs are stored with an additional space at the beginning
dnl and at the end of the string, to avoid any problem with values starting or
dnl ending with quotes.
defines = [(name[1:-1], value[1:-1]) for name, value in [
EOF

dnl confdefs.pytmp contains AC_DEFINEs, in the expected format, but
dnl lacks the final comma (see above).
sed 's/$/,/' confdefs.pytmp >> $CONFIG_STATUS
rm confdefs.pytmp confdefs.h

cat >> $CONFIG_STATUS <<\EOF
] ]

substs = [(name[1:-1], value[1:-1]) for name, value in [
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
] ]

dnl List of files to apply AC_SUBSTs to. This is the list of files given
dnl as an argument to AC_OUTPUT ($1)
files = [
EOF

for out in $1; do
  echo "    '$out'," >> $CONFIG_STATUS
done

cat >> $CONFIG_STATUS <<\EOF
]

dnl List of header files to apply AC_DEFINEs to. This is stored in the
dnl AC_LIST_HEADER m4 macro by AC_CONFIG_HEADER.
headers = [
EOF

ifdef(<<<AC_LIST_HEADER>>>, <<<
HEADERS="AC_LIST_HEADER"
for header in $HEADERS; do
  echo "    '$header'," >> $CONFIG_STATUS
done
>>>)dnl

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

__all__ = ['topobjdir', 'topsrcdir', 'defines', 'non_global_defines', 'substs', 'files', 'headers']

dnl Do the actual work
if __name__ == '__main__':
    args = dict([(name, globals()[name]) for name in __all__])
    import sys
dnl Don't rely on virtualenv here. Standalone js doesn't use it.
    sys.path.append(os.path.join(topsrcdir, ${extra_python_path}'build'))
    from ConfigStatus import config_status
    config_status(**args)
EOF
changequote([, ])
chmod +x $CONFIG_STATUS
rm -fr confdefs* $ac_clean_files
dnl Execute config.status, unless --no-create was passed to configure.
if test "$no_create" != yes && ! ${PYTHON} $CONFIG_STATUS; then
    trap '' EXIT
    exit 1
fi
])
