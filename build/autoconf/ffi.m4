dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_CONFIG_FFI], [

MOZ_ARG_ENABLE_BOOL(system-ffi,
[  --enable-system-ffi       Use system libffi (located with pkgconfig)],
    MOZ_NATIVE_FFI=1 )

if test -n "$MOZ_NATIVE_FFI"; then
    # Vanilla libffi 3.0.9 needs a few patches from upcoming version 3.0.10
    # for non-GCC compilers.
    if test -z "$GNU_CC"; then
        PKG_CHECK_MODULES(MOZ_FFI, libffi > 3.0.9)
    else
        PKG_CHECK_MODULES(MOZ_FFI, libffi >= 3.0.9)
    fi
fi

AC_SUBST(MOZ_NATIVE_FFI)

])
