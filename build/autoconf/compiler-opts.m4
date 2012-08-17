dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Add compiler specific options

AC_DEFUN([MOZ_DEFAULT_COMPILER],
[
dnl Default to MSVC for win32 and gcc-4.2 for darwin
dnl ==============================================================
if test -z "$CROSS_COMPILE"; then
case "$target" in
*-mingw*)
    if test -z "$CC"; then CC=cl; fi
    if test -z "$CXX"; then CXX=cl; fi
    if test -z "$CPP"; then CPP="cl -E -nologo"; fi
    if test -z "$CXXCPP"; then CXXCPP="cl -TP -E -nologo"; ac_cv_prog_CXXCPP="$CXXCPP"; fi
    if test -z "$LD"; then LD=link; fi
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
    # we prefer gcc-4.2 over gcc on older darwin, so
    # use that specific version if it's available.
    # On newer versions of darwin, gcc is llvm-gcc while gcc-4.2 is the plain
    # one, so we also try that first. If that fails, we fall back to clang
    # as llvm-gcc is an unsupported dead end.
    MOZ_PATH_PROGS(CC, $CC gcc-4.2 clang gcc)
    MOZ_PATH_PROGS(CXX, $CXX g++-4.2 clang++ g++)
    IS_LLVM_GCC=$($CC -v 2>&1 | grep llvm-gcc)
    if test -n "$IS_LLVM_GCC"
    then
      echo llvm-gcc is known to be broken, please use gcc-4.2 or clang.
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

dnl A high level macro for selecting compiler options.
AC_DEFUN([MOZ_COMPILER_OPTS],
[
  MOZ_RTTI
if test "$CLANG_CXX"; then
    ## We disable return-type-c-linkage because jsval is defined as a C++ type but is
    ## returned by C functions. This is possible because we use knowledge about the ABI
    ## to typedef it to a C type with the same layout when the headers are included
    ## from C.
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wno-unknown-warning-option -Wno-return-type-c-linkage"
fi

if test "$GNU_CC"; then
    CFLAGS="$CFLAGS -ffunction-sections -fdata-sections"
    CXXFLAGS="$CXXFLAGS -ffunction-sections -fdata-sections -fno-exceptions"
fi

dnl ========================================================
dnl = Identical Code Folding
dnl ========================================================

MOZ_ARG_DISABLE_BOOL(icf,
[  --disable-icf          Disable Identical Code Folding],
    MOZ_DISABLE_ICF=1,
    MOZ_DISABLE_ICF= )

if test "$GNU_CC" -a "$GCC_USE_GNU_LD" -a -z "$MOZ_DISABLE_ICF"; then
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

if test "$GNU_CC" -a "$GCC_USE_GNU_LD" -a -n "$MOZ_DEBUG_FLAGS"; then
   dnl See bug 670659
   AC_CACHE_CHECK([whether removing dead symbols breaks debugging],
       GC_SECTIONS_BREAKS_DEBUG_RANGES,
       [echo 'int foo() {return 42;}' \
             'int bar() {return 1;}' \
             'int main() {return foo();}' > conftest.${ac_ext}
        if AC_TRY_COMMAND([${CC-cc} -o conftest.${ac_objext} $CFLAGS $MOZ_DEBUG_FLAGS -c conftest.${ac_ext} 1>&2]) &&
           AC_TRY_COMMAND([${CC-cc} -o conftest${ac_exeext} $LDFLAGS $MOZ_DEBUG_FLAGS -Wl,--gc-sections conftest.${ac_objext} $LIBS 1>&2]) &&
           test -s conftest${ac_exeext} -a -s conftest.${ac_objext}; then
            if test "`$PYTHON "$_topsrcdir"/build/autoconf/check_debug_ranges.py conftest.${ac_objext} conftest.${ac_ext}`" = \
                    "`$PYTHON "$_topsrcdir"/build/autoconf/check_debug_ranges.py conftest${ac_exeext} conftest.${ac_ext}`"; then
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
