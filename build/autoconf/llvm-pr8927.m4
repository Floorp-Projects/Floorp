dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Check if the compiler suffers from http://llvm.org/pr8927. If so, ask the
dnl user to upgrade.

AC_DEFUN([MOZ_LLVM_PR8927],
[
AC_MSG_CHECKING(for llvm pr8927)
ac_have_llvm_pr8927="no"
AC_LANG_SAVE
AC_LANG_C

_SAVE_CFLAGS=$CFLAGS
CFLAGS="-O2"
AC_TRY_RUN([
struct foobar {
  int x;
};
static const struct foobar* foo() {
  static const struct foobar d = { 0 };
  return &d;
}
static const struct foobar* bar() {
  static const struct foobar d = { 0 };
  return &d;
}
__attribute__((noinline)) int zed(const struct foobar *a,
                                  const struct foobar *b) {
  return a == b;
}
int main() {
  return zed(foo(), bar());
}
], true,
   ac_have_llvm_pr8927="yes",
   true)
CFLAGS="$_SAVE_CFLAGS"

AC_LANG_RESTORE

if test "$ac_have_llvm_pr8927" = "yes"; then
   AC_MSG_RESULT(yes)
   echo This compiler would miscompile firefox, please upgrade.
   echo see http://developer.mozilla.org/en-US/docs/Developer_Guide/Build_Instructions/Mac_OS_X_Prerequisites
   echo for more information.
   exit 1
else
   AC_MSG_RESULT(no)
fi
])
