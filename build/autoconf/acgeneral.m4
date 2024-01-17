dnl Parameterized macros.
dnl Requires GNU m4.
dnl This file is part of Autoconf.
dnl Copyright (C) 1992, 93, 94, 95, 96, 1998 Free Software Foundation, Inc.
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2, or (at your option)
dnl any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
dnl 02111-1307, USA.
dnl
dnl As a special exception, the Free Software Foundation gives unlimited
dnl permission to copy, distribute and modify the configure scripts that
dnl are the output of Autoconf.  You need not follow the terms of the GNU
dnl General Public License when using or distributing such scripts, even
dnl though portions of the text of Autoconf appear in them.  The GNU
dnl General Public License (GPL) does govern all other use of the material
dnl that constitutes the Autoconf program.
dnl
dnl Certain portions of the Autoconf source text are designed to be copied
dnl (in certain cases, depending on the input) into the output of
dnl Autoconf.  We call these the "data" portions.  The rest of the Autoconf
dnl source text consists of comments plus executable code that decides which
dnl of the data portions to output in any given case.  We call these
dnl comments and executable code the "non-data" portions.  Autoconf never
dnl copies any of the non-data portions into its output.
dnl
dnl This special exception to the GPL applies to versions of Autoconf
dnl released by the Free Software Foundation.  When you make and
dnl distribute a modified version of Autoconf, you may extend this special
dnl exception to the GPL to apply to your modified version as well, *unless*
dnl your modified version has the potential to copy into its output some
dnl of the text that was the non-data portion of the version that you started
dnl with.  (In other words, unless your change moves or copies text from
dnl the non-data portions to the data portions.)  If your modification has
dnl such potential, you must delete any notice of this special exception
dnl to the GPL from your modified version.
dnl
dnl Written by David MacKenzie, with help from
dnl Franc,ois Pinard, Karl Berry, Richard Pixley, Ian Lance Taylor,
dnl Roland McGrath, Noah Friedman, david d zuhn, and many others.
dnl
divert(-1)dnl Throw away output until AC_INIT is called.
changequote([, ])

define(AC_ACVERSION, 2.13)

dnl Some old m4's don't support m4exit.  But they provide
dnl equivalent functionality by core dumping because of the
dnl long macros we define.
ifdef([__gnu__], , [errprint(Autoconf requires GNU m4.
Install it before installing Autoconf or set the
M4 environment variable to its path name.
)m4exit(2)])

undefine([eval])
undefine([include])
undefine([shift])
undefine([format])


dnl ### Defining macros


dnl m4 output diversions.  We let m4 output them all in order at the end,
dnl except that we explicitly undivert AC_DIVERSION_SED, AC_DIVERSION_CMDS,
dnl and AC_DIVERSION_ICMDS.

dnl AC_DIVERSION_NOTICE - 1 (= 0)	AC_REQUIRE'd #! /bin/sh line
define(AC_DIVERSION_NOTICE, 1)dnl	copyright notice & option help strings
define(AC_DIVERSION_INIT, 2)dnl		initialization code
define(AC_DIVERSION_NORMAL_4, 3)dnl	AC_REQUIRE'd code, 4 level deep
define(AC_DIVERSION_NORMAL_3, 4)dnl	AC_REQUIRE'd code, 3 level deep
define(AC_DIVERSION_NORMAL_2, 5)dnl	AC_REQUIRE'd code, 2 level deep
define(AC_DIVERSION_NORMAL_1, 6)dnl	AC_REQUIRE'd code, 1 level deep
define(AC_DIVERSION_NORMAL, 7)dnl	the tests and output code
define(AC_DIVERSION_SED, 8)dnl		variable substitutions in config.status
define(AC_DIVERSION_CMDS, 9)dnl		extra shell commands in config.status
define(AC_DIVERSION_ICMDS, 10)dnl	extra initialization in config.status

dnl Change the diversion stream to STREAM, while stacking old values.
dnl AC_DIVERT_PUSH(STREAM)
define(AC_DIVERT_PUSH,
[pushdef([AC_DIVERSION_CURRENT], $1)dnl
divert(AC_DIVERSION_CURRENT)dnl
])

dnl Change the diversion stream to its previous value, unstacking it.
dnl AC_DIVERT_POP()
define(AC_DIVERT_POP,
[popdef([AC_DIVERSION_CURRENT])dnl
divert(AC_DIVERSION_CURRENT)dnl
])

dnl Initialize the diversion setup.
define([AC_DIVERSION_CURRENT], AC_DIVERSION_NORMAL)
dnl This will be popped by AC_REQUIRE in AC_INIT.
pushdef([AC_DIVERSION_CURRENT], AC_DIVERSION_NOTICE)

dnl The prologue for Autoconf macros.
dnl AC_PRO(MACRO-NAME)
define(AC_PRO,
[define([AC_PROVIDE_$1], )dnl
ifelse(AC_DIVERSION_CURRENT, AC_DIVERSION_NORMAL,
[AC_DIVERT_PUSH(builtin(eval, AC_DIVERSION_CURRENT - 1))],
[pushdef([AC_DIVERSION_CURRENT], AC_DIVERSION_CURRENT)])dnl
])

dnl The Epilogue for Autoconf macros.
dnl AC_EPI()
define(AC_EPI,
[AC_DIVERT_POP()dnl
ifelse(AC_DIVERSION_CURRENT, AC_DIVERSION_NORMAL,
[undivert(AC_DIVERSION_NORMAL_4)dnl
undivert(AC_DIVERSION_NORMAL_3)dnl
undivert(AC_DIVERSION_NORMAL_2)dnl
undivert(AC_DIVERSION_NORMAL_1)dnl
])dnl
])

dnl Define a macro which automatically provides itself.  Add machinery
dnl so the macro automatically switches expansion to the diversion
dnl stack if it is not already using it.  In this case, once finished,
dnl it will bring back all the code accumulated in the diversion stack.
dnl This, combined with AC_REQUIRE, achieves the topological ordering of
dnl macros.  We don't use this macro to define some frequently called
dnl macros that are not involved in ordering constraints, to save m4
dnl processing.
dnl AC_DEFUN(NAME, EXPANSION)
define([AC_DEFUN],
[define($1, [AC_PRO([$1])$2[]AC_EPI()])])


dnl ### Initialization


dnl AC_INIT_NOTICE()
AC_DEFUN(AC_INIT_NOTICE,
[# Guess values for system-dependent variables and create Makefiles.
# Generated automatically using autoconf version] AC_ACVERSION [
# Copyright (C) 1992, 93, 94, 95, 96 Free Software Foundation, Inc.
#
# This configure script is free software; the Free Software Foundation
# gives unlimited permission to copy, distribute and modify it.

# Defaults:
ac_help=
ac_default_prefix=/usr/local
[#] Any additions from configure.in:])

dnl AC_PREFIX_DEFAULT(PREFIX)
AC_DEFUN(AC_PREFIX_DEFAULT,
[AC_DIVERT_PUSH(AC_DIVERSION_NOTICE)dnl
ac_default_prefix=$1
AC_DIVERT_POP()])

dnl AC_INIT_PARSE_ARGS()
AC_DEFUN(AC_INIT_PARSE_ARGS,
[
# Initialize some variables set by options.
# The variables have the same names as the options, with
# dashes changed to underlines.
build=NONE
cache_file=./config.cache
exec_prefix=NONE
host=NONE
no_create=
nonopt=NONE
no_recursion=
prefix=NONE
program_prefix=NONE
program_suffix=NONE
program_transform_name=s,x,x,
silent=
site=
srcdir=
target=NONE
verbose=
x_includes=NONE
x_libraries=NONE
dnl Installation directory options.
dnl These are left unexpanded so users can "make install exec_prefix=/foo"
dnl and all the variables that are supposed to be based on exec_prefix
dnl by default will actually change.
dnl Use braces instead of parens because sh, perl, etc. also accept them.
bindir='${exec_prefix}/bin'
sbindir='${exec_prefix}/sbin'
libexecdir='${exec_prefix}/libexec'
datadir='${prefix}/share'
sysconfdir='${prefix}/etc'
sharedstatedir='${prefix}/com'
localstatedir='${prefix}/var'
libdir='${exec_prefix}/lib'
includedir='${prefix}/include'
oldincludedir='/usr/include'
infodir='${prefix}/info'
mandir='${prefix}/man'

# Initialize some other variables.
subdirs=
MFLAGS= MAKEFLAGS=
SHELL=${CONFIG_SHELL-/bin/sh}
# Maximum number of lines to put in a shell here document.
ac_max_here_lines=12

ac_prev=
for ac_option
do

  # If the previous option needs an argument, assign it.
  if test -n "$ac_prev"; then
    eval "$ac_prev=\$ac_option"
    ac_prev=
    continue
  fi

  case "$ac_option" in
changequote(, )dnl
  -*=*) ac_optarg=`echo "$ac_option" | sed 's/[-_a-zA-Z0-9]*=//'` ;;
changequote([, ])dnl
  *) ac_optarg= ;;
  esac

  # Accept the important Cygnus configure options, so we can diagnose typos.

  case "$ac_option" in

  -bindir | --bindir | --bindi | --bind | --bin | --bi)
    ac_prev=bindir ;;
  -bindir=* | --bindir=* | --bindi=* | --bind=* | --bin=* | --bi=*)
    bindir="$ac_optarg" ;;

  -build | --build | --buil | --bui | --bu)
    ac_prev=build ;;
  -build=* | --build=* | --buil=* | --bui=* | --bu=*)
    build="$ac_optarg" ;;

  -cache-file | --cache-file | --cache-fil | --cache-fi \
  | --cache-f | --cache- | --cache | --cach | --cac | --ca | --c)
    ac_prev=cache_file ;;
  -cache-file=* | --cache-file=* | --cache-fil=* | --cache-fi=* \
  | --cache-f=* | --cache-=* | --cache=* | --cach=* | --cac=* | --ca=* | --c=*)
    cache_file="$ac_optarg" ;;

  -datadir | --datadir | --datadi | --datad | --data | --dat | --da)
    ac_prev=datadir ;;
  -datadir=* | --datadir=* | --datadi=* | --datad=* | --data=* | --dat=* \
  | --da=*)
    datadir="$ac_optarg" ;;

  -disable-* | --disable-*)
    ac_feature=`echo $ac_option|sed -e 's/-*disable-//'`
    # Reject names that are not valid shell variable names.
changequote(, )dnl
    if test -n "`echo $ac_feature| sed 's/[-a-zA-Z0-9_]//g'`"; then
changequote([, ])dnl
      AC_MSG_ERROR($ac_feature: invalid feature name)
    fi
    ac_feature=`echo $ac_feature| sed 's/-/_/g'`
    eval "enable_${ac_feature}=no" ;;

  -enable-* | --enable-*)
    ac_feature=`echo $ac_option|sed -e 's/-*enable-//' -e 's/=.*//'`
    # Reject names that are not valid shell variable names.
changequote(, )dnl
    if test -n "`echo $ac_feature| sed 's/[-_a-zA-Z0-9]//g'`"; then
changequote([, ])dnl
      AC_MSG_ERROR($ac_feature: invalid feature name)
    fi
    ac_feature=`echo $ac_feature| sed 's/-/_/g'`
    case "$ac_option" in
      *=*) ;;
      *) ac_optarg=yes ;;
    esac
    eval "enable_${ac_feature}='$ac_optarg'" ;;

  -exec-prefix | --exec_prefix | --exec-prefix | --exec-prefi \
  | --exec-pref | --exec-pre | --exec-pr | --exec-p | --exec- \
  | --exec | --exe | --ex)
    ac_prev=exec_prefix ;;
  -exec-prefix=* | --exec_prefix=* | --exec-prefix=* | --exec-prefi=* \
  | --exec-pref=* | --exec-pre=* | --exec-pr=* | --exec-p=* | --exec-=* \
  | --exec=* | --exe=* | --ex=*)
    exec_prefix="$ac_optarg" ;;

  -gas | --gas | --ga | --g)
    # Obsolete; use --with-gas.
    with_gas=yes ;;

  -help | --help | --hel | --he)
    # Omit some internal or obsolete options to make the list less imposing.
    # This message is too long to be a string in the A/UX 3.1 sh.
    cat << EOF
