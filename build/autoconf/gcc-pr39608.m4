dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Check if the compiler is gcc and has pr39608. If so
dnl disable vrp.

AC_DEFUN([MOZ_GCC_PR39608],
[
AC_MSG_CHECKING(for gcc pr39608)
ac_have_gcc_pr39608="yes"
AC_LANG_SAVE
AC_LANG_CPLUSPLUS

AC_TRY_COMPILE([
typedef void (*FuncType)();
template<FuncType Impl>
void f();
template<typename T> class C {
  typedef C<T> ThisC;
  template<int g()>
  static void h() {
    f<ThisC::h<g> >();
  }
};
], true,
   ac_have_gcc_pr39608="no",
   true)

AC_LANG_RESTORE

AC_MSG_RESULT($ac_have_gcc_pr39608)
if test "$ac_have_gcc_pr39608" = "yes"; then
   echo This compiler would fail to build firefox, plase upgrade.
   exit 1
fi
])
