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

# Run a named Python build action. The first argument is the name of the build
# action. The second argument are the arguments to pass to the action (space
# delimited arguments). e.g.
#
#   libs::
#       $(call py_action,purge_manifests,_build_manifests/purge/foo.manifest)
ifdef .PYMAKE
py_action = %mozbuild.action.$(1) main $(2)
else
py_action = $(PYTHON) -m mozbuild.action.$(1) $(2)
endif