changequote(, )dnl
Usage: configure [options] [host]
Options: [defaults in brackets after descriptions]
Configuration:
  --cache-file=FILE       cache test results in FILE
  --help                  print this message
  --no-create             do not create output files
  --quiet, --silent       do not print \`checking...' messages
  --version               print the version of autoconf that created configure
Directory and file names:
  --prefix=PREFIX         install architecture-independent files in PREFIX
                          [$ac_default_prefix]
  --exec-prefix=EPREFIX   install architecture-dependent files in EPREFIX
                          [same as prefix]
  --bindir=DIR            user executables in DIR [EPREFIX/bin]
  --sbindir=DIR           system admin executables in DIR [EPREFIX/sbin]
  --libexecdir=DIR        program executables in DIR [EPREFIX/libexec]
  --datadir=DIR           read-only architecture-independent data in DIR
                          [PREFIX/share]
  --sysconfdir=DIR        read-only single-machine data in DIR [PREFIX/etc]
  --sharedstatedir=DIR    modifiable architecture-independent data in DIR
                          [PREFIX/com]
  --localstatedir=DIR     modifiable single-machine data in DIR [PREFIX/var]
  --libdir=DIR            object code libraries in DIR [EPREFIX/lib]
  --includedir=DIR        C header files in DIR [PREFIX/include]
  --oldincludedir=DIR     C header files for non-gcc in DIR [/usr/include]
  --infodir=DIR           info documentation in DIR [PREFIX/info]
  --mandir=DIR            man documentation in DIR [PREFIX/man]
  --srcdir=DIR            find the sources in DIR [configure dir or ..]
  --program-prefix=PREFIX prepend PREFIX to installed program names
  --program-suffix=SUFFIX append SUFFIX to installed program names
  --program-transform-name=PROGRAM
                          run sed PROGRAM on installed program names
EOF
    cat << EOF
Host type:
  --build=BUILD           configure for building on BUILD [BUILD=HOST]
  --host=HOST             configure for HOST [guessed]
  --target=TARGET         configure for TARGET [TARGET=HOST]
Features and packages:
  --disable-FEATURE       do not include FEATURE (same as --enable-FEATURE=no)
  --enable-FEATURE[=ARG]  include FEATURE [ARG=yes]
  --with-PACKAGE[=ARG]    use PACKAGE [ARG=yes]
  --without-PACKAGE       do not use PACKAGE (same as --with-PACKAGE=no)
  --x-includes=DIR        X include files are in DIR
  --x-libraries=DIR       X library files are in DIR
changequote([, ])dnl
EOF
    if test -n "$ac_help"; then
      echo "--enable and --with options recognized:$ac_help"
    fi
    exit 0 ;;

  -host | --host | --hos | --ho)
    ac_prev=host ;;
  -host=* | --host=* | --hos=* | --ho=*)
    host="$ac_optarg" ;;

  -includedir | --includedir | --includedi | --included | --include \
  | --includ | --inclu | --incl | --inc)
    ac_prev=includedir ;;
  -includedir=* | --includedir=* | --includedi=* | --included=* | --include=* \
  | --includ=* | --inclu=* | --incl=* | --inc=*)
    includedir="$ac_optarg" ;;

  -infodir | --infodir | --infodi | --infod | --info | --inf)
    ac_prev=infodir ;;
  -infodir=* | --infodir=* | --infodi=* | --infod=* | --info=* | --inf=*)
    infodir="$ac_optarg" ;;

  -libdir | --libdir | --libdi | --libd)
    ac_prev=libdir ;;
  -libdir=* | --libdir=* | --libdi=* | --libd=*)
    libdir="$ac_optarg" ;;

  -libexecdir | --libexecdir | --libexecdi | --libexecd | --libexec \
  | --libexe | --libex | --libe)
    ac_prev=libexecdir ;;
  -libexecdir=* | --libexecdir=* | --libexecdi=* | --libexecd=* | --libexec=* \
  | --libexe=* | --libex=* | --libe=*)
    libexecdir="$ac_optarg" ;;

  -localstatedir | --localstatedir | --localstatedi | --localstated \
  | --localstate | --localstat | --localsta | --localst \
  | --locals | --local | --loca | --loc | --lo)
    ac_prev=localstatedir ;;
  -localstatedir=* | --localstatedir=* | --localstatedi=* | --localstated=* \
  | --localstate=* | --localstat=* | --localsta=* | --localst=* \
  | --locals=* | --local=* | --loca=* | --loc=* | --lo=*)
    localstatedir="$ac_optarg" ;;

  -mandir | --mandir | --mandi | --mand | --man | --ma | --m)
    ac_prev=mandir ;;
  -mandir=* | --mandir=* | --mandi=* | --mand=* | --man=* | --ma=* | --m=*)
    mandir="$ac_optarg" ;;

  -nfp | --nfp | --nf)
    # Obsolete; use --without-fp.
    with_fp=no ;;

  -no-create | --no-create | --no-creat | --no-crea | --no-cre \
  | --no-cr | --no-c)
    no_create=yes ;;

  -no-recursion | --no-recursion | --no-recursio | --no-recursi \
  | --no-recurs | --no-recur | --no-recu | --no-rec | --no-re | --no-r)
    no_recursion=yes ;;

  -oldincludedir | --oldincludedir | --oldincludedi | --oldincluded \
  | --oldinclude | --oldinclud | --oldinclu | --oldincl | --oldinc \
  | --oldin | --oldi | --old | --ol | --o)
    ac_prev=oldincludedir ;;
  -oldincludedir=* | --oldincludedir=* | --oldincludedi=* | --oldincluded=* \
  | --oldinclude=* | --oldinclud=* | --oldinclu=* | --oldincl=* | --oldinc=* \
  | --oldin=* | --oldi=* | --old=* | --ol=* | --o=*)
    oldincludedir="$ac_optarg" ;;

  -prefix | --prefix | --prefi | --pref | --pre | --pr | --p)
    ac_prev=prefix ;;
  -prefix=* | --prefix=* | --prefi=* | --pref=* | --pre=* | --pr=* | --p=*)
    prefix="$ac_optarg" ;;

  -program-prefix | --program-prefix | --program-prefi | --program-pref \
  | --program-pre | --program-pr | --program-p)
    ac_prev=program_prefix ;;
  -program-prefix=* | --program-prefix=* | --program-prefi=* \
  | --program-pref=* | --program-pre=* | --program-pr=* | --program-p=*)
    program_prefix="$ac_optarg" ;;

  -program-suffix | --program-suffix | --program-suffi | --program-suff \
  | --program-suf | --program-su | --program-s)
    ac_prev=program_suffix ;;
  -program-suffix=* | --program-suffix=* | --program-suffi=* \
  | --program-suff=* | --program-suf=* | --program-su=* | --program-s=*)
    program_suffix="$ac_optarg" ;;

  -program-transform-name | --program-transform-name \
  | --program-transform-nam | --program-transform-na \
  | --program-transform-n | --program-transform- \
  | --program-transform | --program-transfor \
  | --program-transfo | --program-transf \
  | --program-trans | --program-tran \
  | --progr-tra | --program-tr | --program-t)
    ac_prev=program_transform_name ;;
  -program-transform-name=* | --program-transform-name=* \
  | --program-transform-nam=* | --program-transform-na=* \
  | --program-transform-n=* | --program-transform-=* \
  | --program-transform=* | --program-transfor=* \
  | --program-transfo=* | --program-transf=* \
  | --program-trans=* | --program-tran=* \
  | --progr-tra=* | --program-tr=* | --program-t=*)
    program_transform_name="$ac_optarg" ;;

  -q | -quiet | --quiet | --quie | --qui | --qu | --q \
  | -silent | --silent | --silen | --sile | --sil)
    silent=yes ;;

  -sbindir | --sbindir | --sbindi | --sbind | --sbin | --sbi | --sb)
    ac_prev=sbindir ;;
  -sbindir=* | --sbindir=* | --sbindi=* | --sbind=* | --sbin=* \
  | --sbi=* | --sb=*)
    sbindir="$ac_optarg" ;;

  -sharedstatedir | --sharedstatedir | --sharedstatedi \
  | --sharedstated | --sharedstate | --sharedstat | --sharedsta \
  | --sharedst | --shareds | --shared | --share | --shar \
  | --sha | --sh)
    ac_prev=sharedstatedir ;;
  -sharedstatedir=* | --sharedstatedir=* | --sharedstatedi=* \
  | --sharedstated=* | --sharedstate=* | --sharedstat=* | --sharedsta=* \
  | --sharedst=* | --shareds=* | --shared=* | --share=* | --shar=* \
  | --sha=* | --sh=*)
    sharedstatedir="$ac_optarg" ;;

  -site | --site | --sit)
    ac_prev=site ;;
  -site=* | --site=* | --sit=*)
    site="$ac_optarg" ;;

  -srcdir | --srcdir | --srcdi | --srcd | --src | --sr)
    ac_prev=srcdir ;;
  -srcdir=* | --srcdir=* | --srcdi=* | --srcd=* | --src=* | --sr=*)
    srcdir="$ac_optarg" ;;

  -sysconfdir | --sysconfdir | --sysconfdi | --sysconfd | --sysconf \
  | --syscon | --sysco | --sysc | --sys | --sy)
    ac_prev=sysconfdir ;;
  -sysconfdir=* | --sysconfdir=* | --sysconfdi=* | --sysconfd=* | --sysconf=* \
  | --syscon=* | --sysco=* | --sysc=* | --sys=* | --sy=*)
    sysconfdir="$ac_optarg" ;;

  -target | --target | --targe | --targ | --tar | --ta | --t)
    ac_prev=target ;;
  -target=* | --target=* | --targe=* | --targ=* | --tar=* | --ta=* | --t=*)
    target="$ac_optarg" ;;

  -v | -verbose | --verbose | --verbos | --verbo | --verb)
    verbose=yes ;;

  -version | --version | --versio | --versi | --vers)
    echo "configure generated by autoconf version AC_ACVERSION"
    exit 0 ;;

  -with-* | --with-*)
    ac_package=`echo $ac_option|sed -e 's/-*with-//' -e 's/=.*//'`
    # Reject names that are not valid shell variable names.
changequote(, )dnl
    if test -n "`echo $ac_package| sed 's/[-_a-zA-Z0-9]//g'`"; then
changequote([, ])dnl
      AC_MSG_ERROR($ac_package: invalid package name)
    fi
    ac_package=`echo $ac_package| sed 's/-/_/g'`
    case "$ac_option" in
      *=*) ;;
      *) ac_optarg=yes ;;
    esac
    eval "with_${ac_package}='$ac_optarg'" ;;

  -without-* | --without-*)
    ac_package=`echo $ac_option|sed -e 's/-*without-//'`
    # Reject names that are not valid shell variable names.
changequote(, )dnl
    if test -n "`echo $ac_package| sed 's/[-a-zA-Z0-9_]//g'`"; then
changequote([, ])dnl
      AC_MSG_ERROR($ac_package: invalid package name)
    fi
    ac_package=`echo $ac_package| sed 's/-/_/g'`
    eval "with_${ac_package}=no" ;;

  --x)
    # Obsolete; use --with-x.
    with_x=yes ;;

  -x-includes | --x-includes | --x-include | --x-includ | --x-inclu \
  | --x-incl | --x-inc | --x-in | --x-i)
    ac_prev=x_includes ;;
  -x-includes=* | --x-includes=* | --x-include=* | --x-includ=* | --x-inclu=* \
  | --x-incl=* | --x-inc=* | --x-in=* | --x-i=*)
    x_includes="$ac_optarg" ;;

  -x-libraries | --x-libraries | --x-librarie | --x-librari \
  | --x-librar | --x-libra | --x-libr | --x-lib | --x-li | --x-l)
    ac_prev=x_libraries ;;
  -x-libraries=* | --x-libraries=* | --x-librarie=* | --x-librari=* \
  | --x-librar=* | --x-libra=* | --x-libr=* | --x-lib=* | --x-li=* | --x-l=*)
    x_libraries="$ac_optarg" ;;

  -*) AC_MSG_ERROR([$ac_option: invalid option; use --help to show usage])
    ;;

  *)
changequote(, )dnl
    if test -n "`echo $ac_option| sed 's/[-a-z0-9.]//g'`"; then
changequote([, ])dnl
      AC_MSG_WARN($ac_option: invalid host type)
    fi
    if test "x$nonopt" != xNONE; then
      AC_MSG_ERROR(can only configure for one host and one target at a time)
    fi
    nonopt="$ac_option"
    ;;

  esac
done

if test -n "$ac_prev"; then
  AC_MSG_ERROR(missing argument to --`echo $ac_prev | sed 's/_/-/g'`)
fi
])

dnl Try to have only one #! line, so the script doesn't look funny
dnl for users of AC_REVISION.
dnl AC_INIT_BINSH()
AC_DEFUN(AC_INIT_BINSH,
[#! /bin/sh
])

dnl AC_INIT(UNIQUE-FILE-IN-SOURCE-DIR)
AC_DEFUN(AC_INIT,
[sinclude(acsite.m4)dnl
sinclude(./aclocal.m4)dnl
AC_REQUIRE([AC_INIT_BINSH])dnl
AC_INIT_NOTICE
AC_DIVERT_POP()dnl to NORMAL
AC_DIVERT_PUSH(AC_DIVERSION_INIT)dnl
AC_INIT_PARSE_ARGS
AC_INIT_PREPARE($1)dnl
AC_DIVERT_POP()dnl to NORMAL
])

dnl AC_INIT_PREPARE(UNIQUE-FILE-IN-SOURCE-DIR)
AC_DEFUN(AC_INIT_PREPARE,
[trap 'rm -fr conftest* confdefs* core core.* *.core $ac_clean_files; exit 1' 1 2 15

# File descriptor usage:
# 0 standard input
# 1 file creation
# 2 errors and warnings
# 3 some systems may open it to /dev/tty
# 4 used on the Kubota Titan
define(AC_FD_MSG, 6)dnl
[#] AC_FD_MSG checking for... messages and results
define(AC_FD_CC, 5)dnl
[#] AC_FD_CC compiler messages saved in config.log
if test "$silent" = yes; then
  exec AC_FD_MSG>/dev/null
else
  exec AC_FD_MSG>&1
fi
exec AC_FD_CC>./config.log

echo "\
This file contains any messages produced by compilers while
running configure, to aid debugging if configure makes a mistake.
" 1>&AC_FD_CC

# Strip out --no-create and --no-recursion so they do not pile up.
# Also quote any args containing shell metacharacters.
ac_configure_args=
for ac_arg
do
  case "$ac_arg" in
  -no-create | --no-create | --no-creat | --no-crea | --no-cre \
  | --no-cr | --no-c) ;;
  -no-recursion | --no-recursion | --no-recursio | --no-recursi \
  | --no-recurs | --no-recur | --no-recu | --no-rec | --no-re | --no-r) ;;
changequote(<<, >>)dnl
dnl If you change this globbing pattern, test it on an old shell --
dnl it's sensitive.  Putting any kind of quote in it causes syntax errors.
  *" "*|*"	"*|*[\[\]\~\<<#>>\$\^\&\*\(\)\{\}\\\|\;\<\>\?]*)
  ac_configure_args="$ac_configure_args '$ac_arg'" ;;
changequote([, ])dnl
  *) ac_configure_args="$ac_configure_args $ac_arg" ;;
  esac
done

# NLS nuisances.
# Only set these to C if already set.  These must not be set unconditionally
# because not all systems understand e.g. LANG=C (notably SCO).
# Fixing LC_MESSAGES prevents Solaris sh from translating var values in `set'!
# Non-C LC_CTYPE values break the ctype check.
if test "${LANG+set}"   = set; then LANG=C;   export LANG;   fi
if test "${LC_ALL+set}" = set; then LC_ALL=C; export LC_ALL; fi
if test "${LC_MESSAGES+set}" = set; then LC_MESSAGES=C; export LC_MESSAGES; fi
if test "${LC_CTYPE+set}"    = set; then LC_CTYPE=C;    export LC_CTYPE;    fi

