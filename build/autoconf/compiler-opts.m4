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
    if test -z "$CC"; then CC=cl; fi
    if test -z "$CXX"; then CXX=cl; fi
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
*-darwin*)
    # GCC on darwin is based on gcc 4.2 and we don't support it anymore.
    if test -z "$CC"; then
        MOZ_PATH_PROGS(CC, clang)
    fi
    if test -z "$CXX"; then
        MOZ_PATH_PROGS(CXX, clang++)
    fi
    IS_GCC=$($CC -v 2>&1 | grep gcc)
    if test -n "$IS_GCC"
    then
      echo gcc is known to be broken on OS X, please use clang.
      echo see http://developer.mozilla.org/en-US/docs/Developer_Guide/Build_Instructions/Mac_OS_X_Prerequisites
      echo for more information.
      exit 1
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

MOZ_DEBUG_ENABLE_DEFS="DEBUG TRACING"
MOZ_ARG_WITH_STRING(debug-label,
[  --with-debug-label=LABELS
                          Define DEBUG_<value> for each comma-separated
                          value given.],
[ for option in `echo $withval | sed 's/,/ /g'`; do
    MOZ_DEBUG_ENABLE_DEFS="$MOZ_DEBUG_ENABLE_DEFS DEBUG_${option}"
done])

if test -n "$MOZ_DEBUG"; then
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
# On OSX, the linker defaults to building PIE programs when targetting OSX 10.7+,
# but not when targetting OSX < 10.7. OSX < 10.7 doesn't support running PIE
# programs, so as long as support for OSX 10.6 is kept, we can't build PIE.
# Even after dropping 10.6 support, MOZ_PIE would not be useful since it's the
# default (and clang says the -pie option is not used).
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

if test "$GNU_CC" -a -n "$MOZ_PIE"; then
    AC_MSG_CHECKING([for PIE support])
    _SAVE_LDFLAGS=$LDFLAGS
    LDFLAGS="$LDFLAGS -pie"
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

dnl GCC and clang will fail if given an unknown warning option like -Wfoobar. 
dnl But later versions won't fail if given an unknown negated warning option
dnl like -Wno-foobar.  So when we are check for support of negated warning 
dnl options, we actually test the positive form, but add the negated form to 
dnl the flags variable.

AC_DEFUN([MOZ_C_SUPPORTS_WARNING],
[
    AC_CACHE_CHECK(whether the C compiler supports $1$2, $3,
        [
            AC_LANG_SAVE
            AC_LANG_C
            _SAVE_CFLAGS="$CFLAGS"
            CFLAGS="$CFLAGS -Werror -W$2"
            AC_TRY_COMPILE([],
                           [return(0);],
                           $3="yes",
                           $3="no")
            CFLAGS="$_SAVE_CFLAGS"
            AC_LANG_RESTORE
        ])
    if test "${$3}" = "yes"; then
        _WARNINGS_CFLAGS="${_WARNINGS_CFLAGS} $1$2"
    fi
])

AC_DEFUN([MOZ_CXX_SUPPORTS_WARNING],
[
    AC_CACHE_CHECK(whether the C++ compiler supports $1$2, $3,
        [
            AC_LANG_SAVE
            AC_LANG_CPLUSPLUS
            _SAVE_CXXFLAGS="$CXXFLAGS"
            CXXFLAGS="$CXXFLAGS -Werror -W$2"
            AC_TRY_COMPILE([],
                           [return(0);],
                           $3="yes",
                           $3="no")
            CXXFLAGS="$_SAVE_CXXFLAGS"
            AC_LANG_RESTORE
        ])
    if test "${$3}" = "yes"; then
        _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} $1$2"
    fi
])

