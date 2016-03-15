dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Usage: MOZ_ZLIB_CHECK([version])

AC_DEFUN([MOZ_ZLIB_CHECK],
[

MOZZLIB=$1

MOZ_ARG_WITH_STRING(system-zlib,
[  --with-system-zlib[=PFX]
                          Use system libz [installed at prefix PFX]],
    ZLIB_DIR=$withval)

if test -z "$MOZ_ZLIB_LIBS$MOZ_ZLIB_CFLAGS$SKIP_LIBRARY_CHECKS"; then
    _SAVE_CFLAGS=$CFLAGS
    _SAVE_LDFLAGS=$LDFLAGS
    _SAVE_LIBS=$LIBS

    if test -n "${ZLIB_DIR}" -a "${ZLIB_DIR}" != "yes"; then
        MOZ_ZLIB_CFLAGS="-I${ZLIB_DIR}/include"
        MOZ_ZLIB_LIBS="-L${ZLIB_DIR}/lib"
        CFLAGS="$MOZ_ZLIB_CFLAGS $CFLAGS"
        LDFLAGS="$MOZ_ZLIB_LIBS $LDFLAGS"
    fi
    if test -z "$ZLIB_DIR" -o "$ZLIB_DIR" = no; then
        MOZ_SYSTEM_ZLIB=
    else
        AC_CHECK_LIB(z, gzread, [MOZ_SYSTEM_ZLIB=1 MOZ_ZLIB_LIBS="$MOZ_ZLIB_LIBS -lz"],
            [MOZ_SYSTEM_ZLIB=])
        if test "$MOZ_SYSTEM_ZLIB" = 1; then
            MOZZLIBNUM=`echo $MOZZLIB | awk -F. changequote(<<, >>)'{printf "0x%x\n", (((<<$>>1 * 16 + <<$>>2) * 16) + <<$>>3) * 16 + <<$>>4}'changequote([, ])`
            AC_TRY_COMPILE([ #include <stdio.h>
                             #include <string.h>
                             #include <zlib.h> ],
                           [ #if ZLIB_VERNUM < $MOZZLIBNUM
                             #error "Insufficient zlib version ($MOZZLIBNUM required)."
                             #endif ],
                           MOZ_SYSTEM_ZLIB=1,
                           AC_MSG_ERROR([Insufficient zlib version for --with-system-zlib ($MOZZLIB required)]))
        fi
    fi
    CFLAGS=$_SAVE_CFLAGS
    LDFLAGS=$_SAVE_LDFLAGS
    LIBS=$_SAVE_LIBS
fi

AC_SUBST_LIST(MOZ_ZLIB_CFLAGS)
AC_SUBST_LIST(MOZ_ZLIB_LIBS)
AC_SUBST(MOZ_SYSTEM_ZLIB)

])