# confdefs.h avoids OS command line length limits that DEFS can exceed.
rm -rf conftest* confdefs.h
# AIX cpp loses on an empty file, so make sure it contains at least a newline.
echo > confdefs.h

# A filename unique to this package, relative to the directory that
# configure is in, which we can look for to find out if srcdir is correct.
ac_unique_file=$1

# Find the source files, if location was not specified.
if test -z "$srcdir"; then
  ac_srcdir_defaulted=yes
  # Try the directory containing this script, then its parent.
  ac_prog=[$]0
changequote(, )dnl
  ac_confdir=`echo $ac_prog|sed 's%/[^/][^/]*$%%'`
changequote([, ])dnl
  test "x$ac_confdir" = "x$ac_prog" && ac_confdir=.
  srcdir=$ac_confdir
  if test ! -r $srcdir/$ac_unique_file; then
    srcdir=..
  fi
else
  ac_srcdir_defaulted=no
fi
if test ! -r $srcdir/$ac_unique_file; then
  if test "$ac_srcdir_defaulted" = yes; then
    AC_MSG_ERROR(can not find sources in $ac_confdir or ..)
  else
    AC_MSG_ERROR(can not find sources in $srcdir)
  fi
fi
dnl Double slashes in pathnames in object file debugging info
dnl mess up M-x gdb in Emacs.
changequote(, )dnl
srcdir=`echo "${srcdir}" | sed 's%\([^/]\)/*$%\1%'`
changequote([, ])dnl

dnl Let the site file select an alternate cache file if it wants to.
AC_SITE_LOAD
AC_CACHE_LOAD
AC_LANG_C
dnl By default always use an empty string as the executable
dnl extension.  Only change it if the script calls AC_EXEEXT.
ac_exeext=
dnl By default assume that objects files use an extension of .o.  Only
dnl change it if the script calls AC_OBJEXT.
ac_objext=o
AC_PROG_ECHO_N
dnl Substitute for predefined variables.
AC_SUBST(SHELL)dnl
AC_SUBST(CFLAGS)dnl
AC_SUBST(CPPFLAGS)dnl
AC_SUBST(CXXFLAGS)dnl
AC_SUBST(FFLAGS)dnl
AC_SUBST(DEFS)dnl
AC_SUBST(LDFLAGS)dnl
AC_SUBST(LIBS)dnl
AC_SUBST(exec_prefix)dnl
AC_SUBST(prefix)dnl
AC_SUBST(program_transform_name)dnl
dnl Installation directory options.
AC_SUBST(bindir)dnl
AC_SUBST(sbindir)dnl
AC_SUBST(libexecdir)dnl
AC_SUBST(datadir)dnl
AC_SUBST(sysconfdir)dnl
AC_SUBST(sharedstatedir)dnl
AC_SUBST(localstatedir)dnl
AC_SUBST(libdir)dnl
AC_SUBST(includedir)dnl
AC_SUBST(oldincludedir)dnl
AC_SUBST(infodir)dnl
AC_SUBST(mandir)dnl
])


dnl ### Selecting optional features


dnl AC_ARG_ENABLE(FEATURE, HELP-STRING, ACTION-IF-TRUE [, ACTION-IF-FALSE])
AC_DEFUN(AC_ARG_ENABLE,
[AC_DIVERT_PUSH(AC_DIVERSION_NOTICE)dnl
ac_help="$ac_help
[$2]"
AC_DIVERT_POP()dnl
[#] Check whether --enable-[$1] or --disable-[$1] was given.
if test "[${enable_]patsubst([$1], -, _)+set}" = set; then
  enableval="[$enable_]patsubst([$1], -, _)"
  ifelse([$3], , :, [$3])
ifelse([$4], , , [else
  $4
])dnl
fi
])

AC_DEFUN(AC_ENABLE,
[AC_OBSOLETE([$0], [; instead use AC_ARG_ENABLE])dnl
AC_ARG_ENABLE([$1], [  --enable-$1], [$2], [$3])dnl
])


dnl ### Working with optional software


dnl AC_ARG_WITH(PACKAGE, HELP-STRING, ACTION-IF-TRUE [, ACTION-IF-FALSE])
AC_DEFUN(AC_ARG_WITH,
[AC_DIVERT_PUSH(AC_DIVERSION_NOTICE)dnl
ac_help="$ac_help
[$2]"
AC_DIVERT_POP()dnl
[#] Check whether --with-[$1] or --without-[$1] was given.
if test "[${with_]patsubst([$1], -, _)+set}" = set; then
  withval="[$with_]patsubst([$1], -, _)"
  ifelse([$3], , :, [$3])
ifelse([$4], , , [else
  $4
])dnl
fi
])

AC_DEFUN(AC_WITH,
[AC_OBSOLETE([$0], [; instead use AC_ARG_WITH])dnl
AC_ARG_WITH([$1], [  --with-$1], [$2], [$3])dnl
])


dnl ### Transforming program names.


dnl AC_ARG_PROGRAM()
AC_DEFUN(AC_ARG_PROGRAM,
[if test "$program_transform_name" = s,x,x,; then
  program_transform_name=
else
  # Double any \ or $.  echo might interpret backslashes.
  cat <<\EOF_SED > conftestsed
s,\\,\\\\,g; s,\$,$$,g
EOF_SED
  program_transform_name="`echo $program_transform_name|sed -f conftestsed`"
  rm -f conftestsed
fi
test "$program_prefix" != NONE &&
  program_transform_name="s,^,${program_prefix},; $program_transform_name"
# Use a double $ so make ignores it.
test "$program_suffix" != NONE &&
  program_transform_name="s,\$\$,${program_suffix},; $program_transform_name"

# sed with no file args requires a program.
test "$program_transform_name" = "" && program_transform_name="s,x,x,"
])


dnl ### Version numbers


dnl AC_REVISION(REVISION-INFO)
AC_DEFUN(AC_REVISION,
[AC_REQUIRE([AC_INIT_BINSH])dnl
[# From configure.in] translit([$1], $")])

dnl Subroutines of AC_PREREQ.

dnl Change the dots in NUMBER into commas.
dnl AC_PREREQ_SPLIT(NUMBER)
define(AC_PREREQ_SPLIT,
[translit($1, ., [, ])])

dnl Default the ternary version number to 0 (e.g., 1, 7 -> 1, 7, 0).
dnl AC_PREREQ_CANON(MAJOR, MINOR [,TERNARY])
define(AC_PREREQ_CANON,
[$1, $2, ifelse([$3], , 0, [$3])])

dnl Complain and exit if version number 1 is less than version number 2.
dnl PRINTABLE2 is the printable version of version number 2.
dnl AC_PREREQ_COMPARE(MAJOR1, MINOR1, TERNARY1, MAJOR2, MINOR2, TERNARY2,
dnl                   PRINTABLE2)
define(AC_PREREQ_COMPARE,
[ifelse(builtin([eval],
[$3 + $2 * 1000 + $1 * 1000000 < $6 + $5 * 1000 + $4 * 1000000]), 1,
[errprint(dnl
FATAL ERROR: Autoconf version $7 or higher is required for this script
)m4exit(3)])])

dnl Complain and exit if the Autoconf version is less than VERSION.
dnl AC_PREREQ(VERSION)
define(AC_PREREQ,
[AC_PREREQ_COMPARE(AC_PREREQ_CANON(AC_PREREQ_SPLIT(AC_ACVERSION)),
AC_PREREQ_CANON(AC_PREREQ_SPLIT([$1])), [$1])])


dnl ### Getting the canonical system type


dnl Find install-sh, config.sub, config.guess, and Cygnus configure
dnl in directory DIR.  These are auxiliary files used in configuration.
dnl DIR can be either absolute or relative to $srcdir.
dnl AC_CONFIG_AUX_DIR(DIR)
AC_DEFUN(AC_CONFIG_AUX_DIR,
[AC_CONFIG_AUX_DIRS($1 $srcdir/$1)])

dnl The default is `$srcdir' or `$srcdir/..' or `$srcdir/../..'.
dnl There's no need to call this macro explicitly; just AC_REQUIRE it.
AC_DEFUN(AC_CONFIG_AUX_DIR_DEFAULT,
[AC_CONFIG_AUX_DIRS($srcdir $srcdir/.. $srcdir/../..)])

dnl Internal subroutine.
dnl Search for the configuration auxiliary files in directory list $1.
dnl We look only for install-sh, so users of AC_PROG_INSTALL
dnl do not automatically need to distribute the other auxiliary files.
dnl AC_CONFIG_AUX_DIRS(DIR ...)
AC_DEFUN(AC_CONFIG_AUX_DIRS,
[ac_aux_dir=
for ac_dir in $1; do
  if test -f $ac_dir/install-sh; then
    ac_aux_dir=$ac_dir
    ac_install_sh="$ac_aux_dir/install-sh -c"
    break
  elif test -f $ac_dir/install.sh; then
    ac_aux_dir=$ac_dir
    ac_install_sh="$ac_aux_dir/install.sh -c"
    break
  fi
done
if test -z "$ac_aux_dir"; then
  AC_MSG_ERROR([can not find install-sh or install.sh in $1])
fi
ac_config_guess=$ac_aux_dir/config.guess
ac_config_sub=$ac_aux_dir/config.sub
ac_configure=$ac_aux_dir/configure # This should be Cygnus configure.
AC_PROVIDE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
])

dnl Canonicalize the host, target, and build system types.
AC_DEFUN(AC_CANONICAL_SYSTEM,
[AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
AC_BEFORE([$0], [AC_ARG_PROGRAM])
# Do some error checking and defaulting for the host and target type.
# The inputs are:
#    configure --host=HOST --target=TARGET --build=BUILD NONOPT
#
# The rules are:
# 1. You are not allowed to specify --host, --target, and nonopt at the
#    same time.
# 2. Host defaults to nonopt.
# 3. If nonopt is not specified, then host defaults to the current host,
#    as determined by config.guess.
# 4. Target and build default to nonopt.
# 5. If nonopt is not specified, then target and build default to host.

# The aliases save the names the user supplied, while $host etc.
# will get canonicalized.
case $host---$target---$nonopt in
NONE---*---* | *---NONE---* | *---*---NONE) ;;
*) AC_MSG_ERROR(can only configure for one host and one target at a time) ;;
esac

AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AC_CANONICAL_BUILD
test "$host_alias" != "$target_alias" &&
  test "$program_prefix$program_suffix$program_transform_name" = \
    NONENONEs,x,x, &&
  program_prefix=${target_alias}-
])

dnl Subroutines of AC_CANONICAL_SYSTEM.

AC_DEFUN(AC_CANONICAL_HOST,
[AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl

# Make sure we can run config.sub.
if ${CONFIG_SHELL-/bin/sh} $ac_config_sub sun4 >/dev/null 2>&1; then :
else AC_MSG_ERROR(can not run $ac_config_sub)
fi

AC_MSG_CHECKING(host system type)

dnl Set host_alias.
host_alias=$host
case "$host_alias" in
NONE)
  case $nonopt in
  NONE)
    if host_alias=`${CONFIG_SHELL-/bin/sh} $ac_config_guess`; then :
    else AC_MSG_ERROR(can not guess host type; you must specify one)
    fi ;;
  *) host_alias=$nonopt ;;
  esac ;;
esac

dnl Set the other host vars.
changequote(<<, >>)dnl
host=`${CONFIG_SHELL-/bin/sh} $ac_config_sub $host_alias`
host_cpu=`echo $host | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\1/'`
host_vendor=`echo $host | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\2/'`
host_os=`echo $host | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\3/'`
changequote([, ])dnl
AC_MSG_RESULT($host)
AC_SUBST(host)dnl
AC_SUBST(host_alias)dnl
AC_SUBST(host_cpu)dnl
AC_SUBST(host_vendor)dnl
AC_SUBST(host_os)dnl
])

