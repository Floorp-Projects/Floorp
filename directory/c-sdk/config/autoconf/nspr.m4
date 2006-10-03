# -*- tab-width: 4; -*-
# Configure paths for NSPR
# Public domain - Chris Seawood <cls@seawood.org> 2001-04-05
# Based upon gtk.m4 (also PD) by Owen Taylor

dnl AM_PATH_NSPR([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for system NSPR, and define NSPR_CFLAGS and NSPR_LIBS
AC_DEFUN([AM_PATH_NSPR],
[dnl

AC_ARG_WITH(nspr-prefix,
	[  --with-nspr-prefix=PFX  Prefix where NSPR is installed],
	nspr_config_prefix="$withval",
	nspr_config_prefix="")

AC_ARG_WITH(nspr-exec-prefix,
	[  --with-nspr-exec-prefix=PFX
                          Exec prefix where NSPR is installed],
	nspr_config_exec_prefix="$withval",
	nspr_config_exec_prefix="")

	if test -n "$nspr_config_exec_prefix"; then
		nspr_config_args="$nspr_config_args --exec-prefix=$nspr_config_exec_prefix"
		if test -z "$NSPR_CONFIG"; then
			NSPR_CONFIG=$nspr_config_exec_prefix/bin/nspr-config
		fi
	fi
	if test -n "$nspr_config_prefix"; then
		nspr_config_args="$nspr_config_args --prefix=$nspr_config_prefix"
		if test -z "$NSPR_CONFIG"; then
			NSPR_CONFIG=$nspr_config_prefix/bin/nspr-config
		fi
	fi

    if test -z "$NSPR_CONFIG" ; then
	    unset ac_cv_path_NSPR_CONFIG
	    AC_PATH_PROG(NSPR_CONFIG, nspr-config, no)
	    min_nspr_version=ifelse([$1], ,4.0.0,$1)
	    AC_MSG_CHECKING(for NSPR - version >= $min_nspr_version (skipping))
    fi

	no_nspr=""
	if test "$NSPR_CONFIG" = "no"; then
        AC_MSG_CHECKING(nspr-config not found, trying pkg-config)
        AC_PATH_PROG(PKG_CONFIG, pkg-config)
        if test -n "$PKG_CONFIG"; then
            if $PKG_CONFIG --exists nspr; then
                AC_MSG_CHECKING(using NSPR from package nspr)
                NSPR_CFLAGS=`$PKG_CONFIG --cflags-only-I nspr`
                NSPR_LIBS=`$PKG_CONFIG --libs-only-L nspr`
            elif $PKG_CONFIG --exists mozilla-nspr; then
                AC_MSG_CHECKING(using NSPR from package mozilla-nspr)
                NSPR_CFLAGS=`$PKG_CONFIG --cflags-only-I mozilla-nspr`
                NSPR_LIBS=`$PKG_CONFIG --libs-only-L mozilla-nspr`
            else
                AC_MSG_CHECKING([system NSPR not found])
		        no_nspr="yes"
            fi
        else
		    no_nspr="yes"
        fi
	else
        AC_MSG_CHECKING(using NSPR from $NSPR_CONFIG)
		NSPR_CFLAGS=`$NSPR_CONFIG $nspr_config_args --cflags`
		NSPR_LIBS=`$NSPR_CONFIG $nspr_config_args --libs`

		dnl Skip version check for now
		nspr_config_major_version=`$NSPR_CONFIG $nspr_config_args --version | \
			sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
		nspr_config_minor_version=`$NSPR_CONFIG $nspr_config_args --version | \
			sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
		nspr_config_micro_version=`$NSPR_CONFIG $nspr_config_args --version | \
			sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
	fi

	if test -z "$no_nspr"; then
		AC_MSG_RESULT(yes)
		ifelse([$2], , :, [$2])     
	else
		AC_MSG_RESULT(no)
	fi


	AC_SUBST(NSPR_CFLAGS)
	AC_SUBST(NSPR_LIBS)

])

dnl AM_PATH_INTREE_NSPR([ROOTPATH, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for in-tree NSPR, and define NSPR_CFLAGS and NSPR_LIBS
dnl First look for path/*.OBJ/include, then look for path/include
dnl Use the cut in case there is more than one path that matches *.OBJ - just
dnl take the first one
AC_DEFUN(AM_PATH_INTREE_NSPR,
[
    AC_MSG_CHECKING(checking for in-tree NSPR from $1)
    for nsprpath in "$1" "$1"/*.OBJ ; do    
        savedir=`pwd`
        cd $nsprpath
        abs_nsprpath=`pwd`
        cd $savedir
        if test -f "$abs_nsprpath/include/nspr/nspr.h" ; then
            NSPR_CFLAGS="-I$abs_nsprpath/include/nspr"
        elif test -f "$abs_nsprpath/include/nspr.h" ; then
            NSPR_CFLAGS="-I$abs_nsprpath/include"
        fi
        if test -d "$abs_nsprpath/lib" ; then
            NSPR_LIBS="-L$abs_nsprpath/lib"
        fi
        if test -n "$NSPR_CFLAGS" -a -n "$NSPR_LIBS" ; then
            break
        fi
    done
    if test -n "$NSPR_CFLAGS" -a -n "$NSPR_LIBS" ; then
	    AC_SUBST(NSPR_CFLAGS)
	    AC_SUBST(NSPR_LIBS)
		AC_MSG_RESULT(yes)
    else
		AC_MSG_RESULT(no)
    fi    
])

dnl AM_PATH_GIVEN_NSPR(no args)
dnl Test for --with-nspr=path, --with-nspr-inc=path, and --with-nspr-lib=path
dnl Makes sure the right files/dirs are in the given paths, and sets
dnl NSPR_CFLAGS and NSPR_LIBS if successful
AC_DEFUN(AM_PATH_GIVEN_NSPR,
[
    # check for --with-nspr
    AC_MSG_CHECKING(for --with-nspr)
    AC_ARG_WITH(nspr, [  --with-nspr=PATH        Netscape Portable Runtime (NSPR) directory],
    [
        if test "$withval" = "no" ; then
            AC_MSG_RESULT(no)
            no_nspr="yes"
        elif test "$withval" = "yes" ; then
            AC_MSG_RESULT(yes)
            no_nspr="no"
        elif test -f "$withval"/include/nspr.h -a -d "$withval"/lib
        then
            AC_MSG_RESULT([using $withval])
            NSPR_CFLAGS="-I$withval/include"
            NSPR_LIBS="-L$withval/lib"
        else
            echo
            AC_MSG_ERROR([$withval not found])
        fi
    ],
    AC_MSG_RESULT(no))

    # check for --with-nspr-inc
    AC_MSG_CHECKING(for --with-nspr-inc)
    AC_ARG_WITH(nspr-inc, [  --with-nspr-inc=PATH        Netscape Portable Runtime (NSPR) include file directory],
    [
      if test -f "$withval"/nspr.h
      then
        AC_MSG_RESULT([using $withval])
        NSPR_CFLAGS="-I$withval"
      else
        echo
        AC_MSG_ERROR([$withval not found])
      fi
    ],
    AC_MSG_RESULT(no))

    # check for --with-nspr-lib
    AC_MSG_CHECKING(for --with-nspr-lib)
    AC_ARG_WITH(nspr-lib, [  --with-nspr-lib=PATH        Netscape Portable Runtime (NSPR) library directory],
    [
      if test -d "$withval"
      then
        AC_MSG_RESULT([using $withval])
        NSPR_LIBS="-L$withval"
      else
        echo
        AC_MSG_ERROR([$withval not found])
      fi
    ],
    AC_MSG_RESULT(no))
])
