# Configure paths for gdk-pixbuf
# Elliot Lee 2000-01-10
# stolen from Raph Levien 98-11-18
# stolen from Manish Singh    98-9-30
# stolen back from Frank Belew
# stolen from Manish Singh
# Shamelessly stolen from Owen Taylor

dnl AM_PATH_GDK_PIXBUF([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for GDK_PIXBUF, and define GDK_PIXBUF_CFLAGS and GDK_PIXBUF_LIBS
dnl
AC_DEFUN(AM_PATH_GDK_PIXBUF,
[dnl 
dnl Get the cflags and libraries from the gdk-pixbuf-config script
dnl
AC_ARG_WITH(gdk-pixbuf-prefix,[  --with-gdk-pixbuf-prefix=PFX   Prefix where GDK_PIXBUF is installed (optional)],
            gdk_pixbuf_prefix="$withval", gdk_pixbuf_prefix="")
AC_ARG_WITH(gdk-pixbuf-exec-prefix,[  --with-gdk-pixbuf-exec-prefix=PFX Exec prefix where GDK_PIXBUF is installed (optional)],
            gdk_pixbuf_exec_prefix="$withval", gdk_pixbuf_exec_prefix="")
AC_ARG_ENABLE(gdk_pixbuftest, [  --disable-gdk_pixbuftest       Do not try to compile and run a test GDK_PIXBUF program],
		    , enable_gdk_pixbuftest=yes)

  if test x$gdk_pixbuf_exec_prefix != x ; then
     gdk_pixbuf_args="$gdk_pixbuf_args --exec-prefix=$gdk_pixbuf_exec_prefix"
     if test x${GDK_PIXBUF_CONFIG+set} = xset ; then
        GDK_PIXBUF_CONFIG=$gdk_pixbuf_exec_prefix/gdk-pixbuf-config
     fi
  fi
  if test x$gdk_pixbuf_prefix != x ; then
     gdk_pixbuf_args="$gdk_pixbuf_args --prefix=$gdk_pixbuf_prefix"
     if test x${GDK_PIXBUF_CONFIG+set} = xset ; then
        GDK_PIXBUF_CONFIG=$gdk_pixbuf_prefix/bin/gdk-pixbuf-config
     fi
  fi

  AC_PATH_PROG(GDK_PIXBUF_CONFIG, gdk-pixbuf-config, no)
  min_gdk_pixbuf_version=ifelse([$1], ,0.2.5,$1)
  AC_MSG_CHECKING(for GDK_PIXBUF - version >= $min_gdk_pixbuf_version)
  no_gdk_pixbuf=""
  if test "$GDK_PIXBUF_CONFIG" = "no" ; then
    no_gdk_pixbuf=yes
  else
    GDK_PIXBUF_CFLAGS=`$GDK_PIXBUF_CONFIG $gdk_pixbufconf_args --cflags`
    GDK_PIXBUF_LIBS=`$GDK_PIXBUF_CONFIG $gdk_pixbufconf_args --libs`

    gdk_pixbuf_major_version=`$GDK_PIXBUF_CONFIG $gdk_pixbuf_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    gdk_pixbuf_minor_version=`$GDK_PIXBUF_CONFIG $gdk_pixbuf_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    gdk_pixbuf_micro_version=`$GDK_PIXBUF_CONFIG $gdk_pixbuf_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_gdk_pixbuftest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GDK_PIXBUF_CFLAGS"
      LIBS="$LIBS $GDK_PIXBUF_LIBS"
dnl
dnl Now check if the installed GDK_PIXBUF is sufficiently new. (Also sanity
dnl checks the results of gdk-pixbuf-config to some extent
dnl
      rm -f conf.gdk_pixbuftest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

char*
my_strdup (char *str)
{
  char *new_str;
  
  if (str)
    {
      new_str = malloc ((strlen (str) + 1) * sizeof(char));
      strcpy (new_str, str);
    }
  else
    new_str = NULL;
  
  return new_str;
}

int main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.gdk_pixbuftest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = my_strdup("$min_gdk_pixbuf_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_gdk_pixbuf_version");
     exit(1);
   }

   if (($gdk_pixbuf_major_version > major) ||
      (($gdk_pixbuf_major_version == major) && ($gdk_pixbuf_minor_version > minor)) ||
      (($gdk_pixbuf_major_version == major) && ($gdk_pixbuf_minor_version == minor) && ($gdk_pixbuf_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'gdk-pixbuf-config --version' returned %d.%d.%d, but the minimum version\n", $gdk_pixbuf_major_version, $gdk_pixbuf_minor_version, $gdk_pixbuf_micro_version);
      printf("*** of GDK_PIXBUF required is %d.%d.%d. If gdk-pixbuf-config is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If gdk-pixbuf-config was wrong, set the environment variable GDK_PIXBUF_CONFIG\n");
      printf("*** to point to the correct copy of gdk-pixbuf-config, and remove the file\n");
      printf("*** config.cache before re-running configure\n");
      return 1;
    }
}

],, no_gdk_pixbuf=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_gdk_pixbuf" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GDK_PIXBUF_CONFIG" = "no" ; then
       echo "*** The gdk-pixbuf-config script installed by GDK_PIXBUF could not be found"
       echo "*** If GDK_PIXBUF was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GDK_PIXBUF_CONFIG environment variable to the"
       echo "*** full path to gdk-pixbuf-config."
     else
       if test -f conf.gdk_pixbuftest ; then
        :
       else
          echo "*** Could not run GDK_PIXBUF test program, checking why..."
          CFLAGS="$CFLAGS $GDK_PIXBUF_CFLAGS"
          LIBS="$LIBS $GDK_PIXBUF_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GDK_PIXBUF or finding the wrong"
          echo "*** version of GDK_PIXBUF. If it is not finding GDK_PIXBUF, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GDK_PIXBUF was incorrectly installed"
          echo "*** or that you have moved GDK_PIXBUF since it was installed. In the latter case, you"
          echo "*** may want to edit the gdk-pixbuf-config script: $GDK_PIXBUF_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GDK_PIXBUF_CFLAGS=""
     GDK_PIXBUF_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GDK_PIXBUF_CFLAGS)
  AC_SUBST(GDK_PIXBUF_LIBS)
  rm -f conf.gdk_pixbuftest
])