dnl Internal use only.
AC_DEFUN(AC_CANONICAL_TARGET,
[AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
AC_MSG_CHECKING(target system type)

dnl Set target_alias.
target_alias=$target
case "$target_alias" in
NONE)
  case $nonopt in
  NONE) target_alias=$host_alias ;;
  *) target_alias=$nonopt ;;
  esac ;;
esac

dnl Set the other target vars.
changequote(<<, >>)dnl
target=`${CONFIG_SHELL-/bin/sh} $ac_config_sub $target_alias`
target_cpu=`echo $target | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\1/'`
target_vendor=`echo $target | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\2/'`
target_os=`echo $target | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\3/'`
changequote([, ])dnl
AC_MSG_RESULT($target)
AC_SUBST(target)dnl
AC_SUBST(target_alias)dnl
AC_SUBST(target_cpu)dnl
AC_SUBST(target_vendor)dnl
AC_SUBST(target_os)dnl
])

dnl Internal use only.
AC_DEFUN(AC_CANONICAL_BUILD,
[AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
AC_MSG_CHECKING(build system type)

dnl Set build_alias.
build_alias=$build
case "$build_alias" in
NONE)
  case $nonopt in
  NONE) build_alias=$host_alias ;;
  *) build_alias=$nonopt ;;
  esac ;;
esac

dnl Set the other build vars.
changequote(<<, >>)dnl
build=`${CONFIG_SHELL-/bin/sh} $ac_config_sub $build_alias`
build_cpu=`echo $build | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\1/'`
build_vendor=`echo $build | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\2/'`
build_os=`echo $build | sed 's/^\([^-]*\)-\([^-]*\)-\(.*\)$/\3/'`
changequote([, ])dnl
AC_MSG_RESULT($build)
AC_SUBST(build)dnl
AC_SUBST(build_alias)dnl
AC_SUBST(build_cpu)dnl
AC_SUBST(build_vendor)dnl
AC_SUBST(build_os)dnl
])


dnl AC_VALIDATE_CACHED_SYSTEM_TUPLE[(cmd)]
dnl if the cache file is inconsistent with the current host,
dnl target and build system types, execute CMD or print a default
dnl error message.
AC_DEFUN(AC_VALIDATE_CACHED_SYSTEM_TUPLE, [
  AC_REQUIRE([AC_CANONICAL_SYSTEM])
  AC_MSG_CHECKING([cached system tuple])
  if { test x"${ac_cv_host_system_type+set}" = x"set" &&
       test x"$ac_cv_host_system_type" != x"$host"; } ||
     { test x"${ac_cv_build_system_type+set}" = x"set" &&
       test x"$ac_cv_build_system_type" != x"$build"; } ||
     { test x"${ac_cv_target_system_type+set}" = x"set" &&
       test x"$ac_cv_target_system_type" != x"$target"; }; then
      AC_MSG_RESULT([different])
      ifelse($#, 1, [$1],
        [AC_MSG_ERROR([remove config.cache and re-run configure])])
  else
    AC_MSG_RESULT(ok)
  fi
  ac_cv_host_system_type="$host"
  ac_cv_build_system_type="$build"
  ac_cv_target_system_type="$target"
])


dnl ### Caching test results


dnl Look for site or system specific initialization scripts.
dnl AC_SITE_LOAD()
define(AC_SITE_LOAD,
[# Prefer explicitly selected file to automatically selected ones.
if test -z "$CONFIG_SITE"; then
  if test "x$prefix" != xNONE; then
    CONFIG_SITE="$prefix/share/config.site $prefix/etc/config.site"
  else
    CONFIG_SITE="$ac_default_prefix/share/config.site $ac_default_prefix/etc/config.site"
  fi
fi
for ac_site_file in $CONFIG_SITE; do
  if test -r "$ac_site_file"; then
    echo "loading site script $ac_site_file"
    . "$ac_site_file"
  fi
done
])

dnl AC_CACHE_LOAD()
define(AC_CACHE_LOAD,
[if test -r "$cache_file"; then
  echo "loading cache $cache_file"
  . $cache_file
else
  echo "creating cache $cache_file"
  > $cache_file
fi
])

dnl AC_CACHE_SAVE()
define(AC_CACHE_SAVE,
[cat > confcache <<\EOF
# This file is a shell script that caches the results of configure
# tests run on this system so they can be shared between configure
# scripts and configure runs.  It is not useful on other systems.
# If it contains results you don't want to keep, you may remove or edit it.
#
# By default, configure uses ./config.cache as the cache file,
# creating it if it does not exist already.  You can give configure
# the --cache-file=FILE option to use a different cache file; that is
# what configure does when it calls configure scripts in
# subdirectories, so they share the cache.
# Giving --cache-file=/dev/null disables caching, for debugging configure.
# config.status only pays attention to the cache file if you give it the
# --recheck option to rerun configure.
#
EOF
dnl Allow a site initialization script to override cache values.
# The following way of writing the cache mishandles newlines in values,
# but we know of no workaround that is simple, portable, and efficient.
# So, don't put newlines in cache variables' values.
# Ultrix sh set writes to stderr and can't be redirected directly,
# and sets the high bit in the cache file unless we assign to the vars.
changequote(, )dnl
(set) 2>&1 |
  case `(ac_space=' '; set | grep ac_space) 2>&1` in
  *ac_space=\ *)
    # `set' does not quote correctly, so add quotes (double-quote substitution
    # turns \\\\ into \\, and sed turns \\ into \).
    sed -n \
      -e "s/'/'\\\\''/g" \
      -e "s/^\\([a-zA-Z0-9_]*_cv_[a-zA-Z0-9_]*\\)=\\(.*\\)/\\1=\${\\1='\\2'}/p"
    ;;
  *)
    # `set' quotes correctly as required by POSIX, so do not add quotes.
    sed -n -e 's/^\([a-zA-Z0-9_]*_cv_[a-zA-Z0-9_]*\)=\(.*\)/\1=${\1=\2}/p'
    ;;
  esac >> confcache
changequote([, ])dnl
if cmp -s $cache_file confcache; then
  :
else
  if test -w $cache_file; then
    echo "updating cache $cache_file"
    cat confcache > $cache_file
  else
    echo "not updating unwritable cache $cache_file"
  fi
fi
rm -f confcache
])

