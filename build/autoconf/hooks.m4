dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Output the contents of config.log when configure exits with an
dnl error code.
define([MOZ_CONFIG_LOG_TRAP],
[changequote(<<<, >>>)dnl
trap '[ "$?" != 0 ] && echo "------ config.log ------" && tail -n 25 config.log' EXIT
changequote([, ])dnl
])

dnl Wrap AC_INIT_PREPARE to add the above trap.
define([_MOZ_AC_INIT_PREPARE], defn([AC_INIT_PREPARE]))
define([AC_INIT_PREPARE],
[_MOZ_AC_INIT_PREPARE($1)
MOZ_CONFIG_LOG_TRAP
> subconfigures
])

define([AC_OUTPUT_SUBDIRS],
[for moz_config_dir in $1; do
  _CONFIG_SHELL=${CONFIG_SHELL-/bin/sh}
  case "$moz_config_dir" in
  *:*)
    objdir=$(echo $moz_config_dir | awk -F: '{print [$]2}')
    ;;
  *)
    objdir=$moz_config_dir
    ;;
  esac
  dnl Because config.status, storing AC_SUBSTs, is written before any
  dnl subconfigure runs, we need to use a file. Moreover, some subconfigures
  dnl are started from a subshell, and variable modifications from a subshell
  dnl wouldn't be preserved.
  echo $objdir >> subconfigures

  dumpenv="true | "
  case "$host" in
  *-mingw*)
    _CONFIG_SHELL=$(cd $(dirname $_CONFIG_SHELL); pwd -W)/$(basename $_CONFIG_SHELL)
    if test ! -e "$_CONFIG_SHELL" -a -e "${_CONFIG_SHELL}.exe"; then
        _CONFIG_SHELL="${_CONFIG_SHELL}.exe"
    fi
    dnl Yes, this is horrible. But since msys doesn't preserve environment
    dnl variables and command line arguments as they are when transitioning
    dnl from msys (this script) to python (below), we have to resort to hacks,
    dnl storing the environment and command line arguments from a msys process
    dnl (perl), and reading it from python.
    dumpenv="$PERL $_topsrcdir/build/win32/dumpenv4python.pl $ac_configure_args | "
    ;;
  esac

  eval $dumpenv $PYTHON $_topsrcdir/build/subconfigure.py --prepare "$srcdir" "$moz_config_dir" "$_CONFIG_SHELL" $ac_configure_args ifelse($2,,,--cache-file="$2")

  dnl Execute subconfigure, unless --no-recursion was passed to configure.
  if test "$no_recursion" != yes; then
    trap '' EXIT
    if ! $PYTHON $_topsrcdir/build/subconfigure.py "$objdir"; then
        exit 1
    fi
    MOZ_CONFIG_LOG_TRAP
  fi
done
])

dnl Print error messages in config.log as well as stderr
define([AC_MSG_ERROR],
[{ echo "configure: error: $1" 1>&2; echo "configure: error: $1" 1>&5; exit 1; }])
