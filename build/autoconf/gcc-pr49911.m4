dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Check if the compiler is gcc and has PR49911. If so
dnl disable vrp.

AC_DEFUN([MOZ_GCC_PR49911],
[
if test "$GNU_CC"; then

AC_MSG_CHECKING(for gcc PR49911)
ac_have_gcc_pr49911="no"
AC_LANG_SAVE
AC_LANG_CPLUSPLUS

_SAVE_CXXFLAGS=$CXXFLAGS
CXXFLAGS="-O2"
AC_TRY_RUN([
extern "C" void abort(void);
typedef enum {
eax,         ecx,         edx,         ebx,         esp,         ebp,
esi,         edi     }
RegisterID;
union StateRemat {
  RegisterID reg_;
  int offset_;
};
static StateRemat FromRegister(RegisterID reg) {
  StateRemat sr;
  sr.reg_ = reg;
  return sr;
}
static StateRemat FromAddress3(int address) {
  StateRemat sr;
  sr.offset_ = address;
  if (address < 46 &&    address >= 0) {
    abort();
  }
  return sr;
}
struct FrameState {
  StateRemat dataRematInfo2(bool y, int z) {
    if (y)         return FromRegister(RegisterID(1));
    return FromAddress3(z);
  }
};
FrameState frame;
StateRemat x;
__attribute__((noinline)) void jsop_setelem(bool y, int z) {
  x = frame.dataRematInfo2(y, z);
}
int main(void) {
  jsop_setelem(0, 47);
}
], true,
   ac_have_gcc_pr49911="yes",
   true)
CXXFLAGS="$_SAVE_CXXFLAGS"

AC_LANG_RESTORE

if test "$ac_have_gcc_pr49911" = "yes"; then
   AC_MSG_RESULT(yes)
   CFLAGS="$CFLAGS -fno-tree-vrp"
   CXXFLAGS="$CXXFLAGS -fno-tree-vrp"
else
   AC_MSG_RESULT(no)
fi
fi
])