dnl The name of shell var CACHE-ID must contain `_cv_' in order to get saved.
dnl AC_CACHE_VAL(CACHE-ID, COMMANDS-TO-SET-IT)
define(AC_CACHE_VAL,
[dnl We used to use the below line, but it fails if the 1st arg is a
dnl shell variable, so we need the eval.
dnl if test "${$1+set}" = set; then
dnl the '' avoids an AIX 4.1 sh bug ("invalid expansion").
if eval "test \"`echo '$''{'$1'+set}'`\" = set"; then
  echo $ac_n "(cached) $ac_c" 1>&AC_FD_MSG
else
  $2
fi
])

dnl AC_CACHE_CHECK(MESSAGE, CACHE-ID, COMMANDS)
define(AC_CACHE_CHECK,
[AC_MSG_CHECKING([$1])
AC_CACHE_VAL([$2], [$3])
AC_MSG_RESULT([$]$2)])


dnl ### Defining symbols


dnl Set VARIABLE to VALUE, verbatim, or 1.
dnl AC_DEFINE(VARIABLE [, VALUE])
define(AC_DEFINE,
[cat >> confdefs.h <<\EOF
[#define] $1 ifelse($#, 2, [$2], $#, 3, [$2], 1)
EOF
])

dnl Similar, but perform shell substitutions $ ` \ once on VALUE.
define(AC_DEFINE_UNQUOTED,
[cat >> confdefs.h <<EOF
[#define] $1 ifelse($#, 2, [$2], $#, 3, [$2], 1)
EOF
])


dnl ### Setting output variables


dnl This macro protects VARIABLE from being diverted twice
dnl if this macro is called twice for it.
dnl AC_SUBST(VARIABLE)
define(AC_SUBST,
[ifdef([AC_SUBST_$1], ,
[define([AC_SUBST_$1], )dnl
AC_DIVERT_PUSH(AC_DIVERSION_SED)dnl
s%@$1@%[$]$1%g
AC_DIVERT_POP()dnl
])])

dnl AC_SUBST_FILE(VARIABLE)
define(AC_SUBST_FILE,
[ifdef([AC_SUBST_$1], ,
[define([AC_SUBST_$1], )dnl
AC_DIVERT_PUSH(AC_DIVERSION_SED)dnl
/@$1@/r [$]$1
s%@$1@%%g
AC_DIVERT_POP()dnl
])])


dnl ### Printing messages


dnl AC_MSG_CHECKING(FEATURE-DESCRIPTION)
define(AC_MSG_CHECKING,
[echo $ac_n "checking $1""... $ac_c" 1>&AC_FD_MSG
echo "configure:__oline__: checking $1" >&AC_FD_CC])

dnl AC_CHECKING(FEATURE-DESCRIPTION)
define(AC_CHECKING,
[echo "checking $1" 1>&AC_FD_MSG
echo "configure:__oline__: checking $1" >&AC_FD_CC])

dnl AC_MSG_RESULT(RESULT-DESCRIPTION)
define(AC_MSG_RESULT,
[echo "$ac_t""$1" 1>&AC_FD_MSG])

dnl AC_VERBOSE(RESULT-DESCRIPTION)
define(AC_VERBOSE,
[AC_OBSOLETE([$0], [; instead use AC_MSG_RESULT])dnl
echo "	$1" 1>&AC_FD_MSG])

dnl AC_MSG_WARN(PROBLEM-DESCRIPTION)
define(AC_MSG_WARN,
[echo "configure: warning: $1" 1>&2])

dnl AC_MSG_ERROR(ERROR-DESCRIPTION)
define(AC_MSG_ERROR,
[{ echo "configure: error: $1" 1>&2; exit 1; }])


dnl ### Selecting which language to use for testing


dnl AC_LANG_C()
AC_DEFUN(AC_LANG_C,
[define([AC_LANG], [C])dnl
ac_ext=c
# CFLAGS is not in ac_cpp because -g, -O, etc. are not valid cpp options.
ac_cpp='$CPP $CPPFLAGS'
ac_compile='${CC-cc} -c $CFLAGS $CPPFLAGS conftest.$ac_ext 1>&AC_FD_CC'
ac_link='${CC-cc} -o conftest${ac_exeext} $CFLAGS $CPPFLAGS $LDFLAGS conftest.$ac_ext $LIBS 1>&AC_FD_CC'
cross_compiling=$ac_cv_prog_cc_cross
])

dnl AC_LANG_CPLUSPLUS()
AC_DEFUN(AC_LANG_CPLUSPLUS,
[define([AC_LANG], [CPLUSPLUS])dnl
ac_ext=C
# CXXFLAGS is not in ac_cpp because -g, -O, etc. are not valid cpp options.
ac_cpp='$CXXCPP $CPPFLAGS'
ac_compile='${CXX-g++} -c $CXXFLAGS $CPPFLAGS conftest.$ac_ext 1>&AC_FD_CC'
ac_link='${CXX-g++} -o conftest${ac_exeext} $CXXFLAGS $CPPFLAGS $LDFLAGS conftest.$ac_ext $LIBS 1>&AC_FD_CC'
cross_compiling=$ac_cv_prog_cxx_cross
])

dnl AC_LANG_FORTRAN77()
AC_DEFUN(AC_LANG_FORTRAN77,
[define([AC_LANG], [FORTRAN77])dnl
ac_ext=f
ac_compile='${F77-f77} -c $FFLAGS conftest.$ac_ext 1>&AC_FD_CC'
ac_link='${F77-f77} -o conftest${ac_exeext} $FFLAGS $LDFLAGS conftest.$ac_ext $LIBS 1>&AC_FD_CC'
cross_compiling=$ac_cv_prog_f77_cross
])

dnl Push the current language on a stack.
dnl AC_LANG_SAVE()
define(AC_LANG_SAVE,
[pushdef([AC_LANG_STACK], AC_LANG)])

dnl Restore the current language from the stack.
dnl AC_LANG_RESTORE()
pushdef([AC_LANG_RESTORE],
[ifelse(AC_LANG_STACK, [C], [AC_LANG_C],dnl
AC_LANG_STACK, [CPLUSPLUS], [AC_LANG_CPLUSPLUS],dnl
AC_LANG_STACK, [FORTRAN77], [AC_LANG_FORTRAN77])[]popdef([AC_LANG_STACK])])


dnl ### Compiler-running mechanics


dnl The purpose of this macro is to "configure:123: command line"
dnl written into config.log for every test run.
dnl AC_TRY_EVAL(VARIABLE)
AC_DEFUN(AC_TRY_EVAL,
[{ (eval echo configure:__oline__: \"[$]$1\") 1>&AC_FD_CC; dnl
(eval [$]$1) 2>&AC_FD_CC; }])

dnl AC_TRY_COMMAND(COMMAND)
AC_DEFUN(AC_TRY_COMMAND,
[{ ac_try='$1'; AC_TRY_EVAL(ac_try); }])


dnl ### Dependencies between macros


dnl AC_BEFORE(THIS-MACRO-NAME, CALLED-MACRO-NAME)
define(AC_BEFORE,
[ifdef([AC_PROVIDE_$2], [errprint(__file__:__line__: [$2 was called before $1
])])])

dnl AC_REQUIRE(MACRO-NAME)
define(AC_REQUIRE,
[ifdef([AC_PROVIDE_$1], ,
[AC_DIVERT_PUSH(builtin(eval, AC_DIVERSION_CURRENT - 1))dnl
indir([$1])
AC_DIVERT_POP()dnl
])])

dnl AC_PROVIDE(MACRO-NAME)
define(AC_PROVIDE,
[define([AC_PROVIDE_$1], )])

dnl AC_OBSOLETE(THIS-MACRO-NAME [, SUGGESTION])
define(AC_OBSOLETE,
[errprint(__file__:__line__: warning: [$1] is obsolete[$2]
)])


dnl ### Checking for programs


dnl AC_CHECK_PROG(VARIABLE, PROG-TO-CHECK-FOR, VALUE-IF-FOUND
dnl               [, [VALUE-IF-NOT-FOUND] [, [PATH] [, [REJECT]]]])
AC_DEFUN(AC_CHECK_PROG,
[# Extract the first word of "$2", so it can be a program name with args.
set dummy $2; ac_word=[$]2
AC_MSG_CHECKING([for $ac_word])
AC_CACHE_VAL(ac_cv_prog_$1,
[if test -n "[$]$1"; then
  ac_cv_prog_$1="[$]$1" # Let the user override the test.
else
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS=":"
ifelse([$6], , , [  ac_prog_rejected=no
])dnl
dnl $ac_dummy forces splitting on constant user-supplied paths.
dnl POSIX.2 word splitting is done only on the output of word expansions,
dnl not every word.  This closes a longstanding sh security hole.
  ac_dummy="ifelse([$5], , $PATH, [$5])"
  for ac_dir in $ac_dummy; do
    test -z "$ac_dir" && ac_dir=.
    if test -f $ac_dir/$ac_word; then
ifelse([$6], , , dnl
[      if test "[$ac_dir/$ac_word]" = "$6"; then
        ac_prog_rejected=yes
	continue
      fi
])dnl
      ac_cv_prog_$1="$3"
      break
    fi
  done
  IFS="$ac_save_ifs"
ifelse([$6], , , [if test $ac_prog_rejected = yes; then
  # We found a bogon in the path, so make sure we never use it.
  set dummy [$]ac_cv_prog_$1
  shift
  if test [$]# -gt 0; then
    # We chose a different compiler from the bogus one.
    # However, it has the same basename, so the bogon will be chosen
    # first if we set $1 to just the basename; use the full file name.
    shift
    set dummy "$ac_dir/$ac_word" "[$]@"
    shift
    ac_cv_prog_$1="[$]@"
ifelse([$2], [$4], dnl
[  else
    # Default is a loser.
    AC_MSG_ERROR([$1=$6 unacceptable, but no other $4 found in dnl
ifelse([$5], , [\$]PATH, [$5])])
])dnl
  fi
fi
])dnl
dnl If no 4th arg is given, leave the cache variable unset,
dnl so AC_CHECK_PROGS will keep looking.
ifelse([$4], , , [  test -z "[$]ac_cv_prog_$1" && ac_cv_prog_$1="$4"
])dnl
fi])dnl
$1="$ac_cv_prog_$1"
if test -n "[$]$1"; then
  AC_MSG_RESULT([$]$1)
else
  AC_MSG_RESULT(no)
fi
AC_SUBST($1)dnl
])

dnl AC_PATH_PROG(VARIABLE, PROG-TO-CHECK-FOR [, VALUE-IF-NOT-FOUND [, PATH]])
AC_DEFUN(AC_PATH_PROG,
[# Extract the first word of "$2", so it can be a program name with args.
set dummy $2; ac_word=[$]2
AC_MSG_CHECKING([for $ac_word])
AC_CACHE_VAL(ac_cv_path_$1,
[case "[$]$1" in
  /*)
  ac_cv_path_$1="[$]$1" # Let the user override the test with a path.
  ;;
  ?:/*)			 
  ac_cv_path_$1="[$]$1" # Let the user override the test with a dos path.
  ;;
  *)
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS=":"
dnl $ac_dummy forces splitting on constant user-supplied paths.
dnl POSIX.2 word splitting is done only on the output of word expansions,
dnl not every word.  This closes a longstanding sh security hole.
  ac_dummy="ifelse([$4], , $PATH, [$4])"
  for ac_dir in $ac_dummy; do 
    test -z "$ac_dir" && ac_dir=.
    if test -f $ac_dir/$ac_word; then
      ac_cv_path_$1="$ac_dir/$ac_word"
      break
    fi
  done
  IFS="$ac_save_ifs"
dnl If no 3rd arg is given, leave the cache variable unset,
dnl so AC_PATH_PROGS will keep looking.
ifelse([$3], , , [  test -z "[$]ac_cv_path_$1" && ac_cv_path_$1="$3"
])dnl
  ;;
esac])dnl
$1="$ac_cv_path_$1"
if test -n "[$]$1"; then
  AC_MSG_RESULT([$]$1)
else
  AC_MSG_RESULT(no)
fi
AC_SUBST($1)dnl
])

dnl AC_CHECK_PROGS(VARIABLE, PROGS-TO-CHECK-FOR [, VALUE-IF-NOT-FOUND
dnl                [, PATH]])
AC_DEFUN(AC_CHECK_PROGS,
[for ac_prog in $2
do
AC_CHECK_PROG($1, [$]ac_prog, [$]ac_prog, , $4)
test -n "[$]$1" && break
done
ifelse([$3], , , [test -n "[$]$1" || $1="$3"
])])

dnl AC_PATH_PROGS(VARIABLE, PROGS-TO-CHECK-FOR [, VALUE-IF-NOT-FOUND
dnl               [, PATH]])
AC_DEFUN(AC_PATH_PROGS,
[for ac_prog in $2
do
AC_PATH_PROG($1, [$]ac_prog, , $4)
test -n "[$]$1" && break
done
ifelse([$3], , , [test -n "[$]$1" || $1="$3"
])])

dnl Internal subroutine.
AC_DEFUN(AC_CHECK_TOOL_PREFIX,
[AC_REQUIRE([AC_CANONICAL_HOST])AC_REQUIRE([AC_CANONICAL_BUILD])dnl
if test $host != $build; then
  ac_tool_prefix=${host_alias}-
else
  ac_tool_prefix=
fi
])

dnl AC_CHECK_TOOL(VARIABLE, PROG-TO-CHECK-FOR[, VALUE-IF-NOT-FOUND [, PATH]])
AC_DEFUN(AC_CHECK_TOOL,
[AC_REQUIRE([AC_CHECK_TOOL_PREFIX])dnl
AC_CHECK_PROG($1, ${ac_tool_prefix}$2, ${ac_tool_prefix}$2,
	      ifelse([$3], , [$2], ), $4)
ifelse([$3], , , [
if test -z "$ac_cv_prog_$1"; then
if test -n "$ac_tool_prefix"; then
  AC_CHECK_PROG($1, $2, $2, $3)
else
  $1="$3"
fi
fi])
])

dnl Guess the value for the `prefix' variable by looking for
dnl the argument program along PATH and taking its parent.
dnl Example: if the argument is `gcc' and we find /usr/local/gnu/bin/gcc,
dnl set `prefix' to /usr/local/gnu.
dnl This comes too late to find a site file based on the prefix,
dnl and it might use a cached value for the path.
dnl No big loss, I think, since most configures don't use this macro anyway.
dnl AC_PREFIX_PROGRAM(PROGRAM)
AC_DEFUN(AC_PREFIX_PROGRAM,
[if test "x$prefix" = xNONE; then
changequote(<<, >>)dnl
define(<<AC_VAR_NAME>>, translit($1, [a-z], [A-Z]))dnl
changequote([, ])dnl
dnl We reimplement AC_MSG_CHECKING (mostly) to avoid the ... in the middle.
echo $ac_n "checking for prefix by $ac_c" 1>&AC_FD_MSG
AC_PATH_PROG(AC_VAR_NAME, $1)
changequote(<<, >>)dnl
  if test -n "$ac_cv_path_<<>>AC_VAR_NAME"; then
    prefix=`echo $ac_cv_path_<<>>AC_VAR_NAME|sed 's%/[^/][^/]*//*[^/][^/]*$%%'`
changequote([, ])dnl
  fi
fi
undefine([AC_VAR_NAME])dnl
])

dnl Try to compile, link and execute TEST-PROGRAM.  Set WORKING-VAR to
dnl `yes' if the current compiler works, otherwise set it ti `no'.  Set
dnl CROSS-VAR to `yes' if the compiler and linker produce non-native
dnl executables, otherwise set it to `no'.  Before calling
dnl `AC_TRY_COMPILER()', call `AC_LANG_*' to set-up for the right
dnl language.
dnl 
dnl AC_TRY_COMPILER(TEST-PROGRAM, WORKING-VAR, CROSS-VAR)
AC_DEFUN(AC_TRY_COMPILER,
[cat > conftest.$ac_ext << EOF
ifelse(AC_LANG, [FORTRAN77], ,
[
[#]line __oline__ "configure"
#include "confdefs.h"
])
[$1]
EOF
if AC_TRY_EVAL(ac_link) && test -s conftest${ac_exeext}; then
  [$2]=yes
  # If we can't run a trivial program, we are probably using a cross compiler.
  if (./conftest; exit) 2>/dev/null; then
    [$3]=no
  else
    [$3]=yes
  fi
else
  echo "configure: failed program was:" >&AC_FD_CC
  cat conftest.$ac_ext >&AC_FD_CC
  [$2]=no
fi
rm -fr conftest*])


dnl ### Checking for libraries


dnl AC_TRY_LINK_FUNC(func, action-if-found, action-if-not-found)
dnl Try to link a program that calls FUNC, handling GCC builtins.  If
dnl the link succeeds, execute ACTION-IF-FOUND; otherwise, execute
dnl ACTION-IF-NOT-FOUND.

AC_DEFUN(AC_TRY_LINK_FUNC,
AC_TRY_LINK(dnl
ifelse([$1], [main], , dnl Avoid conflicting decl of main.
[/* Override any gcc2 internal prototype to avoid an error.  */
]ifelse(AC_LANG, CPLUSPLUS, [#ifdef __cplusplus
extern "C"
#endif
])dnl
[/* We use char because int might match the return type of a gcc2
    builtin and then its argument prototype would still apply.  */
char $1();
]),
[$1()],
[$2],
[$3]))


dnl AC_SEARCH_LIBS(FUNCTION, SEARCH-LIBS [, ACTION-IF-FOUND
dnl            [, ACTION-IF-NOT-FOUND [, OTHER-LIBRARIES]]])
dnl Search for a library defining FUNC, if it's not already available.

AC_DEFUN(AC_SEARCH_LIBS,
[AC_PREREQ([2.13])
AC_CACHE_CHECK([for library containing $1], [ac_cv_search_$1],
[ac_func_search_save_LIBS="$LIBS"
ac_cv_search_$1="no"
AC_TRY_LINK_FUNC([$1], [ac_cv_search_$1="none required"])
test "$ac_cv_search_$1" = "no" && for i in $2; do
LIBS="-l$i $5 $ac_func_search_save_LIBS"
AC_TRY_LINK_FUNC([$1],
[ac_cv_search_$1="-l$i"
break])
done
LIBS="$ac_func_search_save_LIBS"])
if test "$ac_cv_search_$1" != "no"; then
  test "$ac_cv_search_$1" = "none required" || LIBS="$ac_cv_search_$1 $LIBS"
  $3
else :
  $4
fi])



dnl AC_CHECK_LIB(LIBRARY, FUNCTION [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND
dnl              [, OTHER-LIBRARIES]]])
AC_DEFUN(AC_CHECK_LIB,
[AC_MSG_CHECKING([for $2 in -l$1])
dnl Use a cache variable name containing both the library and function name,
dnl because the test really is for library $1 defining function $2, not
dnl just for library $1.  Separate tests with the same $1 and different $2s
dnl may have different results.
ac_lib_var=`echo $1['_']$2 | sed 'y%./+-%__p_%'`
AC_CACHE_VAL(ac_cv_lib_$ac_lib_var,
[ac_save_LIBS="$LIBS"
LIBS="-l$1 $5 $LIBS"
AC_TRY_LINK(dnl
ifelse(AC_LANG, [FORTRAN77], ,
ifelse([$2], [main], , dnl Avoid conflicting decl of main.
[/* Override any gcc2 internal prototype to avoid an error.  */
]ifelse(AC_LANG, CPLUSPLUS, [#ifdef __cplusplus
extern "C"
#endif
])dnl
[/* We use char because int might match the return type of a gcc2
    builtin and then its argument prototype would still apply.  */
char $2();
])),
	    [$2()],
	    eval "ac_cv_lib_$ac_lib_var=yes",
	    eval "ac_cv_lib_$ac_lib_var=no")
LIBS="$ac_save_LIBS"
])dnl
if eval "test \"`echo '$ac_cv_lib_'$ac_lib_var`\" = yes"; then
  AC_MSG_RESULT(yes)
  ifelse([$3], ,
[changequote(, )dnl
  ac_tr_lib=HAVE_LIB`echo $1 | sed -e 's/[^a-zA-Z0-9_]/_/g' \
    -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/'`
changequote([, ])dnl
  AC_DEFINE_UNQUOTED($ac_tr_lib)
  LIBS="-l$1 $LIBS"
], [$3])
else
  AC_MSG_RESULT(no)
ifelse([$4], , , [$4
])dnl
fi
])

dnl AC_HAVE_LIBRARY(LIBRARY, [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND
dnl                 [, OTHER-LIBRARIES]]])
AC_DEFUN(AC_HAVE_LIBRARY,
[AC_OBSOLETE([$0], [; instead use AC_CHECK_LIB])dnl
changequote(<<, >>)dnl
define(<<AC_LIB_NAME>>, dnl
patsubst(patsubst($1, <<lib\([^\.]*\)\.a>>, <<\1>>), <<-l>>, <<>>))dnl
define(<<AC_CV_NAME>>, ac_cv_lib_<<>>AC_LIB_NAME)dnl
changequote([, ])dnl
AC_MSG_CHECKING([for -l[]AC_LIB_NAME])
AC_CACHE_VAL(AC_CV_NAME,
[ac_save_LIBS="$LIBS"
LIBS="-l[]AC_LIB_NAME[] $4 $LIBS"
AC_TRY_LINK( , [main()], AC_CV_NAME=yes, AC_CV_NAME=no)
LIBS="$ac_save_LIBS"
])dnl
AC_MSG_RESULT($AC_CV_NAME)
if test "$AC_CV_NAME" = yes; then
  ifelse([$2], ,
[AC_DEFINE([HAVE_LIB]translit(AC_LIB_NAME, [a-z], [A-Z]))
  LIBS="-l[]AC_LIB_NAME[] $LIBS"
], [$2])
ifelse([$3], , , [else
  $3
])dnl
fi
undefine([AC_LIB_NAME])dnl
undefine([AC_CV_NAME])dnl
])


