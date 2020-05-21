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
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wno-unknown-warning-option"
fi

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
           $LLVM_OBJDUMP -t conftest${ac_exeext} | awk changequote(<<, >>)'{a[<<$>>6] = <<$>>1} END {if (a["foo"] && (a["foo"] != a["bar"])) { exit 1 }}'changequote([, ]); then
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
                 if test "`$PYTHON3 -m mozbuild.configure.check_debug_ranges conftest.${ac_objext} conftest.${ac_ext}`" = \
                         "`$PYTHON3 -m mozbuild.configure.check_debug_ranges conftest${ac_exeext} conftest.${ac_ext}`"; then
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

if test "$GNU_CC$CLANG_CC"; then
    MOZ_PROGRAM_LDFLAGS="$MOZ_PROGRAM_LDFLAGS -pie"
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