AC_DEFUN([MOZ_SET_WARNINGS_CFLAGS],
[
    # Turn on gcc/clang warnings:
    # https://gcc.gnu.org/onlinedocs/gcc-4.7.2/gcc/Warning-Options.html

    # -Wall - lots of useful warnings
    # -Wempty-body - catches bugs, e.g. "if (c); foo();", few false positives
    # -Wignored-qualifiers - catches return types with qualifiers like const
    # -Wpointer-arith - catches pointer arithmetic using NULL or sizeof(void)
    # -Wsign-compare - catches comparing signed/unsigned ints
    # -Wtype-limits - catches overflow bugs, few false positives
    # -Wunreachable-code - catches some dead code
    _WARNINGS_CFLAGS="${_WARNINGS_CFLAGS} -Wall"
    _WARNINGS_CFLAGS="${_WARNINGS_CFLAGS} -Wempty-body"
    _WARNINGS_CFLAGS="${_WARNINGS_CFLAGS} -Wignored-qualifiers"
    _WARNINGS_CFLAGS="${_WARNINGS_CFLAGS} -Wpointer-arith"
    _WARNINGS_CFLAGS="${_WARNINGS_CFLAGS} -Wsign-compare"
    _WARNINGS_CFLAGS="${_WARNINGS_CFLAGS} -Wtype-limits"
    _WARNINGS_CFLAGS="${_WARNINGS_CFLAGS} -Wunreachable-code"

    # -Wclass-varargs - catches objects passed by value to variadic functions.
    # -Wloop-analysis - catches issues around loops
    # -Wnon-literal-null-conversion - catches expressions used as a null pointer constant
    # -Wstring-conversion - catches string literals used in boolean expressions
    # -Wthread-safety - catches inconsistent use of mutexes
    #
    # XXX: at the time of writing, the version of clang used on the OS X test
    # machines has a bug that causes it to reject some valid files if both
    # -Wnon-literal-null-conversion and -Wsometimes-uninitialized are
    # specified. We work around this by instead using
    # -Werror=non-literal-null-conversion, but we only do that when
    # --enable-warnings-as-errors is specified so that no unexpected fatal
    # warnings are produced.
    MOZ_C_SUPPORTS_WARNING(-W, class-varargs, ac_c_has_wclass_varargs)
    MOZ_C_SUPPORTS_WARNING(-W, loop-analysis, ac_c_has_wloop_analysis)

    if test "$MOZ_ENABLE_WARNINGS_AS_ERRORS"; then
        MOZ_C_SUPPORTS_WARNING(-Werror=, non-literal-null-conversion, ac_c_has_non_literal_null_conversion)
    fi

    MOZ_C_SUPPORTS_WARNING(-W, string-conversion, ac_c_has_wstring_conversion)
    MOZ_C_SUPPORTS_WARNING(-W, thread-safety, ac_c_has_wthread_safety)

    # Turn off some non-useful warnings that -Wall turns on.

    # Prevent the following GCC warnings from being treated as errors:
    # -Wmaybe-uninitialized - too many false positives
    # -Wdeprecated-declarations - we don't want our builds held hostage when a
    #   platform-specific API becomes deprecated.
    # -Wfree-nonheap-object - false positives during PGO
    # -Warray-bounds - false positives depending on optimization
    MOZ_C_SUPPORTS_WARNING(-W, no-error=maybe-uninitialized, ac_c_has_noerror_maybe_uninitialized)
    MOZ_C_SUPPORTS_WARNING(-W, no-error=deprecated-declarations, ac_c_has_noerror_deprecated_declarations)
    MOZ_C_SUPPORTS_WARNING(-W, no-error=array-bounds, ac_c_has_noerror_array_bounds)

    if test -n "$MOZ_PGO"; then
        MOZ_C_SUPPORTS_WARNING(-W, no-error=coverage-mismatch, ac_c_has_noerror_coverage_mismatch)
        MOZ_C_SUPPORTS_WARNING(-W, no-error=free-nonheap-object, ac_c_has_noerror_free_nonheap_object)
    fi
])

