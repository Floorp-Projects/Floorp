dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl check if the build is using lto. This is really primitive and only detects llvm based
dnl compilers right now.
AC_DEFUN([MOZ_DOING_LTO],
[
  cat > conftest.c <<EOF
                  int foo = 1;
EOF
  $1=no
  if ${CC-cc} ${CFLAGS} -S conftest.c -o conftest.s >/dev/null 2>&1; then
    if grep '^target triple =' conftest.s; then
      $1=yes
    fi
  fi
  rm -f conftest.[cs]
])
