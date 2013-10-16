#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# functions.mk
#
# Defines functions that are needed by various Makefiles throughout the build
# system, which are needed before config.mk can be included.
#

# Define an include-at-most-once flag
ifdef INCLUDED_FUNCTIONS_MK
$(error Do not include functions.mk twice!)
endif
INCLUDED_FUNCTIONS_MK = 1

core_abspath = $(error core_abspath is unsupported, use $$(abspath) instead)
core_realpath = $(error core_realpath is unsupported)

core_winabspath = $(error core_winabspath is unsupported)
