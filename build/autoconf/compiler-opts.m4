dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Add compiler specific options

dnl ============================================================================
dnl C++ rtti
dnl We don't use it in the code, but it can be usefull for debugging, so give
dnl the user the option of enabling it.
dnl ============================================================================
AC_DEFUN([MOZ_RTTI],
[
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
fi
])

dnl A high level macro for selecting compiler options.
AC_DEFUN([MOZ_COMPILER_OPTS],
[
  MOZ_DEBUGGING_OPTS
  MOZ_RTTI

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

if test "$GNU_CC" -a "$GCC_USE_GNU_LD" -a -z "$MOZ_DISABLE_ICF" -a -z "$DEVELOPER_OPTIONS"; then
    AC_CACHE_CHECK([whether the linker supports Identical Code Folding],
        moz_cv_opt_ld_supports_icf,
        [echo 'int foo() {return 42;}' \
              'int bar() {return 42;}' \
              'int main() {return foo() - bar();}' > conftest.${ac_ext}
        # If the linker supports ICF, foo and bar symbols will have
        # the same address
        if AC_TRY_COMMAND([${CC-cc} -o conftest${ac_exeext} $LDFLAGS -Wl,--icf=safe -ffunction-sections conftest.${ac_ext} $LIBS 1>&2]) &&
           test -s conftest${ac_exeext} &&
           $LLVM_OBJDUMP -t conftest${ac_exeext} | awk changequote(<<, >>)'{a[<<$>>6] = <<$>>1} END {if (a["foo"] && (a["foo"] != a["bar"])) { exit 1 }}'changequote([, ]); then
            moz_cv_opt_ld_supports_icf=yes
        else
            moz_cv_opt_ld_supports_icf=no
        fi
        rm -rf conftest*])
    if test "$moz_cv_opt_ld_supports_icf" = yes; then
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
dnl = Detect static linkage of libstdc++
dnl ========================================================

if test "$OS_TARGET" = Linux; then

AC_CACHE_CHECK([whether we're trying to statically link with libstdc++],
    moz_cv_opt_static_libstdcxx,
    [moz_cv_opt_static_libstdcxx=no
     AC_LANG_SAVE
     AC_LANG_CPLUSPLUS
     cat > conftest.$ac_ext <<EOF
#include <iostream>
int main() { std::cout << 1; }
EOF
     dnl This test is quite conservative: it assumes dynamic linkage if the compilation step fails or if
     dnl the binary format is not supported. But it still detects basic issues.
     if AC_TRY_EVAL([ac_link]) && test -s conftest${ac_exeext} && $LLVM_OBJDUMP --private-headers conftest${ac_exeext} 2> conftest.err 1> conftest.out
     then
         if test -s conftest.err
         then :
         elif grep -q -E 'NEEDED.*lib(std)?c\+\+' conftest.out
         then :
         else moz_cv_opt_static_libstdcxx=yes
         fi
     fi
     AC_LANG_RESTORE
     rm -f conftest*
])
if test "$moz_cv_opt_static_libstdcxx" = "yes"; then
    AC_MSG_ERROR([Firefox does not support linking statically with libstdc++])
fi

fi

dnl ========================================================
dnl = Automatically remove dead symbols
dnl ========================================================

SANCOV=
if test -n "$LIBFUZZER"; then
    case "$LIBFUZZER_FLAGS" in
    *-fsanitize-coverage*|*-fsanitize=fuzzer*)
        SANCOV=1
        ;;
    esac
fi

if test "$GNU_CC" -a "$GCC_USE_GNU_LD" -a -z "$DEVELOPER_OPTIONS" -a -z "$MOZ_PROFILE_GENERATE" -a -z "$SANCOV"; then
    if test -n "$MOZ_DEBUG_FLAGS"; then
        dnl See bug 670659
        AC_CACHE_CHECK([whether removing dead symbols breaks debugging],
            moz_cv_opt_gc_sections_breaks_debug_ranges,
            [echo 'int foo() {return 42;}' \
                  'int bar() {return 1;}' \
                  'int main() {return foo();}' > conftest.${ac_ext}
            if AC_TRY_COMMAND([${CC-cc} -o conftest.${ac_objext} $CFLAGS $MOZ_DEBUG_FLAGS -c conftest.${ac_ext} 1>&2]) &&
                AC_TRY_COMMAND([${CC-cc} -o conftest${ac_exeext} $LDFLAGS $MOZ_DEBUG_FLAGS -Wl,--gc-sections conftest.${ac_objext} $LIBS 1>&2]) &&
                test -s conftest${ac_exeext} -a -s conftest.${ac_objext}; then
                 if test "`$PYTHON3 -m mozbuild.configure.check_debug_ranges conftest.${ac_objext} conftest.${ac_ext}`" = \
                         "`$PYTHON3 -m mozbuild.configure.check_debug_ranges conftest${ac_exeext} conftest.${ac_ext}`"; then
                     moz_cv_opt_gc_sections_breaks_debug_ranges=no
                 else
                     moz_cv_opt_gc_sections_breaks_debug_ranges=yes
                 fi
             else
                  dnl We really don't expect to get here, but just in case
                  moz_cv_opt_gc_sections_breaks_debug_ranges="no, but it's broken in some other way"
             fi
             rm -rf conftest*])
         if test "$moz_cv_opt_gc_sections_breaks_debug_ranges" = no; then
             DSO_LDOPTS="$DSO_LDOPTS -Wl,--gc-sections"
         fi
    else
        DSO_LDOPTS="$DSO_LDOPTS -Wl,--gc-sections"
    fi
fi

if test "$GNU_CC$CLANG_CC"; then
    case "${OS_TARGET}" in
    Darwin|WASI)
        # It's the default on those targets, and clang complains about -pie
        # being unused if passed.
        ;;
    *)
        MOZ_PROGRAM_LDFLAGS="$MOZ_PROGRAM_LDFLAGS -pie"
        ;;
    esac
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
