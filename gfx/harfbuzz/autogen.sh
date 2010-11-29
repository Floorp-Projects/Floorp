#!/bin/sh
# Run this to generate all the initial makefiles, etc.
set -e

ARGV0=$0

# Allow invocation from a separate build directory; in that case, we change
# to the source directory to run the auto*, then change back before running configure
srcdir=`dirname $ARGV0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

PACKAGE=harfbuzz

LIBTOOLIZE_FLAGS="--copy --force --automake"
ACLOCAL_FLAGS=""
AUTOHEADER=${AUTOHEADER-autoheader}
GTKDOCIZE_FLAGS="--copy"
GTKDOCIZE=${GTKDOCIZE-gtkdocize}
AUTOMAKE_FLAGS="--add-missing --gnu -Wall"
AUTOCONF=${AUTOCONF-autoconf}

CONFIGURE_AC=
test -f configure.ac && CONFIGURE_AC=configure.ac

if test "X$CONFIGURE_AC" = X; then
  echo "$ARGV0: ERROR: No $srcdir/configure.in or $srcdir/configure.ac found."
  exit 1
fi

extract_version() {
	grep "^ *$1" "$CONFIGURE_AC" | sed 's/.*(\[*\([^])]*\)]*).*/\1/'
}

autoconf_min_vers=`extract_version AC_PREREQ`
automake_min_vers=`extract_version AM_INIT_AUTOMAKE`
libtoolize_min_vers=`extract_version AC_PROG_LIBTOOL`
aclocal_min_vers=$automake_min_vers


# Not all echo versions allow -n, so we check what is possible. This test is
# based on the one in autoconf.
case `echo "testing\c"; echo 1,2,3`,`echo -n testing; echo 1,2,3` in
  *c*,-n*) ECHO_N= ;;
  *c*,*  ) ECHO_N=-n ;;
  *)       ECHO_N= ;;
esac


# some terminal codes ...
boldface="`tput bold 2>/dev/null || true`"
normal="`tput sgr0 2>/dev/null || true`"
printbold() {
    echo $ECHO_N "$boldface"
    echo "$@"
    echo $ECHO_N "$normal"
}
printerr() {
    echo "$@" >&2
}


# Usage:
#     compare_versions MIN_VERSION ACTUAL_VERSION
# returns true if ACTUAL_VERSION >= MIN_VERSION
compare_versions() {
    ch_min_version=$1
    ch_actual_version=$2
    ch_status=0
    IFS="${IFS=         }"; ch_save_IFS="$IFS"; IFS="."
    set $ch_actual_version
    for ch_min in $ch_min_version; do
        ch_cur=`echo $1 | sed 's/[^0-9].*$//'`; shift # remove letter suffixes
        if [ -z "$ch_min" ]; then break; fi
        if [ -z "$ch_cur" ]; then ch_status=1; break; fi
        if [ $ch_cur -gt $ch_min ]; then break; fi
        if [ $ch_cur -lt $ch_min ]; then ch_status=1; break; fi
    done
    IFS="$ch_save_IFS"
    return $ch_status
}

# Usage:
#     version_check PACKAGE VARIABLE CHECKPROGS MIN_VERSION SOURCE
# checks to see if the package is available
version_check() {
    vc_package=$1
    vc_variable=$2
    vc_checkprogs=$3
    vc_min_version=$4
    vc_source=$5
    vc_status=1

    vc_checkprog=`eval echo "\\$$vc_variable"`
    if [ -n "$vc_checkprog" ]; then
	printbold "using $vc_checkprog for $vc_package"
	return 0
    fi

    printbold "checking for $vc_package >= $vc_min_version..."
    for vc_checkprog in $vc_checkprogs; do
	echo $ECHO_N "  testing $vc_checkprog... "
	if $vc_checkprog --version < /dev/null > /dev/null 2>&1; then
	    vc_actual_version=`$vc_checkprog --version | head -n 1 | \
                               sed 's/^.*[ 	]\([0-9.]*[a-z]*\).*$/\1/'`
	    if compare_versions $vc_min_version $vc_actual_version; then
		echo "found $vc_actual_version"
		# set variable
		eval "$vc_variable=$vc_checkprog"
		vc_status=0
		break
	    else
		echo "too old (found version $vc_actual_version)"
	    fi
	else
	    echo "not found."
	fi
    done
    if [ "$vc_status" != 0 ]; then
	printerr "***Error***: You must have $vc_package >= $vc_min_version installed"
	printerr "  to build $PROJECT.  Download the appropriate package for"
	printerr "  from your distribution or get the source tarball at"
        printerr "    $vc_source"
	printerr
    fi
    return $vc_status
}


version_check autoconf AUTOCONF $AUTOCONF $autoconf_min_vers \
    "http://ftp.gnu.org/pub/gnu/autoconf/autoconf-${autoconf_min_vers}.tar.gz" || DIE=1

#
# Hunt for an appropriate version of automake and aclocal; we can't
# assume that 'automake' is necessarily the most recent installed version
#
# We check automake first to allow it to be a newer version than we know about.
#
version_check automake AUTOMAKE "$AUTOMAKE automake automake-1.10 automake-1.9 automake-1.8 automake-1.7" $automake_min_vers \
    "http://ftp.gnu.org/pub/gnu/automake/automake-${automake_min_vers}.tar.gz" || DIE=1
ACLOCAL=`echo $AUTOMAKE | sed s/automake/aclocal/`


version_check libtool LIBTOOLIZE "$LIBTOOLIZE glibtoolize libtoolize" $libtoolize_min_vers \
    "http://ftp.gnu.org/pub/gnu/libtool/libtool-${libtool_min_vers}.tar.gz" || DIE=1

if test -n "$DIE"; then
  exit 1
fi


if test -z "$*"; then
  echo "$ARGV0:	Note: \`./configure' will be run with no arguments."
  echo "		If you wish to pass any to it, please specify them on the"
  echo "		\`$0' command line."
  echo
fi

do_cmd() {
    echo "$ARGV0: running \`$@'"
    $@
}

do_cmd $LIBTOOLIZE $LIBTOOLIZE_FLAGS

do_cmd $ACLOCAL $ACLOCAL_FLAGS

do_cmd $AUTOHEADER

touch ChangeLog

#do_cmd $GTKDOCIZE $GTKDOCIZE_FLAGS

do_cmd $AUTOMAKE $AUTOMAKE_FLAGS

do_cmd $AUTOCONF

cd "$ORIGDIR" || exit 1

rm -f config.cache

do_cmd $srcdir/configure \
	${1+"$@"} && echo "Now type \`make' to compile $PROJECT." || exit 1
