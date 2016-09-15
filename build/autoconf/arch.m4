dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_ARCH_OPTS],
[

dnl ========================================================
dnl = ARM toolchain tweaks
dnl ========================================================

MOZ_THUMB=toolchain-default
MOZ_THUMB_INTERWORK=toolchain-default
MOZ_FPU=toolchain-default
MOZ_FLOAT_ABI=toolchain-default
MOZ_SOFT_FLOAT=toolchain-default
MOZ_ALIGN=toolchain-default

MOZ_ARG_WITH_STRING(arch,
[  --with-arch=[[type|toolchain-default]]
                           Use specific CPU features (-march=type). Resets
                           thumb, fpu, float-abi, etc. defaults when set],
    if test -z "$GNU_CC"; then
        AC_MSG_ERROR([--with-arch is not supported on non-GNU toolchains])
    fi
    MOZ_ARCH=$withval)

if test -z "$MOZ_ARCH"; then
    dnl Defaults
    case "${CPU_ARCH}-${OS_TARGET}" in
    arm-Android)
        MOZ_THUMB=yes
        MOZ_ARCH=armv7-a
        MOZ_FPU=vfp
        MOZ_FLOAT_ABI=softfp
        MOZ_ALIGN=no
        ;;
    arm-Darwin)
        MOZ_ARCH=toolchain-default
        ;;
    esac
fi

if test "$MOZ_ARCH" = "armv6" -a "$OS_TARGET" = "Android"; then
   MOZ_FPU=vfp
   MOZ_FLOAT_ABI=softfp
fi

MOZ_ARG_WITH_STRING(thumb,
[  --with-thumb[[=yes|no|toolchain-default]]]
[                          Use Thumb instruction set (-mthumb)],
    if test -z "$GNU_CC"; then
        AC_MSG_ERROR([--with-thumb is not supported on non-GNU toolchains])
    fi
    MOZ_THUMB=$withval)

MOZ_ARG_WITH_STRING(thumb-interwork,
[  --with-thumb-interwork[[=yes|no|toolchain-default]]
                           Use Thumb/ARM instuctions interwork (-mthumb-interwork)],
    if test -z "$GNU_CC"; then
        AC_MSG_ERROR([--with-thumb-interwork is not supported on non-GNU toolchains])
    fi
    MOZ_THUMB_INTERWORK=$withval)

MOZ_ARG_WITH_STRING(fpu,
[  --with-fpu=[[type|toolchain-default]]
                           Use specific FPU type (-mfpu=type)],
    if test -z "$GNU_CC"; then
        AC_MSG_ERROR([--with-fpu is not supported on non-GNU toolchains])
    fi
    MOZ_FPU=$withval)

MOZ_ARG_WITH_STRING(float-abi,
[  --with-float-abi=[[type|toolchain-default]]
                           Use specific arm float ABI (-mfloat-abi=type)],
    if test -z "$GNU_CC"; then
        AC_MSG_ERROR([--with-float-abi is not supported on non-GNU toolchains])
    fi
    MOZ_FLOAT_ABI=$withval)

MOZ_ARG_WITH_STRING(soft-float,
[  --with-soft-float[[=yes|no|toolchain-default]]
                           Use soft float library (-msoft-float)],
    if test -z "$GNU_CC"; then
        AC_MSG_ERROR([--with-soft-float is not supported on non-GNU toolchains])
    fi
    MOZ_SOFT_FLOAT=$withval)

case "$MOZ_ARCH" in
toolchain-default|"")
    arch_flag=""
    ;;
*)
    arch_flag="-march=$MOZ_ARCH"
    ;;
esac

case "$MOZ_THUMB" in
yes)
    MOZ_THUMB2=1
    thumb_flag="-mthumb"
    ;;
no)
    MOZ_THUMB2=
    thumb_flag="-marm"
    ;;
*)
    _SAVE_CFLAGS="$CFLAGS"
    CFLAGS="$arch_flag"
    AC_TRY_COMPILE([],[return sizeof(__thumb2__);],
        MOZ_THUMB2=1,
        MOZ_THUMB2=)
    CFLAGS="$_SAVE_CFLAGS"
    thumb_flag=""
    ;;
esac

if test "$MOZ_THUMB2" = 1; then
    AC_DEFINE(MOZ_THUMB2)
fi

case "$MOZ_THUMB_INTERWORK" in
yes)
    thumb_interwork_flag="-mthumb-interwork"
    ;;
no)
    thumb_interwork_flag="-mno-thumb-interwork"
    ;;
