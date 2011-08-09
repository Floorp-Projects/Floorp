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
