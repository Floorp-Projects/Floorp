dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_SUBCONFIGURE_JEMALLOC], [

if test "$MOZ_BUILD_APP" != js -o -n "$JS_STANDALONE"; then

  # Run jemalloc configure script

  if test -z "$MOZ_SYSTEM_JEMALLOC" -a "$MOZ_MEMORY" && test -n "$MOZ_JEMALLOC4" -o -n "$MOZ_REPLACE_MALLOC"; then
    ac_configure_args="--build=$build --host=$target --enable-stats --with-jemalloc-prefix=je_ --disable-valgrind"
    # We're using memalign for _aligned_malloc in memory/build/mozmemory_wrap.c
    # on Windows, so just export memalign on all platforms.
    ac_configure_args="$ac_configure_args ac_cv_func_memalign=yes"
    if test -n "$MOZ_REPLACE_MALLOC"; then
      # When using replace_malloc, we always want valloc exported from jemalloc.
      ac_configure_args="$ac_configure_args ac_cv_func_valloc=yes"
      if test "${OS_ARCH}" = Darwin; then
        # We also need to enable pointer validation on Mac because jemalloc's
        # zone allocator is not used.
        ac_configure_args="$ac_configure_args --enable-ivsalloc"
      fi
    fi
    if test -n "$MOZ_JEMALLOC4"; then
      case "${OS_ARCH}" in
        WINNT|Darwin)
          # We want jemalloc functions to be kept hidden on both Mac and Windows
          # See memory/build/mozmemory_wrap.h for details.
          ac_configure_args="$ac_configure_args --without-export"
          ;;
      esac
      if test "${OS_ARCH}" = WINNT; then
        # Lazy lock initialization doesn't play well with lazy linking of
        # mozglue.dll on Windows XP (leads to startup crash), so disable it.
        ac_configure_args="$ac_configure_args --disable-lazy-lock"

        # 64-bit Windows builds require a minimum 16-byte alignment.
        if test -n "$HAVE_64BIT_BUILD"; then
          ac_configure_args="$ac_configure_args --with-lg-tiny-min=4"
        fi
      fi
    elif test "${OS_ARCH}" = Darwin; then
      # When building as a replace-malloc lib, disabling the zone allocator
      # forces to use pthread_atfork.
      ac_configure_args="$ac_configure_args --disable-zone-allocator"
    fi
    _MANGLE="malloc posix_memalign aligned_alloc calloc realloc free memalign valloc malloc_usable_size"
    JEMALLOC_WRAPPER=
    if test -z "$MOZ_REPLACE_MALLOC"; then
      case "$OS_ARCH" in
        Linux|DragonFly|FreeBSD|NetBSD|OpenBSD)
          MANGLE=$_MANGLE
          ;;
      esac
    elif test -z "$MOZ_JEMALLOC4"; then
      MANGLE=$_MANGLE
      JEMALLOC_WRAPPER=replace_
    fi
    if test -n "$MANGLE"; then
      MANGLED=
      for mangle in ${MANGLE}; do
        if test -n "$MANGLED"; then
          MANGLED="$mangle:$JEMALLOC_WRAPPER$mangle,$MANGLED"
        else
          MANGLED="$mangle:$JEMALLOC_WRAPPER$mangle"
        fi
      done
      ac_configure_args="$ac_configure_args --with-mangling=$MANGLED"
    fi
    unset CONFIG_FILES
    if test -z "$MOZ_TLS"; then
      ac_configure_args="$ac_configure_args --disable-tls"
    fi
    EXTRA_CFLAGS="$CFLAGS"
    for var in AS CC CXX CPP LD AR RANLIB STRIP CPPFLAGS EXTRA_CFLAGS LDFLAGS; do
      ac_configure_args="$ac_configure_args $var='`eval echo \\${${var}}`'"
    done
    # Force disable DSS support in jemalloc.
    ac_configure_args="$ac_configure_args ac_cv_func_sbrk=false"

    # Make Linux builds munmap freed chunks instead of recycling them.
    ac_configure_args="$ac_configure_args --enable-munmap"

    # Disable cache oblivious behavior that appears to have a performance
    # impact on Firefox.
    ac_configure_args="$ac_configure_args --disable-cache-oblivious"

    if ! test -e memory/jemalloc; then
      mkdir -p memory/jemalloc
    fi

    # jemalloc's configure runs git to determine the version. But when building
    # from a gecko git clone, the git commands it uses is going to pick gecko's
    # information, not jemalloc's, which is useless. So pretend we don't have git
    # at all. That will make jemalloc's configure pick the in-tree VERSION file.
    (PATH="$srcdir/memory/jemalloc/helper:$PATH";
    AC_OUTPUT_SUBDIRS(memory/jemalloc/src)
    ) || exit 1
    ac_configure_args="$_SUBDIR_CONFIG_ARGS"
  fi

fi

])