AC_DEFUN([MOZ_SET_WARNINGS_CXXFLAGS],
[
    # Turn on gcc/clang warnings:
    # https://gcc.gnu.org/onlinedocs/gcc-4.7.2/gcc/Warning-Options.html

    # -Wall - lots of useful warnings
    # -Wc++1[14z]-compat[-pedantic] - catches C++ version forward-compat issues
    # -Wempty-body - catches bugs, e.g. "if (c); foo();", few false positives
    # -Wignored-qualifiers - catches return types with qualifiers like const
    # -Woverloaded-virtual - function declaration hides virtual function from base class
    # -Wpointer-arith - catches pointer arithmetic using NULL or sizeof(void)
    # -Wsign-compare - catches comparing signed/unsigned ints
    # -Wtype-limits - catches overflow bugs, few false positives
    # -Wunreachable-code - catches some dead code
    # -Wwrite-strings - catches treating string literals as non-const
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wall"
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wc++11-compat"
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wempty-body"
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wignored-qualifiers"
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Woverloaded-virtual"
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wpointer-arith"
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wsign-compare"
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wtype-limits"
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wunreachable-code"
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wwrite-strings"

    # -Wclass-varargs - catches objects passed by value to variadic functions.
    # -Wimplicit-fallthrough - catches unintentional switch case fallthroughs
    # -Wloop-analysis - catches issues around loops
    # -Wnon-literal-null-conversion - catches expressions used as a null pointer constant
    # -Wstring-conversion - catches string literals used in boolean expressions
    # -Wthread-safety - catches inconsistent use of mutexes
    #
    # XXX: at the time of writing, the version of clang used on the OS X test
    # machines has a bug that causes it to reject some valid files if both
    # -Wnon-literal-null-conversion and -Wsometimes-uninitialized are
    # specified. We work around this by instead using
    # -Werror=non-literal-null-conversion, but we only do that when
    # --enable-warnings-as-errors is specified so that no unexpected fatal
    # warnings are produced.
    MOZ_CXX_SUPPORTS_WARNING(-W, c++11-compat-pedantic, ac_cxx_has_wcxx11_compat_pedantic)
    MOZ_CXX_SUPPORTS_WARNING(-W, c++14-compat, ac_cxx_has_wcxx14_compat)
    MOZ_CXX_SUPPORTS_WARNING(-W, c++14-compat-pedantic, ac_cxx_has_wcxx14_compat_pedantic)
    MOZ_CXX_SUPPORTS_WARNING(-W, c++1z-compat, ac_cxx_has_wcxx1z_compat)
    MOZ_CXX_SUPPORTS_WARNING(-W, class-varargs, ac_cxx_has_wclass_varargs)
    MOZ_CXX_SUPPORTS_WARNING(-W, implicit-fallthrough, ac_cxx_has_wimplicit_fallthrough)
    MOZ_CXX_SUPPORTS_WARNING(-W, loop-analysis, ac_cxx_has_wloop_analysis)

    if test "$MOZ_ENABLE_WARNINGS_AS_ERRORS"; then
        MOZ_CXX_SUPPORTS_WARNING(-Werror=, non-literal-null-conversion, ac_cxx_has_non_literal_null_conversion)
    fi

    MOZ_CXX_SUPPORTS_WARNING(-W, string-conversion, ac_cxx_has_wstring_conversion)
    MOZ_CXX_SUPPORTS_WARNING(-W, thread-safety, ac_cxx_has_wthread_safety)

    # Turn off some non-useful warnings that -Wall turns on.

    # -Wno-invalid-offsetof - we use offsetof on non-POD types frequently
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wno-invalid-offsetof"

    # -Wno-inline-new-delete - we inline 'new' and 'delete' in mozalloc
    MOZ_CXX_SUPPORTS_WARNING(-Wno-, inline-new-delete, ac_cxx_has_wno_inline_new_delete)

    # Prevent the following GCC warnings from being treated as errors:
    # -Wmaybe-uninitialized - too many false positives
    # -Wdeprecated-declarations - we don't want our builds held hostage when a
    #   platform-specific API becomes deprecated.
    # -Wfree-nonheap-object - false positives during PGO
    # -Warray-bounds - false positives depending on optimization
    MOZ_CXX_SUPPORTS_WARNING(-W, no-error=maybe-uninitialized, ac_cxx_has_noerror_maybe_uninitialized)
    MOZ_CXX_SUPPORTS_WARNING(-W, no-error=deprecated-declarations, ac_cxx_has_noerror_deprecated_declarations)
    MOZ_CXX_SUPPORTS_WARNING(-W, no-error=array-bounds, ac_cxx_has_noerror_array_bounds)

    if test -n "$MOZ_PGO"; then
        MOZ_CXX_SUPPORTS_WARNING(-W, no-error=coverage-mismatch, ac_cxx_has_noerror_coverage_mismatch)
        MOZ_CXX_SUPPORTS_WARNING(-W, no-error=free-nonheap-object, ac_cxx_has_noerror_free_nonheap_object)
    fi
])
