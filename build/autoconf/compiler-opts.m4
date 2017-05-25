dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Add compiler specific options

AC_DEFUN([MOZ_DEFAULT_COMPILER],
[
dnl set DEVELOPER_OPTIONS early; MOZ_DEFAULT_COMPILER is usually the first non-setup directive
  if test -z "$MOZILLA_OFFICIAL"; then
    DEVELOPER_OPTIONS=1
  fi
  MOZ_ARG_ENABLE_BOOL(release,
  [  --enable-release        Build with more conservative, release engineering-oriented options.
                          This may slow down builds.],
      DEVELOPER_OPTIONS=,
      DEVELOPER_OPTIONS=1)

dnl Default to MSVC for win32 and gcc-4.2 for darwin
dnl ==============================================================
if test -z "$CROSS_COMPILE"; then
case "$target" in
*-mingw*)
    if test -z "$CPP"; then CPP="$CC -E -nologo"; fi
    if test -z "$CXXCPP"; then CXXCPP="$CXX -TP -E -nologo"; ac_cv_prog_CXXCPP="$CXXCPP"; fi
    if test -z "$AS"; then
        case "${target_cpu}" in
        i*86)
            AS=ml;
            ;;
        x86_64)
            AS=ml64;
            ;;
        esac
    fi
    if test -z "$MIDL"; then MIDL=midl; fi

    # need override this flag since we don't use $(LDFLAGS) for this.
    if test -z "$HOST_LDFLAGS" ; then
        HOST_LDFLAGS=" "
    fi
    ;;
esac
fi
])

dnl ============================================================================
dnl C++ rtti
dnl We don't use it in the code, but it can be usefull for debugging, so give
dnl the user the option of enabling it.
dnl ============================================================================
AC_DEFUN([MOZ_RTTI],
[
MOZ_ARG_ENABLE_BOOL(cpp-rtti,
[  --enable-cpp-rtti       Enable C++ RTTI ],
[ _MOZ_USE_RTTI=1 ],
[ _MOZ_USE_RTTI= ])

if test -z "$_MOZ_USE_RTTI"; then
    if test "$GNU_CC"; then
        CXXFLAGS="$CXXFLAGS -fno-rtti"
    else
        case "$target" in
        *-mingw*)
            CXXFLAGS="$CXXFLAGS -GR-"
        esac
    fi
fi
])

