dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_RUST_SUPPORT], [
  MOZ_PATH_PROG(RUSTC, rustc)
  if test -n "$RUSTC"; then
    AC_MSG_CHECKING([rustc version])
    RUSTC_VERSION=`$RUSTC --version | cut -d ' ' -f 2`
    # Parse out semversion elements.
    _RUSTC_MAJOR_VERSION=`echo ${RUSTC_VERSION} | cut -d . -f 1`
    _RUSTC_MINOR_VERSION=`echo ${RUSTC_VERSION} | cut -d . -f 2`
    _RUSTC_EXTRA_VERSION=`echo ${RUSTC_VERSION} | cut -d . -f 3 | cut -d + -f 1`
    _RUSTC_PATCH_VERSION=`echo ${_RUSTC_EXTRA_VERSION} | cut -d '-' -f 1`
    AC_MSG_RESULT([${_RUSTC_MAJOR_VERSION}.${_RUSTC_MINOR_VERSION}.${_RUSTC_PATCH_VERSION} ($RUSTC_VERSION)])
  fi
  MOZ_ARG_ENABLE_BOOL([rust],
                      [  --enable-rust           Include Rust language sources],
                      [MOZ_RUST=1],
                      [MOZ_RUST= ])
  if test -z "$RUSTC" -a -n "$MOZ_RUST"; then
    AC_MSG_ERROR([Rust compiler not found.
      To compile rust language sources, you must have 'rustc' in your path.
      See http://www.rust-lang.org/ for more information.])
  fi
  if test -n "$MOZ_RUST" && test -z "$_RUSTC_MAJOR_VERSION" -o \
    "$_RUSTC_MAJOR_VERSION" -lt 1 -o \
    \( "$_RUSTC_MAJOR_VERSION" -eq 1 -a "$_RUSTC_MINOR_VERSION" -lt 5 \); then
    AC_MSG_ERROR([Rust compiler ${RUSTC_VERSION} is too old.
      To compile Rust language sources please install at least
      version 1.5 of the 'rustc' toolchain and make sure it is
      first in your path.
      You can verify this by typing 'rustc --version'.])
  fi

  if test -n "$RUSTC" -a -n "$MOZ_RUST"; then
    # Rust's --target options are similar to, but not exactly the same
    # as, the autoconf-derived targets we use.  An example would be that
    # Rust uses distinct target triples for targetting the GNU C++ ABI
    # and the MSVC C++ ABI on Win32, whereas autoconf has a single
    # triple and relies on the user to ensure that everything is
    # compiled for the appropriate ABI.  We need to perform appropriate
    # munging to get the correct option to rustc.
    #
    # The canonical list of targets supported can be derived from:
    #
    # https://github.com/rust-lang/rust/tree/master/mk/cfg
    rust_target=
    case "$target" in
      # Linux
      i*86*linux-gnu)
          rust_target=i686-unknown-linux-gnu
          ;;
      x86_64*linux-gnu)
          rust_target=x86_64-unknown-linux-gnu
          ;;

      # OS X and iOS
      i*86-apple-darwin*)
          rust_target=i686-apple-darwin
          ;;
      i*86-apple-ios*)
          rust_target=i386-apple-ios
          ;;
      x86_64-apple-darwin*)
          rust_target=x86_64-apple-darwin
          ;;

      # Android
      i*86*linux-android)
          rust_target=i686-linux-android
          ;;
      arm*linux-android*)
          rust_target=arm-linux-androideabi
          ;;

      # Windows
      i*86-pc-mingw32)
          # XXX better detection of CXX needed here, to figure out whether
          # we need i686-pc-windows-gnu instead, since mingw32 builds work.
          rust_target=i686-pc-windows-msvc
          ;;
      x86_64-pc-mingw32)
          # XXX and here as well
          rust_target=x86_64-pc-windows-msvc
          ;;
      *)
          AC_ERROR([don't know how to translate $target for rustc])
    esac

    # Check to see whether we need to pass --target to RUSTC.  This can
    # happen when building Firefox on Windows: js's configure will receive
    # a RUSTC from the toplevel configure that already has --target added to
    # it.
    rustc_target_arg=
    case "$RUSTC" in
      *--target=${rust_target}*)
        ;;
      *)
        rustc_target_arg=--target=${rust_target}
        ;;
    esac

    # Check to see whether our rustc has a reasonably functional stdlib
    # for our chosen target.
    echo 'pub extern fn hello() { println!("Hello world"); }' > conftest.rs
    if AC_TRY_COMMAND(${RUSTC} --crate-type staticlib ${rustc_target_arg} -o conftest.rlib conftest.rs > /dev/null) && test -s conftest.rlib; then
      RUSTC="${RUSTC} ${rustc_target_arg}"
    else
      AC_ERROR([cannot compile for ${rust_target} with ${RUSTC}])
    fi
    rm -f conftest.rs conftest.rlib
  fi

  # TODO: separate HOST_RUSTC and RUSTC variables

  AC_SUBST(MOZ_RUST)
])
