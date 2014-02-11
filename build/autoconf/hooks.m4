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
])

dnl Disable the trap when running sub-configures.
define([_MOZ_AC_OUTPUT_SUBDIRS], defn([AC_OUTPUT_SUBDIRS]))
define([MOZ_SUBCONFIGURE_WRAP],
[ _CONFIG_SHELL=${CONFIG_SHELL-/bin/sh}
case "$host" in
*-mingw*)
    _CONFIG_SHELL=$(cd $(dirname $_CONFIG_SHELL); pwd -W)/$(basename $_CONFIG_SHELL)
    if test ! -e "$_CONFIG_SHELL" -a -e "${_CONFIG_SHELL}.exe"; then
        _CONFIG_SHELL="${_CONFIG_SHELL}.exe"
    fi
    ;;
esac

if test -d "$1"; then
    (cd "$1"; $PYTHON $_topsrcdir/build/subconfigure.py dump "$_CONFIG_SHELL")
else
    mkdir -p "$1"
fi
$2
(cd "$1"; $PYTHON $_topsrcdir/build/subconfigure.py adjust $ac_sub_configure)
])

define([AC_OUTPUT_SUBDIRS],
[trap '' EXIT
for moz_config_dir in $1; do
  MOZ_SUBCONFIGURE_WRAP([$moz_config_dir],[
    _MOZ_AC_OUTPUT_SUBDIRS($moz_config_dir)
  ])
done

MOZ_CONFIG_LOG_TRAP
])

dnl Print error messages in config.log as well as stderr
define([AC_MSG_ERROR],
[{ echo "configure: error: $1" 1>&2; echo "configure: error: $1" 1>&5; exit 1; }])
