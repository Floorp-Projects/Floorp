# -*- tab-width: 4; -*-
# Configure paths for SVRCORE
# Public domain - Rich Megginson <richm@stanfordalumni.org> 2005-12-21
# Based upon nspr.m4 (also PD) by Chris Seawood

dnl AM_PATH_INTREE_SVRCORE([ROOTPATH, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for in-tree SVRCORE, and define SVRCORE_CFLAGS and SVRCORE_LIBS
AC_DEFUN(AM_PATH_INTREE_SVRCORE,
[
    if test -n "$HAVE_SVRCORE" ; then
        svrcorelibpath=`echo $1/*.OBJ/lib | cut -f1 -d' '`
        savedir=`pwd`
        cd $svrcorelibpath
        abs_svrcorelibpath=`pwd`
        cd $savedir
        svrcoreincpath=$1/public/svrcore
        savedir=`pwd`
        cd $svrcoreincpath
        abs_svrcoreincpath=`pwd`
        cd $savedir
        if test -f "$abs_svrcoreincpath/svrcore.h" ; then
            SVRCORE_CFLAGS="-I$abs_svrcoreincpath"
        fi
        if test -d "$abs_svrcorelibpath" ; then
            SVRCORE_LIBS="-L$abs_svrcorelibpath"
        fi
        if test -n "$SVRCORE_CFLAGS" -a -n "$SVRCORE_LIBS" ; then
            AC_MSG_CHECKING(using in-tree SVRCORE from $svrcoreincpath $svrcorelibpath)
            AC_SUBST(SVRCORE_CFLAGS)
            AC_SUBST(SVRCORE_LIBS)
            AC_MSG_RESULT(yes)
        elif test -n "$HAVE_SVRCORE" ; then
            AC_MSG_CHECKING(could not find in-tree SVRCORE in $1)
            AC_MSG_RESULT(no)
        else
        # If user didn't ask for it, don't complain (really!)
            AC_MSG_RESULT(no)
        fi
    fi
])

dnl AM_PATH_GIVEN_SVRCORE(no args)
dnl Test for --with-svrcore=path, --with-svrcore-inc=path, and --with-svrcore-lib=path
dnl Makes sure the right files/dirs are in the given paths, and sets
dnl SVRCORE_CFLAGS and SVRCORE_LIBS if successful
AC_DEFUN(AM_PATH_GIVEN_SVRCORE,
[
    AC_MSG_CHECKING(for --with-svrcore)
    AC_ARG_WITH(svrcore, 
        [[  --with-svrcore[=PATH]              Use svrcore - optional PATH is path to svrcore lib and include dirs]],
        [   if test "$withval" = "yes"; then
                HAVE_SVRCORE=1
                AC_MSG_RESULT(yes)
    	    elif test -n "$withval" -a -d "$withval" -a -d "$withval/lib" -a -f "$withval/include/svrcore.h" ; then
                HAVE_SVRCORE=1
                AC_MSG_RESULT([using $withval])
                SVRCORE_CFLAGS="-I$withval/include"
                SVRCORE_LIBS="-L$withval/lib"
    	    fi], HAVE_SVRCORE=)

    # check for --with-svrcore-inc
    AC_ARG_WITH(svrcore-inc, [  --with-svrcore-inc=PATH        svrcore include file directory],
    [
      if test -n "$withval" -a -f "$withval"/svrcore.h
      then
        AC_MSG_RESULT([using $withval])
        SVRCORE_CFLAGS="-I$withval"
      else
        echo
        AC_MSG_ERROR([$withval not found])
      fi
    ],
    )

    # check for --with-svrcore-lib
    AC_ARG_WITH(svrcore-lib, [  --with-svrcore-lib=PATH        svrcore library directory],
    [
      if test -n "$withval" -a -d "$withval"
      then
        AC_MSG_RESULT([using $withval])
        SVRCORE_LIBS="-L$withval"
      else
        echo
        AC_MSG_ERROR([$withval not found])
      fi
    ],
    )

    if test -z "$SVRCORE_CFLAGS" -o -z "$SVRCORE_LIBS" ; then
        AC_MSG_RESULT(no)
    else
        HAVE_SVRCORE=1
    fi
])

dnl AM_PATH_SVRCORE([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for system SVRCORE, and define SVRCORE_CFLAGS and SVRCORE_LIBS
AC_DEFUN(AM_PATH_SVRCORE,
[dnl

    if test -n "$HAVE_SVRCORE" ; then
        no_svrcore=""
        AC_MSG_CHECKING(Trying pkg-config svrcore)
        AC_PATH_PROG(PKG_CONFIG, pkg-config)
        if test -n "$PKG_CONFIG"; then
            if $PKG_CONFIG --exists svrcore-devel; then
                AC_MSG_CHECKING(using SVRCORE from package svrcore)
                SVRCORE_CFLAGS=`$PKG_CONFIG --cflags-only-I svrcore-devel`
                SVRCORE_LIBS=`$PKG_CONFIG --libs-only-L svrcore-devel`
            else
                no_svrcore="yes"
            fi
        else
            no_svrcore="yes"
        fi

        if test -z "$no_svrcore"; then
            AC_MSG_RESULT(yes)
            ifelse([$2], , :, [$2])     
        else
            AC_MSG_RESULT(no)
        fi


        AC_SUBST(SVRCORE_CFLAGS)
        AC_SUBST(SVRCORE_LIBS)
    fi
])
