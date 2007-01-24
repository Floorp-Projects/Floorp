# -*- tab-width: 4; -*-
# Configure paths for SASL
# Public domain - Nathan Kinder <nkinder@redhat.com> 2006-06-26
# Based upon svrcore.m4 (also PD) by Rich Megginson <richm@stanfordalumni.org>

AC_DEFUN(AM_PATH_GIVEN_SASL,
[
dnl ========================================================
dnl = sasl is used to support various authentication mechanisms
dnl = such as DIGEST-MD5 and GSSAPI.
dnl ========================================================
dnl ========================================================
dnl = Use the sasl libraries on the system (assuming it exists)
dnl ========================================================
AC_MSG_CHECKING(for --with-sasl)
AC_ARG_WITH(sasl,
    [[  --with-sasl[=PATH]   Use system installed sasl - optional path for sasl]],
    dnl = Look in the standard system locations
    [
      if test "$withval" = "yes"; then
        AC_MSG_RESULT(yes)
        HAVE_SASL=1

        dnl = Check for sasl.h in the normal locations
        if test -f /usr/include/sasl/sasl.h; then
          SASL_CFLAGS="-I/usr/include/sasl"
        elif test -f /usr/include/sasl.h; then
          SASL_CFLAGS="-I/usr/include"
        else
          AC_MSG_ERROR(sasl.h not found)
        fi

      dnl = Check the user provided location
      elif test -d "$withval" -a -d "$withval/lib" -a -d "$withval/include" ; then
        AC_MSG_RESULT([using $withval])
        HAVE_SASL=1

        if test -f "$withval/include/sasl/sasl.h"; then
          SASL_CFLAGS="-I$withval/include/sasl"
        elif test -f "$withval/include/sasl.h"; then
          SASL_CFLAGS="-I$withval/include"
        else
          AC_MSG_ERROR(sasl.h not found)
        fi

        SASL_LIBS="-L$withval/lib"
      else
        AC_MSG_RESULT(yes)
        AC_MSG_ERROR([sasl not found in $withval])
      fi
    ],
    AC_MSG_RESULT(no))

AC_MSG_CHECKING(for --with-sasl-inc)
AC_ARG_WITH(sasl-inc,
    [[  --with-sasl-inc=PATH   SASL include file directory]],
    [
      if test -f "$withval"/sasl.h; then
        AC_MSG_RESULT([using $withval])
        HAVE_SASL=1
        SASL_CFLAGS="-I$withval"
      else
        echo
        AC_MSG_ERROR([$withval/sasl.h not found])
      fi
    ],
    AC_MSG_RESULT(no))

AC_MSG_CHECKING(for --with-sasl-lib)
AC_ARG_WITH(sasl-lib,
    [[  --with-sasl-lib=PATH   SASL library directory]],
    [
      if test -d "$withval"; then
        AC_MSG_RESULT([using $withval])
        HAVE_SASL=1
        SASL_LIBS="-L$withval"
      else
        echo
        AC_MSG_ERROR([$withval not found])
      fi
    ],
    AC_MSG_RESULT(no))

# check for sasl
if test -n "$_WIN32_MSVC" -a -n "$USE_WINDOWS_PATHS" ; then
    if test -n "$SASL_CFLAGS" -a -z "$SKIP_CYGWIN_FIXUP" ; then
        path=`echo $SASL_CFLAGS | sed -e 's/^-I//'`
        path=`cygpath -m $path`
        SASL_CFLAGS="/I$path"
	fi
    if test -n "$SASL_LIBS" -a -z "$SKIP_CYGWIN_FIXUP" ; then
        path=`echo $SASL_LIBS | sed -e 's/^-L//'`
        path=`cygpath -m $path`
        SASL_LIBS="/LIBPATH:$path"
    fi
	special_sasl_check_lib=1
fi
# set ldflags to point to where the user told us to find the sasl libs,
# if any - otherwise it will just use the default location (e.g. /usr/lib)
# the way AC_CHECK_LIB works is it actually attempts to compile and link
# a test program - that's why we need to set LDFLAGS
SAVE_LDFLAGS=$LDFLAGS
if test -n "$SASL_LIBS" ; then
    LDFLAGS="$LDFLAGS $SASL_LIBS"
fi

AC_CHECK_FUNC(getaddrinfo,,[
	AC_CHECK_LIB(socket, getaddrinfo, [LIBS="-lsocket -lnsl $LIBS"])])

if test -n "$special_sasl_check_lib" ; then
    # use this to override the ac_link LIBS used by AC_CHECK_LIB
    # ac_link puts conftest.c betwen $LDFLAGS and $LIBS, but we
	# have to use /LIBPATH which must come after the /link directive
	# however, anything after /link on the command line is assumed to
	# be a linker directive, and conftest.c is not valid there
	# we want to end up with cl.exe .... conftest.c /link /LIBPATH:foo libsasl.lib
	LDFLAGS=
	SAVE_LIBS="$LIBS"
	LIBS="/link $SASL_LIBS sasl32.lib"
	AC_CHECK_LIB([sasl32], [sasl_client_init], [sasl_lib=sasl32.lib])
	if test -z "$sasl_lib" ; then
		LIBS="/link $SASL_LIBS libsasl2.lib"
		AC_CHECK_LIB([sasl2], [sasl_client_init], [sasl_lib=libsasl2.lib])
	fi
	if test -z "$sasl_lib" ; then
		LIBS="/link $SASL_LIBS libsasl.lib"
		AC_CHECK_LIB([sasl], [sasl_client_init], [sasl_lib=libsasl.lib])
	fi
	LIBS="$SAVE_LIBS"
else
	AC_CHECK_LIB([sasl2], [sasl_client_init], [sasl_lib=-lsasl2],
    	         AC_CHECK_LIB([sasl], [sasl_client_init], [sasl_lib=-lsasl]))
fi

SASL_LIBS="$SASL_LIBS $sasl_lib"
LDFLAGS=$SAVE_LDFLAGS

AC_SUBST(SASL_LIBS)
AC_SUBST(SASL_CFLAGS)
AC_SUBST(HAVE_SASL)

if test -n "$HAVE_SASL"; then
  AC_DEFINE(HAVE_SASL)
  AC_DEFINE(HAVE_SASL_OPTIONS)
  AC_DEFINE(LDAP_SASLIO_HOOKS)
fi
])
