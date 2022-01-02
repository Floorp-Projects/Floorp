dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

dnl Wrap AC_INIT_PREPARE to add the above trap.
define([_MOZ_AC_INIT_PREPARE], defn([AC_INIT_PREPARE]))
define([AC_INIT_PREPARE],
[_MOZ_AC_INIT_PREPARE($1)

test "x$prefix" = xNONE && prefix=$ac_default_prefix
# Let make expand exec_prefix.
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
])

dnl Print error messages in config.log as well as stderr
define([AC_MSG_ERROR],
[{ echo "configure: error: $1" 1>&2; echo "configure: error: $1" 1>&5; exit 1; }])

dnl Divert AC_TRY_COMPILER to make ac_cv_prog_*_works actually cached.
dnl This will allow to just skip the test when python configure has set
dnl the value for us. But since ac_cv_prog_*_cross is calculated at the same
dnl time, and has a different meaning as in python configure, we only want to
dnl use its value to display whether a cross-compile is happening. We forbid
dnl configure tests that would rely on ac_cv_prog_*_cross autoconf meaning
dnl (being able to execute the product of compilation), which are already bad
dnl for cross compiles anyways, so it's a win to get rid of them.
define([_MOZ_AC_TRY_COMPILER], defn([AC_TRY_COMPILER]))
define([AC_TRY_COMPILER], [AC_CACHE_VAL($2, _MOZ_AC_TRY_COMPILER($1, $2, $3))])

define([AC_TRY_RUN], [m4_fatal([AC_TRY_RUN is forbidden])])
define([AC_CHECK_FILE], [m4_fatal([AC_CHECK_FILE is forbidden])])