dnl ### Examining declarations


dnl AC_TRY_CPP(INCLUDES, [ACTION-IF-TRUE [, ACTION-IF-FALSE]])
AC_DEFUN(AC_TRY_CPP,
[AC_REQUIRE_CPP()dnl
cat > conftest.$ac_ext <<EOF
[#]line __oline__ "configure"
#include "confdefs.h"
[$1]
EOF
dnl Capture the stderr of cpp.  eval is necessary to expand ac_cpp.
dnl We used to copy stderr to stdout and capture it in a variable, but
dnl that breaks under sh -x, which writes compile commands starting
dnl with ` +' to stderr in eval and subshells.
ac_try="$ac_cpp conftest.$ac_ext >/dev/null 2>conftest.out"
AC_TRY_EVAL(ac_try)
ac_err=`grep -v '^ *+' conftest.out | grep -v "^conftest.${ac_ext}\$"`
if test -z "$ac_err"; then
  ifelse([$2], , :, [rm -rf conftest*
  $2])
else
  echo "$ac_err" >&AC_FD_CC
  echo "configure: failed program was:" >&AC_FD_CC
  cat conftest.$ac_ext >&AC_FD_CC
ifelse([$3], , , [  rm -rf conftest*
  $3
])dnl
fi
rm -f conftest*])

dnl AC_EGREP_HEADER(PATTERN, HEADER-FILE, ACTION-IF-FOUND [,
dnl                 ACTION-IF-NOT-FOUND])
AC_DEFUN(AC_EGREP_HEADER,
[AC_EGREP_CPP([$1], [#include <$2>], [$3], [$4])])

dnl Because this macro is used by AC_PROG_GCC_TRADITIONAL, which must
dnl come early, it is not included in AC_BEFORE checks.
dnl AC_EGREP_CPP(PATTERN, PROGRAM, [ACTION-IF-FOUND [,
dnl              ACTION-IF-NOT-FOUND]])
AC_DEFUN(AC_EGREP_CPP,
[AC_REQUIRE_CPP()dnl
cat > conftest.$ac_ext <<EOF
[#]line __oline__ "configure"
#include "confdefs.h"
[$2]
EOF
dnl eval is necessary to expand ac_cpp.
dnl Ultrix and Pyramid sh refuse to redirect output of eval, so use subshell.
if (eval "$ac_cpp conftest.$ac_ext") 2>&AC_FD_CC |
dnl Prevent m4 from eating character classes:
changequote(, )dnl
  grep -E "$1" >/dev/null 2>&1; then
changequote([, ])dnl
  ifelse([$3], , :, [rm -rf conftest*
  $3])
ifelse([$4], , , [else
  rm -rf conftest*
  $4
])dnl
fi
rm -f conftest*
])


dnl ### Examining syntax


dnl AC_TRY_COMPILE(INCLUDES, FUNCTION-BODY,
dnl             [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN(AC_TRY_COMPILE,
[cat > conftest.$ac_ext <<EOF
ifelse(AC_LANG, [FORTRAN77],
[      program main
[$2]
      end],
[dnl This sometimes fails to find confdefs.h, for some reason.
dnl [#]line __oline__ "[$]0"
[#]line __oline__ "configure"
#include "confdefs.h"
[$1]
int main() {
[$2]
; return 0; }
])EOF
if AC_TRY_EVAL(ac_compile); then
  ifelse([$3], , :, [rm -rf conftest*
  $3])
else
  echo "configure: failed program was:" >&AC_FD_CC
  cat conftest.$ac_ext >&AC_FD_CC
ifelse([$4], , , [  rm -rf conftest*
  $4
])dnl
fi
rm -f conftest*])


dnl ### Examining libraries


dnl AC_COMPILE_CHECK(ECHO-TEXT, INCLUDES, FUNCTION-BODY,
dnl                  ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
AC_DEFUN(AC_COMPILE_CHECK,
[AC_OBSOLETE([$0], [; instead use AC_TRY_COMPILE or AC_TRY_LINK, and AC_MSG_CHECKING and AC_MSG_RESULT])dnl
ifelse([$1], , , [AC_CHECKING([for $1])
])dnl
AC_TRY_LINK([$2], [$3], [$4], [$5])
])

dnl AC_TRY_LINK(INCLUDES, FUNCTION-BODY,
dnl             [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN(AC_TRY_LINK,
[cat > conftest.$ac_ext <<EOF
ifelse(AC_LANG, [FORTRAN77],
[
      program main
      call [$2]
      end
],
[dnl This sometimes fails to find confdefs.h, for some reason.
dnl [#]line __oline__ "[$]0"
[#]line __oline__ "configure"
#include "confdefs.h"
[$1]
int main() {
[$2]
; return 0; }
])EOF
if AC_TRY_EVAL(ac_link) && test -s conftest${ac_exeext}; then
  ifelse([$3], , :, [rm -rf conftest*
  $3])
else
  echo "configure: failed program was:" >&AC_FD_CC
  cat conftest.$ac_ext >&AC_FD_CC
ifelse([$4], , , [  rm -rf conftest*
  $4
])dnl
fi
rm -f conftest*])


dnl ### Checking for run-time features


dnl AC_TRY_RUN(PROGRAM, [ACTION-IF-TRUE [, ACTION-IF-FALSE
dnl            [, ACTION-IF-CROSS-COMPILING]]])
AC_DEFUN(AC_TRY_RUN,
[if test "$cross_compiling" = yes; then
  ifelse([$4], ,
    [errprint(__file__:__line__: warning: [AC_TRY_RUN] called without default to allow cross compiling
)dnl
  AC_MSG_ERROR(can not run test program while cross compiling)],
  [$4])
else
  AC_TRY_RUN_NATIVE([$1], [$2], [$3])
fi
])

dnl Like AC_TRY_RUN but assumes a native-environment (non-cross) compiler.
dnl AC_TRY_RUN_NATIVE(PROGRAM, [ACTION-IF-TRUE [, ACTION-IF-FALSE]])
AC_DEFUN(AC_TRY_RUN_NATIVE,
[cat > conftest.$ac_ext <<EOF
[#]line __oline__ "configure"
#include "confdefs.h"
ifelse(AC_LANG, CPLUSPLUS, [#ifdef __cplusplus
extern "C" void exit(int);
#endif
])dnl
[$1]
EOF
if AC_TRY_EVAL(ac_link) && test -s conftest${ac_exeext} && (./conftest; exit) 2>/dev/null
then
dnl Don't remove the temporary files here, so they can be examined.
  ifelse([$2], , :, [$2])
else
  echo "configure: failed program was:" >&AC_FD_CC
  cat conftest.$ac_ext >&AC_FD_CC
ifelse([$3], , , [  rm -fr conftest*
  $3
])dnl
fi
rm -fr conftest*])


dnl ### Checking for header files


dnl AC_CHECK_HEADER(HEADER-FILE, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN(AC_CHECK_HEADER,
[dnl Do the transliteration at runtime so arg 1 can be a shell variable.
ac_safe=`echo "$1" | sed 'y%./+-%__p_%'`
AC_MSG_CHECKING([for $1])
AC_CACHE_VAL(ac_cv_header_$ac_safe,
[AC_TRY_CPP([#include <$1>], eval "ac_cv_header_$ac_safe=yes",
  eval "ac_cv_header_$ac_safe=no")])dnl
if eval "test \"`echo '$ac_cv_header_'$ac_safe`\" = yes"; then
  AC_MSG_RESULT(yes)
  ifelse([$2], , :, [$2])
else
  AC_MSG_RESULT(no)
ifelse([$3], , , [$3
])dnl
fi
])

dnl AC_CHECK_HEADERS(HEADER-FILE... [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN(AC_CHECK_HEADERS,
[for ac_hdr in $1
do
AC_CHECK_HEADER($ac_hdr,
[changequote(, )dnl
  ac_tr_hdr=HAVE_`echo $ac_hdr | sed 'y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%'`
changequote([, ])dnl
  AC_DEFINE_UNQUOTED($ac_tr_hdr) $2], $3)dnl
done
])


dnl ### Checking for the existence of files

dnl AC_CHECK_FILE(FILE, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN(AC_CHECK_FILE,
[AC_REQUIRE([AC_PROG_CC])
dnl Do the transliteration at runtime so arg 1 can be a shell variable.
ac_safe=`echo "$1" | sed 'y%./+-%__p_%'`
AC_MSG_CHECKING([for $1])
AC_CACHE_VAL(ac_cv_file_$ac_safe,
[if test "$cross_compiling" = yes; then
  errprint(__file__:__line__: warning: Cannot check for file existence when cross compiling
)dnl
  AC_MSG_ERROR(Cannot check for file existence when cross compiling)
else
  if test -r $1; then
    eval "ac_cv_file_$ac_safe=yes"
  else
    eval "ac_cv_file_$ac_safe=no"
  fi
fi])dnl
if eval "test \"`echo '$ac_cv_file_'$ac_safe`\" = yes"; then
  AC_MSG_RESULT(yes)
  ifelse([$2], , :, [$2])
else
  AC_MSG_RESULT(no)
ifelse([$3], , , [$3])
fi
])

dnl AC_CHECK_FILES(FILE... [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN(AC_CHECK_FILES,
[for ac_file in $1
do
AC_CHECK_FILE($ac_file,
[changequote(, )dnl
  ac_tr_file=HAVE_`echo $ac_file | sed 'y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%'`
changequote([, ])dnl
  AC_DEFINE_UNQUOTED($ac_tr_file) $2], $3)dnl
done
])


dnl ### Checking for library functions


dnl AC_CHECK_FUNC(FUNCTION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN(AC_CHECK_FUNC,
[AC_MSG_CHECKING([for $1])
AC_CACHE_VAL(ac_cv_func_$1,
[AC_TRY_LINK(
dnl Don't include <ctype.h> because on OSF/1 3.0 it includes <sys/types.h>
dnl which includes <sys/select.h> which contains a prototype for
dnl select.  Similarly for bzero.
[/* System header to define __stub macros and hopefully few prototypes,
    which can conflict with char $1(); below.  */
#include <assert.h>
/* Override any gcc2 internal prototype to avoid an error.  */
]ifelse(AC_LANG, CPLUSPLUS, [#ifdef __cplusplus
extern "C"
#endif
])dnl
[/* We use char because int might match the return type of a gcc2
    builtin and then its argument prototype would still apply.  */
char $1();
], [
/* The GNU C library defines this for functions which it implements
    to always fail with ENOSYS.  Some functions are actually named
    something starting with __ and the normal name is an alias.  */
#if defined (__stub_$1) || defined (__stub___$1)
choke me
#else
$1();
#endif
], eval "ac_cv_func_$1=yes", eval "ac_cv_func_$1=no")])
if eval "test \"`echo '$ac_cv_func_'$1`\" = yes"; then
  AC_MSG_RESULT(yes)
  ifelse([$2], , :, [$2])
else
  AC_MSG_RESULT(no)
ifelse([$3], , , [$3
])dnl
fi
])

dnl AC_CHECK_FUNCS(FUNCTION... [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN(AC_CHECK_FUNCS,
[for ac_func in $1
do
AC_CHECK_FUNC($ac_func,
[changequote(, )dnl
  ac_tr_func=HAVE_`echo $ac_func | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
changequote([, ])dnl
  AC_DEFINE_UNQUOTED($ac_tr_func) $2], $3)dnl
done
])

dnl AC_REPLACE_FUNCS(FUNCTION...)
AC_DEFUN(AC_REPLACE_FUNCS,
[AC_CHECK_FUNCS([$1], , [LIBOBJS="$LIBOBJS ${ac_func}.${ac_objext}"])
AC_SUBST(LIBOBJS)dnl
])


dnl ### Checking compiler characteristics


