dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

define(GENERATE_SUBDIRS_ABS, [
define([AC_OUTPUT_SUBDIRS], [
patsubst($@, [/\*)], [/* | ?:/*)])
])
])
GENERATE_SUBDIRS_ABS(defn([AC_OUTPUT_SUBDIRS]))
