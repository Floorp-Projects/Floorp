# Configure paths for LIBIDL

dnl AM_PATH_LIBIDL([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, MODULES]]]])
dnl Test for LIBIDL, and define LIBIDL_CFLAGS and LIBIDL_LIBS
dnl
AC_DEFUN(AM_PATH_LIBIDL,
[dnl 
dnl Get the cflags and libraries from the libIDL-config script
dnl
AC_ARG_WITH(libIDL-prefix,[  --with-libIDL-prefix=PFX   Prefix where libIDL is installed (optional)],
            libIDL_config_prefix="$withval", libIDL_config_prefix="")
AC_ARG_WITH(libIDL-exec-prefix,[  --with-libIDL-exec-prefix=PFX Exec prefix where libIDL is installed (optional)],
            libIDL_config_exec_prefix="$withval", libIDL_config_exec_prefix="")
AC_ARG_ENABLE(libIDLtest, [  --disable-libIDLtest       Do not try to compile and run a test libIDL program],
		    , enable_libIDLtest=yes)

  if test x$libIDL_config_exec_prefix != x ; then
     libIDL_config_args="$libIDL_config_args --exec-prefix=$libIDL_config_exec_prefix"
     if test x${LIBIDL_CONFIG+set} != xset ; then
        LIBIDL_CONFIG=$libIDL_config_exec_prefix/bin/libIDL-config
     fi
  fi
  if test x$libIDL_config_prefix != x ; then
     libIDL_config_args="$libIDL_config_args --prefix=$libIDL_config_prefix"
     if test x${LIBIDL_CONFIG+set} != xset ; then
        LIBIDL_CONFIG=$libIDL_config_prefix/bin/libIDL-config
     fi
  fi

  AM_PATH_GLIB(1.2.0)

  AC_PATH_PROG(LIBIDL_CONFIG, libIDL-config, no)
  min_libIDL_version=ifelse([$1], ,0.6.0,$1)
  AC_MSG_CHECKING(for libIDL - version >= $min_libIDL_version)
  no_libIDL=""
  if test "$LIBIDL_CONFIG" = "no" ; then
    no_libIDL=yes
  else
    LIBIDL_CFLAGS=`$LIBIDL_CONFIG $libIDL_config_args --cflags`
    LIBIDL_LIBS=`$LIBIDL_CONFIG $libIDL_config_args --libs`
    libIDL_config_major_version=`$LIBIDL_CONFIG $libIDL_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    libIDL_config_minor_version=`$LIBIDL_CONFIG $libIDL_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    libIDL_config_micro_version=`$LIBIDL_CONFIG $libIDL_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_libIDLtest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $LIBIDL_CFLAGS"
      LIBS="$LIBIDL_LIBS $LIBS"
dnl
dnl Now check if the installed LIBIDL is sufficiently new.
dnl
      rm -f conf.libIDLtest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <libIDL/IDL.h>

int 
main ()
{
  int major, minor, micro;
  int libIDL_major_version;
  int libIDL_minor_version;
  int libIDL_micro_version;
  char *tmp_version;

  system ("touch conf.libIDLtest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = g_strdup ("$min_libIDL_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_libIDL_version");
     exit(1);
  }
  tmp_version = g_strdup (IDL_get_libver_string ());
  if (sscanf(tmp_version, "%d.%d.%d",
	     &libIDL_major_version,
	     &libIDL_minor_version,
	     &libIDL_micro_version) != 3) {
     printf("%s, bad version string\n", tmp_version);
     exit(1);
  }

  if ((libIDL_major_version != $libIDL_config_major_version) ||
      (libIDL_minor_version != $libIDL_config_minor_version) ||
      (libIDL_micro_version != $libIDL_config_micro_version))
    {
      printf("\n*** 'libIDL-config --version' returned %d.%d.%d, but libIDL (%d.%d.%d)\n", 
             $libIDL_config_major_version, $libIDL_config_minor_version, $libIDL_config_micro_version,
             libIDL_major_version, libIDL_minor_version, libIDL_micro_version);
      printf ("*** was found! If libIDL-config was correct, then it is best\n");
      printf ("*** to remove the old version of LIBIDL. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If libIDL-config was wrong, set the environment variable LIBIDL_CONFIG\n");
      printf("*** to point to the correct copy of libIDL-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
    } 
  else if ((libIDL_major_version != LIBIDL_MAJOR_VERSION) ||
	   (libIDL_minor_version != LIBIDL_MINOR_VERSION) ||
           (libIDL_micro_version != LIBIDL_MICRO_VERSION))
    {
      printf("\n*** libIDL header files (version %d.%d.%d) do not match\n",
	     LIBIDL_MAJOR_VERSION, LIBIDL_MINOR_VERSION, LIBIDL_MICRO_VERSION);
      printf("*** library (version %d.%d.%d)\n",
	     libIDL_major_version, libIDL_minor_version, libIDL_micro_version);
    }
  else
    {
      if ((libIDL_major_version > major) ||
        ((libIDL_major_version == major) && (libIDL_minor_version > minor)) ||
        ((libIDL_major_version == major) && (libIDL_minor_version == minor) && (libIDL_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of libIDL (%d.%d.%d) was found.\n",
               libIDL_major_version, libIDL_minor_version, libIDL_micro_version);
        printf("*** You need at least libIDL version %d.%d.%d.\n",
               major, minor, micro);
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the libIDL-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of libIDL, but you can also set the LIBIDL_CONFIG environment to point to the\n");
        printf("*** correct copy of libIDL-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_libIDL=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_libIDL" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$LIBIDL_CONFIG" = "no" ; then
       echo "*** The libIDL-config script installed by libIDL could not be found"
       echo "*** If libIDL was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the LIBIDL_CONFIG environment variable to the"
       echo "*** full path to libIDL-config."
     else
       if test -f conf.libIDLtest ; then
        :
       else
          echo "*** Could not run libIDL test program, checking why..."
          CFLAGS="$CFLAGS $LIBIDL_CFLAGS"
          LIBS="$LIBS $LIBIDL_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <stdlib.h>
#include <libIDL/IDL.h>
],      [ return IDL_get_libver_string ? 1 : 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding libIDL or finding the wrong"
          echo "*** version of LIBIDL. If it is not finding libIDL, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means libIDL was incorrectly installed"
          echo "*** or that you have moved libIDL since it was installed. In the latter case, you"
          echo "*** may want to edit the libIDL-config script: $LIBIDL_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     LIBIDL_CFLAGS=""
     LIBIDL_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(LIBIDL_CFLAGS)
  AC_SUBST(LIBIDL_LIBS)
  rm -f conf.libIDLtest
])