dnl AC_CHECK_SIZEOF(TYPE [, CROSS-SIZE])
AC_DEFUN(AC_CHECK_SIZEOF,
[changequote(<<, >>)dnl
dnl The name to #define.
define(<<AC_TYPE_NAME>>, translit(sizeof_$1, [a-z *], [A-Z_P]))dnl
dnl The cache variable name.
define(<<AC_CV_NAME>>, translit(ac_cv_sizeof_$1, [ *], [_p]))dnl
changequote([, ])dnl
AC_MSG_CHECKING(size of $1)
AC_CACHE_VAL(AC_CV_NAME,
[AC_TRY_RUN([#include <stdio.h>
main()
{
  FILE *f=fopen("conftestval", "w");
  if (!f) exit(1);
  fprintf(f, "%d\n", sizeof($1));
  exit(0);
}], AC_CV_NAME=`cat conftestval`, AC_CV_NAME=0, ifelse([$2], , , AC_CV_NAME=$2))])dnl
AC_MSG_RESULT($AC_CV_NAME)
AC_DEFINE_UNQUOTED(AC_TYPE_NAME, $AC_CV_NAME)
undefine([AC_TYPE_NAME])dnl
undefine([AC_CV_NAME])dnl
])


dnl ### Checking for typedefs


dnl AC_CHECK_TYPE(TYPE, DEFAULT)
AC_DEFUN(AC_CHECK_TYPE,
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(ac_cv_type_$1,
[AC_EGREP_CPP(dnl
changequote(<<,>>)dnl
<<(^|[^a-zA-Z_0-9])$1[^a-zA-Z_0-9]>>dnl
changequote([,]), [#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif], ac_cv_type_$1=yes, ac_cv_type_$1=no)])dnl
AC_MSG_RESULT($ac_cv_type_$1)
if test $ac_cv_type_$1 = no; then
  AC_DEFINE($1, $2)
fi
])


dnl ### Creating output files


dnl AC_CONFIG_HEADER(HEADER-TO-CREATE ...)
AC_DEFUN(AC_CONFIG_HEADER,
[define(AC_LIST_HEADER, $1)])

dnl Link each of the existing files SOURCE... to the corresponding
dnl link name in DEST...
dnl AC_LINK_FILES(SOURCE..., DEST...)
AC_DEFUN(AC_LINK_FILES,
[dnl
define([AC_LIST_FILES], ifdef([AC_LIST_FILES], [AC_LIST_FILES ],)[$1])dnl
define([AC_LIST_LINKS], ifdef([AC_LIST_LINKS], [AC_LIST_LINKS ],)[$2])])

dnl Add additional commands for AC_OUTPUT to put into config.status.
dnl Use diversions instead of macros so we can be robust in the
dnl presence of commas in $1 and/or $2.
dnl AC_OUTPUT_COMMANDS(EXTRA-CMDS, INIT-CMDS)
AC_DEFUN(AC_OUTPUT_COMMANDS,
[AC_DIVERT_PUSH(AC_DIVERSION_CMDS)dnl
[$1]
AC_DIVERT_POP()dnl
AC_DIVERT_PUSH(AC_DIVERSION_ICMDS)dnl
[$2]
AC_DIVERT_POP()])

dnl AC_CONFIG_SUBDIRS(DIR ...)
AC_DEFUN(AC_CONFIG_SUBDIRS,
[AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
define([AC_LIST_SUBDIRS], ifdef([AC_LIST_SUBDIRS], [AC_LIST_SUBDIRS ],)[$1])dnl
subdirs="AC_LIST_SUBDIRS"
AC_SUBST(subdirs)dnl
])

dnl The big finish.
dnl Produce config.status, config.h, and links; and configure subdirs.
dnl AC_OUTPUT([FILE...] [, EXTRA-CMDS] [, INIT-CMDS])
define(AC_OUTPUT,
[trap '' 1 2 15
AC_CACHE_SAVE
trap 'rm -fr conftest* confdefs* core core.* *.core $ac_clean_files; exit 1' 1 2 15

test "x$prefix" = xNONE && prefix=$ac_default_prefix
# Let make expand exec_prefix.
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

# Any assignment to VPATH causes Sun make to only execute
# the first set of double-colon rules, so remove it if not needed.
# If there is a colon in the path, we need to keep it.
if test "x$srcdir" = x.; then
changequote(, )dnl
  ac_vpsub='/^[ 	]*VPATH[ 	]*=[^:]*$/d'
changequote([, ])dnl
fi

trap 'rm -f $CONFIG_STATUS conftest*; exit 1' 1 2 15

ifdef([AC_LIST_HEADER], [DEFS=-DHAVE_CONFIG_H], [AC_OUTPUT_MAKE_DEFS()])

# Without the "./", some shells look in PATH for config.status.
: ${CONFIG_STATUS=./config.status}

echo creating $CONFIG_STATUS
rm -f $CONFIG_STATUS
cat > $CONFIG_STATUS <<EOF
#! /bin/sh
# Generated automatically by configure.
# Run this file to recreate the current configuration.
# This directory was configured as follows,
dnl hostname on some systems (SVR3.2, Linux) returns a bogus exit status,
dnl so uname gets run too.
# on host `(hostname || uname -n) 2>/dev/null | sed 1q`:
#
[#] [$]0 [$]ac_configure_args
#
# Compiler output produced by configure, useful for debugging
# configure, is in ./config.log if it exists.

changequote(, )dnl
ac_cs_usage="Usage: $CONFIG_STATUS [--recheck] [--version] [--help]"
changequote([, ])dnl
for ac_option
do
  case "[\$]ac_option" in
  -recheck | --recheck | --rechec | --reche | --rech | --rec | --re | --r)
    echo "running [\$]{CONFIG_SHELL-/bin/sh} [$]0 [$]ac_configure_args --no-create --no-recursion"
    exec [\$]{CONFIG_SHELL-/bin/sh} [$]0 [$]ac_configure_args --no-create --no-recursion ;;
  -version | --version | --versio | --versi | --vers | --ver | --ve | --v)
    echo "$CONFIG_STATUS generated by autoconf version AC_ACVERSION"
    exit 0 ;;
  -help | --help | --hel | --he | --h)
    echo "[\$]ac_cs_usage"; exit 0 ;;
  *) echo "[\$]ac_cs_usage"; exit 1 ;;
  esac
done

ac_given_srcdir=$srcdir
ifdef([AC_PROVIDE_AC_PROG_INSTALL], [ac_given_INSTALL="$INSTALL"
])dnl

changequote(<<, >>)dnl
ifdef(<<AC_LIST_HEADER>>,
<<trap 'rm -fr `echo "$1 AC_LIST_HEADER" | sed "s/:[^ ]*//g"` conftest*; exit 1' 1 2 15>>,
<<trap 'rm -fr `echo "$1" | sed "s/:[^ ]*//g"` conftest*; exit 1' 1 2 15>>)
changequote([, ])dnl
EOF
cat >> $CONFIG_STATUS <<EOF

AC_OUTPUT_FILES($1)
ifdef([AC_LIST_HEADER], [AC_OUTPUT_HEADER(AC_LIST_HEADER)])dnl
ifdef([AC_LIST_LINKS], [AC_OUTPUT_LINKS(AC_LIST_FILES, AC_LIST_LINKS)])dnl
EOF
cat >> $CONFIG_STATUS <<EOF
undivert(AC_DIVERSION_ICMDS)dnl
$3
EOF
cat >> $CONFIG_STATUS <<\EOF
undivert(AC_DIVERSION_CMDS)dnl
$2
exit 0
EOF
chmod +x $CONFIG_STATUS
rm -fr confdefs* $ac_clean_files
test "$no_create" = yes || ${CONFIG_SHELL-/bin/sh} $CONFIG_STATUS || exit 1
dnl config.status should not do recursion.
ifdef([AC_LIST_SUBDIRS], [AC_OUTPUT_SUBDIRS(AC_LIST_SUBDIRS)])dnl
])dnl

