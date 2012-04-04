dnl Add compiler specific options

AC_DEFUN([MOZ_COMPILER_OPTS],
[
if test "$CLANG_CXX"; then
    ## We disable return-type-c-linkage because jsval is defined as a C++ type but is
    ## returned by C functions. This is possible because we use knowledge about the ABI
    ## to typedef it to a C type with the same layout when the headers are included
    ## from C.
    _WARNINGS_CXXFLAGS="${_WARNINGS_CXXFLAGS} -Wno-unknown-warning-option -Wno-return-type-c-linkage"
fi

if test "$GNU_CC"; then
    CFLAGS="$CFLAGS -ffunction-sections -fdata-sections"
    CXXFLAGS="$CXXFLAGS -ffunction-sections -fdata-sections"
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
