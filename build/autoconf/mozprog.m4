dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_PROG_CHECKMSYS],
[AC_REQUIRE([AC_INIT_BINSH])dnl
if test `uname -s | grep -c MINGW 2>/dev/null` != "0"; then
  msyshost=1
fi
])

AC_DEFUN([MOZ_PATH_PROG],
[ AC_PATH_PROG($1,$2,$3,$4)
  if test "$msyshost"; then
    case "[$]$1" in
    /*)
      tmp_DIRNAME=`dirname "[$]$1"`
      tmp_BASENAME=`basename "[$]$1"`
      tmp_PWD=`cd "$tmp_DIRNAME" && pwd -W`
      $1="$tmp_PWD/$tmp_BASENAME"
      if test -e "[$]$1.exe"; then
        $1="[$]$1.exe"
      fi
    esac
  fi
])

AC_DEFUN([MOZ_PATH_PROGS],
[  AC_PATH_PROGS($1,$2,$3,$4)
  if test "$msyshost"; then
    case "[$]$1" in
    /*)
      tmp_DIRNAME=`dirname "[$]$1"`
      tmp_BASENAME=`basename "[$]$1"`
      tmp_PWD=`cd "$tmp_DIRNAME" && pwd -W`
      $1="$tmp_PWD/$tmp_BASENAME"
      if test -e "[$]$1.exe"; then
        $1="[$]$1.exe"
      fi
    esac
  fi
])
