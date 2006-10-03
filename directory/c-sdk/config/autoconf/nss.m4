# -*- tab-width: 4; -*-
# Configure paths for NSS
# Public domain - Chris Seawood <cls@seawood.org> 2001-04-05
# Based upon gtk.m4 (also PD) by Owen Taylor

dnl AM_PATH_NSS([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for system NSS, and define NSS_CFLAGS and NSS_LIBS
AC_DEFUN(AM_PATH_NSS,
[dnl

AC_ARG_WITH(nss-prefix,
	[  --with-nss-prefix=PFX  Prefix where NSS is installed],
	nss_config_prefix="$withval",
	nss_config_prefix="")

AC_ARG_WITH(nss-exec-prefix,
	[  --with-nss-exec-prefix=PFX
                          Exec prefix where NSS is installed],
	nss_config_exec_prefix="$withval",
	nss_config_exec_prefix="")

	if test -n "$nss_config_exec_prefix"; then
		nss_config_args="$nss_config_args --exec-prefix=$nss_config_exec_prefix"
		if test -z "$NSS_CONFIG"; then
			NSS_CONFIG=$nss_config_exec_prefix/bin/nss-config
		fi
	fi
	if test -n "$nss_config_prefix"; then
		nss_config_args="$nss_config_args --prefix=$nss_config_prefix"
		if test -z "$NSS_CONFIG"; then
			NSS_CONFIG=$nss_config_prefix/bin/nss-config
		fi
	fi

    if test -z "$NSS_CONFIG" ; then
	    unset ac_cv_path_NSS_CONFIG
	    AC_PATH_PROG(NSS_CONFIG, nss-config, no)
	    min_nss_version=ifelse([$1], ,4.0.0,$1)
	    AC_MSG_CHECKING(for NSS - version >= $min_nss_version (skipping))
    fi

	no_nss=""
	if test "$NSS_CONFIG" = "no"; then
        AC_MSG_CHECKING(nss-config not found, trying pkg-config)
        AC_PATH_PROG(PKG_CONFIG, pkg-config)
        if test -n "$PKG_CONFIG"; then
            if $PKG_CONFIG --exists nss; then
                AC_MSG_CHECKING(using NSS from package nss)
                NSS_CFLAGS=`$PKG_CONFIG --cflags-only-I nss`
                NSS_LIBS=`$PKG_CONFIG --libs-only-L nss`
            elif $PKG_CONFIG --exists mozilla-nss; then
                AC_MSG_CHECKING(using NSS from package mozilla-nss)
                NSS_CFLAGS=`$PKG_CONFIG --cflags-only-I mozilla-nss`
                NSS_LIBS=`$PKG_CONFIG --libs-only-L mozilla-nss`
            else
                AC_MSG_ERROR([system NSS not found])
		        no_nss="yes"
            fi
        else
		    no_nss="yes"
        fi
	else
        AC_MSG_CHECKING(using NSS from $NSS_CONFIG)
		NSS_CFLAGS=`$NSS_CONFIG $nss_config_args --cflags`
		NSS_LIBS=`$NSS_CONFIG $nss_config_args --libs`

		dnl Skip version check for now
		nss_config_major_version=`$NSS_CONFIG $nss_config_args --version | \
			sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
		nss_config_minor_version=`$NSS_CONFIG $nss_config_args --version | \
			sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
		nss_config_micro_version=`$NSS_CONFIG $nss_config_args --version | \
			sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
	fi

	if test -z "$no_nss"; then
		AC_MSG_RESULT(yes)
		ifelse([$2], , :, [$2])     
	else
		AC_MSG_RESULT(no)
	fi


	AC_SUBST(NSS_CFLAGS)
	AC_SUBST(NSS_LIBS)

])

dnl AM_PATH_INTREE_NSS([ROOTPATH, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for in-tree NSS, and define NSS_CFLAGS and NSS_LIBS
AC_DEFUN(AM_PATH_INTREE_NSS,
[
    nsslibpath=`echo $1/*.OBJ/lib | cut -f1 -d' '`
    savedir=`pwd`
    cd $nsslibpath
    abs_nsslibpath=`pwd`
    cd $savedir
    nssincpath=$1/public/nss
    savedir=`pwd`
    cd $nssincpath
    abs_nssincpath=`pwd`
    cd $savedir
    if test -f "$abs_nssincpath/nss.h" ; then
        NSS_CFLAGS="-I$abs_nssincpath"
    fi
    if test -d "$abs_nsslibpath" ; then
        NSS_LIBS="-L$abs_nsslibpath"
    fi
    if test -n "$NSS_CFLAGS" -a -n "$NSS_LIBS" ; then
        AC_MSG_CHECKING(using in-tree NSS from $nssincpath $nsslibpath)
	    AC_SUBST(NSS_CFLAGS)
	    AC_SUBST(NSS_LIBS)
		AC_MSG_RESULT(yes)
    else
        AC_MSG_CHECKING(could not find in-tree NSS in $1)
		AC_MSG_RESULT(no)
    fi
])

dnl AM_PATH_GIVEN_NSS(no args)
dnl Test for --with-nss=path, --with-nss-inc=path, and --with-nss-lib=path
dnl Makes sure the right files/dirs are in the given paths, and sets
dnl NSS_CFLAGS and NSS_LIBS if successful
AC_DEFUN(AM_PATH_GIVEN_NSS,
[
    dnl ========================================================
    dnl = Build libssldap, and use the NSS installed in dist for
    dnl = the crypto.
    dnl ========================================================
    dnl
    AC_MSG_CHECKING(for --with-nss)
    AC_ARG_WITH(nss, 
        [[  --with-nss[=PATH]              Build libssldap, using NSS for crypto - optional PATH is path to NSS package]],
        [   if test "$withval" = "yes"; then
                USE_NSS=1
		        AC_MSG_RESULT(yes)
    	    elif test -d "$withval" -a -d "$withval/lib" -a -d "$withval/include" ; then
                USE_NSS=1
                AC_MSG_RESULT([using $withval])
                NSS_CFLAGS="-I$withval/include"
                NSS_LIBS="-L$withval/lib"
            else
		        AC_MSG_RESULT(no)
                USE_NSS=
    	    fi],
        AC_MSG_RESULT(no))

    # check for --with-nss-inc
    AC_MSG_CHECKING(for --with-nss-inc)
    AC_ARG_WITH(nss-inc, [  --with-nss-inc=PATH        Netscape Portable Runtime (NSS) include file directory],
    [
      if test -f "$withval"/nss.h
      then
        AC_MSG_RESULT([using $withval])
        NSS_CFLAGS="-I$withval"
        USE_NSS=1
      else
        echo
        AC_MSG_ERROR([$withval not found])
      fi
    ],
    AC_MSG_RESULT(no))

    # check for --with-nss-lib
    AC_MSG_CHECKING(for --with-nss-lib)
    AC_ARG_WITH(nss-lib, [  --with-nss-lib=PATH        Netscape Portable Runtime (NSS) library directory],
    [
      if test -d "$withval"
      then
        AC_MSG_RESULT([using $withval])
        NSS_LIBS="-L$withval"
        USE_NSS=1
      else
        echo
        AC_MSG_ERROR([$withval not found])
      fi
    ],
    AC_MSG_RESULT(no))
])
