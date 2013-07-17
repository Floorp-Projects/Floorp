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

core_abspath = $(if $(findstring :,$(1)),$(1),$(if $(filter /%,$(1)),$(1),$(CURDIR)/$(1)))
core_realpath = $(if $(realpath $(1)),$(realpath $(1)),$(call core_abspath,$(1)))

core_winabspath = $(firstword $(subst /, ,$(call core_abspath,$(1)))):$(subst $(space),,$(patsubst %,\\%,$(wordlist 2,$(words $(subst /, ,$(call core_abspath,$(1)))), $(strip $(subst /, ,$(call core_abspath,$(1)))))))
