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
define([AC_OUTPUT_SUBDIRS],
[trap '' EXIT
_MOZ_AC_OUTPUT_SUBDIRS($1)
MOZ_CONFIG_LOG_TRAP
])