dnl Set the DEFS variable to the -D options determined earlier.
dnl This is a subroutine of AC_OUTPUT.
dnl It is called inside configure, outside of config.status.
dnl AC_OUTPUT_MAKE_DEFS()
define(AC_OUTPUT_MAKE_DEFS,
[# Transform confdefs.h into DEFS.
dnl Using a here document instead of a string reduces the quoting nightmare.
# Protect against shell expansion while executing Makefile rules.
# Protect against Makefile macro expansion.
cat > conftest.defs <<\EOF
changequote(<<, >>)dnl
s%<<#define>> \([A-Za-z_][A-Za-z0-9_]*\) *\(.*\)%-D\1=\2%g
s%[ 	`~<<#>>$^&*(){}\\|;'"<>?]%\\&%g
s%\[%\\&%g
s%\]%\\&%g
s%\$%$$%g
changequote([, ])dnl
EOF
DEFS=`sed -f conftest.defs confdefs.h | tr '\012' ' '`
rm -f conftest.defs
])

dnl Do the variable substitutions to create the Makefiles or whatever.
dnl This is a subroutine of AC_OUTPUT.  It is called inside an unquoted
dnl here document whose contents are going into config.status, but
dnl upon returning, the here document is being quoted.
dnl AC_OUTPUT_FILES(FILE...)
define(AC_OUTPUT_FILES,
[# Protect against being on the right side of a sed subst in config.status.
changequote(, )dnl
sed 's/%@/@@/; s/@%/@@/; s/%g\$/@g/; /@g\$/s/[\\\\&%]/\\\\&/g;
 s/@@/%@/; s/@@/@%/; s/@g\$/%g/' > conftest.subs <<\\CEOF
changequote([, ])dnl
dnl These here document variables are unquoted when configure runs
dnl but quoted when config.status runs, so variables are expanded once.
$ac_vpsub
dnl Shell code in configure.in might set extrasub.
$extrasub
dnl Insert the sed substitutions of variables.
undivert(AC_DIVERSION_SED)
CEOF
EOF

cat >> $CONFIG_STATUS <<\EOF

# Split the substitutions into bite-sized pieces for seds with
# small command number limits, like on Digital OSF/1 and HP-UX.
ac_max_sed_cmds=90 # Maximum number of lines to put in a sed script.
ac_file=1 # Number of current file.
ac_beg=1 # First line for current file.
ac_end=$ac_max_sed_cmds # Line after last line for current file.
ac_more_lines=:
ac_sed_cmds=""
while $ac_more_lines; do
  if test $ac_beg -gt 1; then
    sed "1,${ac_beg}d; ${ac_end}q" conftest.subs > conftest.s$ac_file
  else
    sed "${ac_end}q" conftest.subs > conftest.s$ac_file
  fi
  if test ! -s conftest.s$ac_file; then
    ac_more_lines=false
    rm -f conftest.s$ac_file
  else
    if test -z "$ac_sed_cmds"; then
      ac_sed_cmds="sed -f conftest.s$ac_file"
    else
      ac_sed_cmds="$ac_sed_cmds | sed -f conftest.s$ac_file"
    fi
    ac_file=`expr $ac_file + 1`
    ac_beg=$ac_end
    ac_end=`expr $ac_end + $ac_max_sed_cmds`
  fi
done
if test -z "$ac_sed_cmds"; then
  ac_sed_cmds=cat
fi
EOF

cat >> $CONFIG_STATUS <<EOF

CONFIG_FILES=\${CONFIG_FILES-"$1"}
EOF
cat >> $CONFIG_STATUS <<\EOF
for ac_file in .. $CONFIG_FILES; do if test "x$ac_file" != x..; then
changequote(, )dnl
  # Support "outfile[:infile[:infile...]]", defaulting infile="outfile.in".
  case "$ac_file" in
  *:*) ac_file_in=`echo "$ac_file"|sed 's%[^:]*:%%'`
       ac_file=`echo "$ac_file"|sed 's%:.*%%'` ;;
  *) ac_file_in="${ac_file}.in" ;;
  esac

  # Adjust a relative srcdir, top_srcdir, and INSTALL for subdirectories.

  # Remove last slash and all that follows it.  Not all systems have dirname.
  ac_dir=`echo $ac_file|sed 's%/[^/][^/]*$%%'`
changequote([, ])dnl
  if test "$ac_dir" != "$ac_file" && test "$ac_dir" != .; then
    # The file is in a subdirectory.
    test ! -d "$ac_dir" && mkdir "$ac_dir"
    ac_dir_suffix="/`echo $ac_dir|sed 's%^\./%%'`"
    # A "../" for each directory in $ac_dir_suffix.
changequote(, )dnl
    ac_dots=`echo $ac_dir_suffix|sed 's%/[^/]*%../%g'`
changequote([, ])dnl
  else
    ac_dir_suffix= ac_dots=
  fi

  case "$ac_given_srcdir" in
  .)  srcdir=.
      if test -z "$ac_dots"; then top_srcdir=.
      else top_srcdir=`echo $ac_dots|sed 's%/$%%'`; fi ;;
  /*) srcdir="$ac_given_srcdir$ac_dir_suffix"; top_srcdir="$ac_given_srcdir" ;;
  *) # Relative path.
    srcdir="$ac_dots$ac_given_srcdir$ac_dir_suffix"
    top_srcdir="$ac_dots$ac_given_srcdir" ;;
  esac

ifdef([AC_PROVIDE_AC_PROG_INSTALL],
[  case "$ac_given_INSTALL" in
changequote(, )dnl
  [/$]*) INSTALL="$ac_given_INSTALL" ;;
changequote([, ])dnl
  *) INSTALL="$ac_dots$ac_given_INSTALL" ;;
  esac
])dnl

  echo creating "$ac_file"
  rm -f "$ac_file"
  configure_input="Generated automatically from `echo $ac_file_in|sed 's%.*/%%'` by configure."
  case "$ac_file" in
  *Makefile*) ac_comsub="1i\\
# $configure_input" ;;
  *) ac_comsub= ;;
  esac

  ac_file_inputs=`echo $ac_file_in|sed -e "s%^%$ac_given_srcdir/%" -e "s%:% $ac_given_srcdir/%g"`
  sed -e "$ac_comsub
s%@configure_input@%$configure_input%g
s%@srcdir@%$srcdir%g
s%@top_srcdir@%$top_srcdir%g
ifdef([AC_PROVIDE_AC_PROG_INSTALL], [s%@INSTALL@%$INSTALL%g
])dnl
dnl The parens around the eval prevent an "illegal io" in Ultrix sh.
" $ac_file_inputs | (eval "$ac_sed_cmds") > $ac_file
dnl This would break Makefile dependencies.
dnl  if cmp -s $ac_file conftest.out 2>/dev/null; then
dnl    echo "$ac_file is unchanged"
dnl    rm -f conftest.out
dnl   else
dnl     rm -f $ac_file
dnl    mv conftest.out $ac_file
dnl  fi
fi; done
rm -f conftest.s*
])

dnl Create the config.h files from the config.h.in files.
dnl This is a subroutine of AC_OUTPUT.  It is called inside a quoted
dnl here document whose contents are going into config.status.
dnl AC_OUTPUT_HEADER(HEADER-FILE...)
define(AC_OUTPUT_HEADER,
[changequote(<<, >>)dnl
# These sed commands are passed to sed as "A NAME B NAME C VALUE D", where
# NAME is the cpp macro being defined and VALUE is the value it is being given.
#
# ac_d sets the value in "#define NAME VALUE" lines.
ac_dA='s%^\([ 	]*\)#\([ 	]*define[ 	][ 	]*\)'
ac_dB='\([ 	][ 	]*\)[^ 	]*%\1#\2'
ac_dC='\3'
ac_dD='%g'
# ac_u turns "#undef NAME" with trailing blanks into "#define NAME VALUE".
ac_uA='s%^\([ 	]*\)#\([ 	]*\)undef\([ 	][ 	]*\)'
ac_uB='\([ 	]\)%\1#\2define\3'
ac_uC=' '
ac_uD='\4%g'
# ac_e turns "#undef NAME" without trailing blanks into "#define NAME VALUE".
ac_eA='s%^\([ 	]*\)#\([ 	]*\)undef\([ 	][ 	]*\)'
ac_eB='<<$>>%\1#\2define\3'
ac_eC=' '
ac_eD='%g'
changequote([, ])dnl

if test "${CONFIG_HEADERS+set}" != set; then
EOF
dnl Support passing AC_CONFIG_HEADER a value containing shell variables.
cat >> $CONFIG_STATUS <<EOF
  CONFIG_HEADERS="$1"
EOF
cat >> $CONFIG_STATUS <<\EOF
fi
for ac_file in .. $CONFIG_HEADERS; do if test "x$ac_file" != x..; then
changequote(, )dnl
  # Support "outfile[:infile[:infile...]]", defaulting infile="outfile.in".
  case "$ac_file" in
  *:*) ac_file_in=`echo "$ac_file"|sed 's%[^:]*:%%'`
       ac_file=`echo "$ac_file"|sed 's%:.*%%'` ;;
  *) ac_file_in="${ac_file}.in" ;;
  esac
changequote([, ])dnl

  echo creating $ac_file

  rm -f conftest.frag conftest.in conftest.out
  ac_file_inputs=`echo $ac_file_in|sed -e "s%^%$ac_given_srcdir/%" -e "s%:% $ac_given_srcdir/%g"`
  cat $ac_file_inputs > conftest.in

EOF

# Transform confdefs.h into a sed script conftest.vals that substitutes
# the proper values into config.h.in to produce config.h.  And first:
# Protect against being on the right side of a sed subst in config.status.
# Protect against being in an unquoted here document in config.status.
rm -f conftest.vals
dnl Using a here document instead of a string reduces the quoting nightmare.
dnl Putting comments in sed scripts is not portable.
cat > conftest.hdr <<\EOF
changequote(<<, >>)dnl
s/[\\&%]/\\&/g
s%[\\$`]%\\&%g
s%<<#define>> \([A-Za-z_][A-Za-z0-9_]*\) *\(.*\)%${ac_dA}\1${ac_dB}\1${ac_dC}\2${ac_dD}%gp
s%ac_d%ac_u%gp
s%ac_u%ac_e%gp
changequote([, ])dnl
EOF
sed -n -f conftest.hdr confdefs.h > conftest.vals
rm -f conftest.hdr

# This sed command replaces #undef with comments.  This is necessary, for
# example, in the case of _POSIX_SOURCE, which is predefined and required
# on some systems where configure will not decide to define it.
cat >> conftest.vals <<\EOF
changequote(, )dnl
s%^[ 	]*#[ 	]*undef[ 	][ 	]*[a-zA-Z_][a-zA-Z_0-9]*%/* & */%
changequote([, ])dnl
EOF

# Break up conftest.vals because some shells have a limit on
# the size of here documents, and old seds have small limits too.

rm -f conftest.tail
while :
do
  ac_lines=`grep -c . conftest.vals`
  # grep -c gives empty output for an empty file on some AIX systems.
  if test -z "$ac_lines" || test "$ac_lines" -eq 0; then break; fi
  # Write a limited-size here document to conftest.frag.
  echo '  cat > conftest.frag <<CEOF' >> $CONFIG_STATUS
  sed ${ac_max_here_lines}q conftest.vals >> $CONFIG_STATUS
  echo 'CEOF
  sed -f conftest.frag conftest.in > conftest.out
  rm -f conftest.in
  mv conftest.out conftest.in
' >> $CONFIG_STATUS
  sed 1,${ac_max_here_lines}d conftest.vals > conftest.tail
  rm -f conftest.vals
  mv conftest.tail conftest.vals
done
rm -f conftest.vals

dnl Now back to your regularly scheduled config.status.
cat >> $CONFIG_STATUS <<\EOF
  rm -f conftest.frag conftest.h
  echo "/* $ac_file.  Generated automatically by configure.  */" > conftest.h
  cat conftest.in >> conftest.h
  rm -f conftest.in
  if cmp -s $ac_file conftest.h 2>/dev/null; then
    echo "$ac_file is unchanged"
    rm -f conftest.h
  else
    # Remove last slash and all that follows it.  Not all systems have dirname.
  changequote(, )dnl
    ac_dir=`echo $ac_file|sed 's%/[^/][^/]*$%%'`
  changequote([, ])dnl
    if test "$ac_dir" != "$ac_file" && test "$ac_dir" != .; then
      # The file is in a subdirectory.
      test ! -d "$ac_dir" && mkdir "$ac_dir"
    fi
    rm -f $ac_file
    mv conftest.h $ac_file
  fi
fi; done

])

dnl This is a subroutine of AC_OUTPUT.  It is called inside a quoted
dnl here document whose contents are going into config.status.
dnl AC_OUTPUT_LINKS(SOURCE..., DEST...)
define(AC_OUTPUT_LINKS,
[EOF

cat >> $CONFIG_STATUS <<EOF
ac_sources="$1"
ac_dests="$2"
EOF

cat >> $CONFIG_STATUS <<\EOF
srcdir=$ac_given_srcdir
while test -n "$ac_sources"; do
  set $ac_dests; ac_dest=[$]1; shift; ac_dests=[$]*
  set $ac_sources; ac_source=[$]1; shift; ac_sources=[$]*

  echo "linking $srcdir/$ac_source to $ac_dest"

  if test ! -r $srcdir/$ac_source; then
    AC_MSG_ERROR($srcdir/$ac_source: File not found)
  fi
  rm -f $ac_dest

  # Make relative symlinks.
  # Remove last slash and all that follows it.  Not all systems have dirname.
changequote(, )dnl
  ac_dest_dir=`echo $ac_dest|sed 's%/[^/][^/]*$%%'`
changequote([, ])dnl
  if test "$ac_dest_dir" != "$ac_dest" && test "$ac_dest_dir" != .; then
    # The dest file is in a subdirectory.
    test ! -d "$ac_dest_dir" && mkdir "$ac_dest_dir"
    ac_dest_dir_suffix="/`echo $ac_dest_dir|sed 's%^\./%%'`"
    # A "../" for each directory in $ac_dest_dir_suffix.
changequote(, )dnl
    ac_dots=`echo $ac_dest_dir_suffix|sed 's%/[^/]*%../%g'`
changequote([, ])dnl
  else
    ac_dest_dir_suffix= ac_dots=
  fi

  case "$srcdir" in
changequote(, )dnl
  [/$]*) ac_rel_source="$srcdir/$ac_source" ;;
changequote([, ])dnl
  *) ac_rel_source="$ac_dots$srcdir/$ac_source" ;;
  esac

  # Make a symlink if possible; otherwise try a hard link.
  if ln -s $ac_rel_source $ac_dest 2>/dev/null ||
    ln $srcdir/$ac_source $ac_dest; then :
  else
    AC_MSG_ERROR(can not link $ac_dest to $srcdir/$ac_source)
  fi
done
])

dnl This is a subroutine of AC_OUTPUT.
dnl It is called after running config.status.
dnl AC_OUTPUT_SUBDIRS(DIRECTORY...)
define(AC_OUTPUT_SUBDIRS,
[
if test "$no_recursion" != yes; then

  # Remove --cache-file and --srcdir arguments so they do not pile up.
  ac_sub_configure_args=
  ac_prev=
  for ac_arg in $ac_configure_args; do
    if test -n "$ac_prev"; then
      ac_prev=
      continue
    fi
    case "$ac_arg" in
    -cache-file | --cache-file | --cache-fil | --cache-fi \
    | --cache-f | --cache- | --cache | --cach | --cac | --ca | --c)
      ac_prev=cache_file ;;
    -cache-file=* | --cache-file=* | --cache-fil=* | --cache-fi=* \
    | --cache-f=* | --cache-=* | --cache=* | --cach=* | --cac=* | --ca=* | --c=*)
      ;;
    -srcdir | --srcdir | --srcdi | --srcd | --src | --sr)
      ac_prev=srcdir ;;
    -srcdir=* | --srcdir=* | --srcdi=* | --srcd=* | --src=* | --sr=*)
      ;;
    *) ac_sub_configure_args="$ac_sub_configure_args $ac_arg" ;;
    esac
  done

  for ac_config_dir in $1; do

    # Do not complain, so a configure script can configure whichever
    # parts of a large source tree are present.
    if test ! -d $srcdir/$ac_config_dir; then
      continue
    fi

    echo configuring in $ac_config_dir

    case "$srcdir" in
    .) ;;
    *)
      if test -d ./$ac_config_dir || mkdir ./$ac_config_dir; then :;
      else
        AC_MSG_ERROR(can not create `pwd`/$ac_config_dir)
      fi
      ;;
    esac

    ac_popdir=`pwd`
    cd $ac_config_dir

changequote(, )dnl
      # A "../" for each directory in /$ac_config_dir.
      ac_dots=`echo $ac_config_dir|sed -e 's%^\./%%' -e 's%[^/]$%&/%' -e 's%[^/]*/%../%g'`
changequote([, ])dnl

    case "$srcdir" in
    .) # No --srcdir option.  We are building in place.
      ac_sub_srcdir=$srcdir ;;
    /*) # Absolute path.
      ac_sub_srcdir=$srcdir/$ac_config_dir ;;
    *) # Relative path.
      ac_sub_srcdir=$ac_dots$srcdir/$ac_config_dir ;;
    esac

    # Check for guested configure; otherwise get Cygnus style configure.
    if test -f $ac_sub_srcdir/configure; then
      ac_sub_configure=$ac_sub_srcdir/configure
    elif test -f $ac_sub_srcdir/configure.in; then
      ac_sub_configure=$ac_configure
    else
      AC_MSG_WARN(no configuration information is in $ac_config_dir)
      ac_sub_configure=
    fi

    # The recursion is here.
    if test -n "$ac_sub_configure"; then

      # Make the cache file name correct relative to the subdirectory.
      case "$cache_file" in
      /*) ac_sub_cache_file=$cache_file ;;
      *) # Relative path.
        ac_sub_cache_file="$ac_dots$cache_file" ;;
      esac
ifdef([AC_PROVIDE_AC_PROG_INSTALL],
      [  case "$ac_given_INSTALL" in
changequote(, )dnl
        [/$]*) INSTALL="$ac_given_INSTALL" ;;
changequote([, ])dnl
        *) INSTALL="$ac_dots$ac_given_INSTALL" ;;
        esac
])dnl

      echo "[running ${CONFIG_SHELL-/bin/sh} $ac_sub_configure $ac_sub_configure_args --cache-file=$ac_sub_cache_file] --srcdir=$ac_sub_srcdir"
      # The eval makes quoting arguments work.
      if eval ${CONFIG_SHELL-/bin/sh} $ac_sub_configure $ac_sub_configure_args --cache-file=$ac_sub_cache_file --srcdir=$ac_sub_srcdir
      then :
      else
        AC_MSG_ERROR($ac_sub_configure failed for $ac_config_dir)
      fi
    fi

    cd $ac_popdir
  done
fi
])
