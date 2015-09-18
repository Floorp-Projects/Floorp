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
    AC_MSG_RESULT([$RUSTC_VERSION (v${_RUSTC_MAJOR_VERSION}.${_RUSTC_MINOR_VERSION}.${_RUSTC_PATCH_VERSION})])
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
    "$_RUSTC_MAJOR_VERSION" -lt 1; then
    AC_MSG_ERROR([Rust compiler ${RUSTC_VERSION} is too old.
      To compile Rust language sources please install at least
      version 1.0 of the 'rustc' toolchain and make sure it is
      first in your path.
      You can verify this by typing 'rustc --version'.])
  fi
  AC_SUBST(MOZ_RUST)
])
