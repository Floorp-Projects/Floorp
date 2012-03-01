dnl Set MOZ_FRAMEPTR_FLAGS to the flags that should be used for enabling or
dnl disabling frame pointers in this architecture based on the configure
dnl options

AC_DEFUN([MOZ_SET_FRAMEPTR_FLAGS], [
  case "$target" in
  *android*)
    unwind_tables="-funwind-tables"
    ;;
  esac
  if test "$GNU_CC"; then
    MOZ_ENABLE_FRAME_PTR="-fno-omit-frame-pointer $unwind_tables"
    MOZ_DISABLE_FRAME_PTR="-fomit-frame-pointer"
  else
    case "$target" in
    *-mingw*)
      MOZ_ENABLE_FRAME_PTR="-Oy-"
      MOZ_DISABLE_FRAME_PTR="-Oy"
    ;;
    esac
  fi

  # if we are debugging or profiling, we want a frame pointer.
  if test -z "$MOZ_OPTIMIZE" -o \
          -n "$MOZ_PROFILING" -o -n "$MOZ_DEBUG"; then
    MOZ_FRAMEPTR_FLAGS="$MOZ_ENABLE_FRAME_PTR"
  else
    MOZ_FRAMEPTR_FLAGS="$MOZ_DISABLE_FRAME_PTR"
  fi
])