dnl ========================================================
dnl =
dnl = Debugging Options
dnl =
dnl ========================================================
AC_DEFUN([MOZ_DEBUGGING_OPTS],
[

if test -z "$MOZ_DEBUG" -o -n "$MOZ_ASAN"; then
    MOZ_NO_DEBUG_RTL=1
fi

AC_SUBST(MOZ_NO_DEBUG_RTL)

MOZ_DEBUG_ENABLE_DEFS="DEBUG"
MOZ_ARG_WITH_STRING(debug-label,
[  --with-debug-label=LABELS
                          Define DEBUG_<value> for each comma-separated
                          value given.],
[ for option in `echo $withval | sed 's/,/ /g'`; do
    MOZ_DEBUG_ENABLE_DEFS="$MOZ_DEBUG_ENABLE_DEFS DEBUG_${option}"
done])

if test -n "$MOZ_DEBUG"; then
    if test -n "$COMPILE_ENVIRONMENT"; then
        AC_MSG_CHECKING([for valid debug flags])
        _SAVE_CFLAGS=$CFLAGS
        CFLAGS="$CFLAGS $MOZ_DEBUG_FLAGS"
        AC_TRY_COMPILE([#include <stdio.h>],
            [printf("Hello World\n");],
            _results=yes,
            _results=no)
        AC_MSG_RESULT([$_results])
        if test "$_results" = "no"; then
            AC_MSG_ERROR([These compiler flags are invalid: $MOZ_DEBUG_FLAGS])
        fi
        CFLAGS=$_SAVE_CFLAGS
    fi

    MOZ_DEBUG_DEFINES="$MOZ_DEBUG_ENABLE_DEFS"
else
    MOZ_DEBUG_DEFINES="NDEBUG TRIMMED"
fi

AC_SUBST_LIST(MOZ_DEBUG_DEFINES)

])

dnl A high level macro for selecting compiler options.
AC_DEFUN([MOZ_COMPILER_OPTS],
[
  MOZ_DEBUGGING_OPTS
  MOZ_RTTI
if test "$CLANG_CXX"; then
    ## We disable return-type-c-linkage because jsval is defined as a C++ type but is
    ## returned by C functions. This is possible because we use knowledge about the ABI
    ## to typedef it to a C type with the same layout when the headers are included
    ## from C.
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wno-unknown-warning-option -Wno-return-type-c-linkage"
fi

if test -n "$DEVELOPER_OPTIONS"; then
    MOZ_FORCE_GOLD=1
fi

MOZ_ARG_ENABLE_BOOL(gold,
[  --enable-gold           Enable GNU Gold Linker when it is not already the default],
    MOZ_FORCE_GOLD=1,
    MOZ_FORCE_GOLD=
    )

if test "$GNU_CC" -a -n "$MOZ_FORCE_GOLD"; then
    dnl if the default linker is BFD ld, check if gold is available and try to use it
    dnl for local builds only.
    if $CC -Wl,--version 2>&1 | grep -q "GNU ld"; then
        GOLD=$($CC -print-prog-name=ld.gold)
        case "$GOLD" in
        /*)
            ;;
        *)
            GOLD=$(which $GOLD)
            ;;
        esac
        if test -n "$GOLD"; then
            mkdir -p $_objdir/build/unix/gold
            rm -f $_objdir/build/unix/gold/ld
            ln -s "$GOLD" $_objdir/build/unix/gold/ld
            if $CC -B $_objdir/build/unix/gold -Wl,--version 2>&1 | grep -q "GNU gold"; then
                LDFLAGS="$LDFLAGS -B $_objdir/build/unix/gold"
            else
                rm -rf $_objdir/build/unix/gold
            fi
        fi
    fi
fi
if test "$GNU_CC"; then
    if $CC $LDFLAGS -Wl,--version 2>&1 | grep -q "GNU ld"; then
        LD_IS_BFD=1
    fi
fi

AC_SUBST([LD_IS_BFD])

if test "$GNU_CC"; then
    if test -z "$DEVELOPER_OPTIONS"; then
        CFLAGS="$CFLAGS -ffunction-sections -fdata-sections"
        CXXFLAGS="$CXXFLAGS -ffunction-sections -fdata-sections"
    fi
    CFLAGS="$CFLAGS -fno-math-errno"
    CXXFLAGS="$CXXFLAGS -fno-exceptions -fno-math-errno"
fi

dnl ========================================================
dnl = Identical Code Folding
dnl ========================================================

MOZ_ARG_DISABLE_BOOL(icf,
[  --disable-icf          Disable Identical Code Folding],
    MOZ_DISABLE_ICF=1,
    MOZ_DISABLE_ICF= )

if test "$GNU_CC" -a "$GCC_USE_GNU_LD" -a -z "$MOZ_DISABLE_ICF" -a -z "$DEVELOPER_OPTIONS"; then
    AC_CACHE_CHECK([whether the linker supports Identical Code Folding],
        LD_SUPPORTS_ICF,
        [echo 'int foo() {return 42;}' \
              'int bar() {return 42;}' \
              'int main() {return foo() - bar();}' > conftest.${ac_ext}
        # If the linker supports ICF, foo and bar symbols will have
        # the same address
        if AC_TRY_COMMAND([${CC-cc} -o conftest${ac_exeext} $LDFLAGS -Wl,--icf=safe -ffunction-sections conftest.${ac_ext} $LIBS 1>&2]) &&
           test -s conftest${ac_exeext} &&
           objdump -t conftest${ac_exeext} | awk changequote(<<, >>)'{a[<<$>>6] = <<$>>1} END {if (a["foo"] && (a["foo"] != a["bar"])) { exit 1 }}'changequote([, ]); then
            LD_SUPPORTS_ICF=yes
        else
            LD_SUPPORTS_ICF=no
        fi
        rm -rf conftest*])
    if test "$LD_SUPPORTS_ICF" = yes; then
        _SAVE_LDFLAGS="$LDFLAGS -Wl,--icf=safe"
        LDFLAGS="$LDFLAGS -Wl,--icf=safe -Wl,--print-icf-sections"
        AC_TRY_LINK([], [],
                    [LD_PRINT_ICF_SECTIONS=-Wl,--print-icf-sections],
                    [LD_PRINT_ICF_SECTIONS=])
        AC_SUBST([LD_PRINT_ICF_SECTIONS])
        LDFLAGS="$_SAVE_LDFLAGS"
    fi
fi

dnl ========================================================
dnl = Automatically remove dead symbols
dnl ========================================================

if test "$GNU_CC" -a "$GCC_USE_GNU_LD" -a -z "$DEVELOPER_OPTIONS"; then
    if test -n "$MOZ_DEBUG_FLAGS"; then
        dnl See bug 670659
        AC_CACHE_CHECK([whether removing dead symbols breaks debugging],
            GC_SECTIONS_BREAKS_DEBUG_RANGES,
            [echo 'int foo() {return 42;}' \
                  'int bar() {return 1;}' \
                  'int main() {return foo();}' > conftest.${ac_ext}
            if AC_TRY_COMMAND([${CC-cc} -o conftest.${ac_objext} $CFLAGS $MOZ_DEBUG_FLAGS -c conftest.${ac_ext} 1>&2]) &&
                AC_TRY_COMMAND([${CC-cc} -o conftest${ac_exeext} $LDFLAGS $MOZ_DEBUG_FLAGS -Wl,--gc-sections conftest.${ac_objext} $LIBS 1>&2]) &&
                test -s conftest${ac_exeext} -a -s conftest.${ac_objext}; then
                 if test "`$PYTHON -m mozbuild.configure.check_debug_ranges conftest.${ac_objext} conftest.${ac_ext}`" = \
                         "`$PYTHON -m mozbuild.configure.check_debug_ranges conftest${ac_exeext} conftest.${ac_ext}`"; then
                     GC_SECTIONS_BREAKS_DEBUG_RANGES=no
                 else
                     GC_SECTIONS_BREAKS_DEBUG_RANGES=yes
                 fi
             else
                  dnl We really don't expect to get here, but just in case
                  GC_SECTIONS_BREAKS_DEBUG_RANGES="no, but it's broken in some other way"
             fi
             rm -rf conftest*])
         if test "$GC_SECTIONS_BREAKS_DEBUG_RANGES" = no; then
             DSO_LDOPTS="$DSO_LDOPTS -Wl,--gc-sections"
         fi
    else
        DSO_LDOPTS="$DSO_LDOPTS -Wl,--gc-sections"
    fi
fi

# bionic in Android < 4.1 doesn't support PIE
# On OSX, the linker defaults to building PIE programs when targeting OSX 10.7.
# On other Unix systems, some file managers (Nautilus) can't start PIE programs
if test -n "$gonkdir" && test "$ANDROID_VERSION" -ge 16; then
    MOZ_PIE=1
else
    MOZ_PIE=
fi

MOZ_ARG_ENABLE_BOOL(pie,
[  --enable-pie           Enable Position Independent Executables],
    MOZ_PIE=1,
    MOZ_PIE= )

if test "$GNU_CC$CLANG_CC" -a -n "$MOZ_PIE"; then
    AC_MSG_CHECKING([for PIE support])
    _SAVE_LDFLAGS=$LDFLAGS
    LDFLAGS="$LDFLAGS $DSO_PIC_CFLAGS -pie"
    AC_TRY_LINK(,,AC_MSG_RESULT([yes])
                  [MOZ_PROGRAM_LDFLAGS="$MOZ_PROGRAM_LDFLAGS -pie"],
                  AC_MSG_RESULT([no])
                  AC_MSG_ERROR([--enable-pie requires PIE support from the linker.]))
    LDFLAGS=$_SAVE_LDFLAGS
fi

AC_SUBST(MOZ_PROGRAM_LDFLAGS)

dnl ASan assumes no symbols are being interposed, and when that happens,
dnl it's not happy with it. Unconveniently, since Firefox is exporting
dnl libffi symbols and Gtk+3 pulls system libffi via libwayland-client,
dnl system libffi interposes libffi symbols that ASan assumes are in
dnl libxul, so it barfs about buffer overflows.
dnl Using -Wl,-Bsymbolic ensures no exported symbol can be interposed.
if test -n "$GCC_USE_GNU_LD"; then
  case "$LDFLAGS" in
  *-fsanitize=address*)
    LDFLAGS="$LDFLAGS -Wl,-Bsymbolic"
    ;;
  esac
fi

])