*) # toolchain-default
    thumb_interwork_flag=""
    ;;
esac

case "$MOZ_FPU" in
toolchain-default|"")
    fpu_flag=""
    ;;
*)
    fpu_flag="-mfpu=$MOZ_FPU"
    ;;
esac

case "$MOZ_FLOAT_ABI" in
toolchain-default|"")
    float_abi_flag=""
    ;;
*)
    float_abi_flag="-mfloat-abi=$MOZ_FLOAT_ABI"
    ;;
esac

case "$MOZ_SOFT_FLOAT" in
yes)
    soft_float_flag="-msoft-float"
    ;;
no)
    soft_float_flag="-mno-soft-float"
    ;;
*) # toolchain-default
    soft_float_flag=""
    ;;
esac

case "$MOZ_ALIGN" in
no)
    align_flag="-mno-unaligned-access"
    ;;
yes)
    align_flag="-munaligned-access"
    ;;
*)
    align_flag=""
    ;;
esac

if test -n "$align_flag"; then
  _SAVE_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $align_flag"
  AC_MSG_CHECKING(whether alignment flag ($align_flag) is supported)
  AC_TRY_COMPILE([],[],,align_flag="")
  CFLAGS="$_SAVE_CFLAGS"
fi

dnl Use echo to avoid accumulating space characters
all_flags=`echo $arch_flag $thumb_flag $thumb_interwork_flag $fpu_flag $float_abi_flag $soft_float_flag $align_flag`
if test -n "$all_flags"; then
    _SAVE_CFLAGS="$CFLAGS"
    CFLAGS="$all_flags"
    AC_MSG_CHECKING(whether the chosen combination of compiler flags ($all_flags) works)
    AC_TRY_COMPILE([],[return 0;],
        AC_MSG_RESULT([yes]),
        AC_MSG_ERROR([no]))

    CFLAGS="$_SAVE_CFLAGS $all_flags"
    CXXFLAGS="$CXXFLAGS $all_flags"
    ASFLAGS="$ASFLAGS $all_flags"
    if test -n "$thumb_flag"; then
        LDFLAGS="$LDFLAGS $thumb_flag"
    fi
fi

AC_SUBST(MOZ_THUMB2)

if test "$CPU_ARCH" = "arm"; then
  NEON_FLAGS="-mfpu=neon"
  AC_MSG_CHECKING(for ARM SIMD support in compiler)
  # We try to link so that this also fails when
  # building with LTO.
  AC_TRY_LINK([],
                 [asm("uqadd8 r1, r1, r2");],
                 result="yes", result="no")
  AC_MSG_RESULT("$result")
  if test "$result" = "yes"; then
      AC_DEFINE(HAVE_ARM_SIMD)
      HAVE_ARM_SIMD=1
  fi

  AC_MSG_CHECKING(ARM version support in compiler)
  dnl Determine the target ARM architecture (5 for ARMv5, v5T, v5E, etc.; 6 for ARMv6, v6K, etc.)
  ARM_ARCH=`${CC-cc} ${CFLAGS} -dM -E - < /dev/null | sed -n 's/.*__ARM_ARCH_\([[0-9]][[0-9]]*\).*/\1/p'`
  AC_MSG_RESULT("$ARM_ARCH")

  AC_MSG_CHECKING(for ARM NEON support in compiler)
  # We try to link so that this also fails when
  # building with LTO.
  AC_TRY_LINK([],
                 [asm(".fpu neon\n vadd.i8 d0, d0, d0");],
                 result="yes", result="no")
  AC_MSG_RESULT("$result")
  if test "$result" = "yes"; then
      AC_DEFINE(HAVE_ARM_NEON)
      HAVE_ARM_NEON=1

      dnl We don't need to build NEON support if we're targetting a non-NEON device.
      dnl This matches media/webrtc/trunk/webrtc/build/common.gypi.
      if test -n "$ARM_ARCH"; then
          if test "$ARM_ARCH" -lt 7; then
              BUILD_ARM_NEON=
          else
              AC_DEFINE(BUILD_ARM_NEON)
              BUILD_ARM_NEON=1
          fi
      fi
  fi

fi # CPU_ARCH = arm

AC_SUBST(HAVE_ARM_SIMD)
AC_SUBST(HAVE_ARM_NEON)
AC_SUBST(BUILD_ARM_NEON)
AC_SUBST(ARM_ARCH)
AC_SUBST_LIST(NEON_FLAGS)

])
