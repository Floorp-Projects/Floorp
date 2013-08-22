# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The purpose of this file is to pull in non-recursive targets when performing
# a partial tree (not top-level) build. This will allow people to continue to
# build individual directories while some of the targets may not be normally
# defined in that make file.
#
# Non-recursive targets are attached to existing make targets. The
# NONRECURSIVE_TARGETS variable lists the make targets that modified. For
# each target in this list, the NONRECURSIVE_TARGET_<target> variable will
# contain a list of partial variable names. We will then look in variables
# named NONRECURSIVE_TARGETS_<target>_<fragment>_* for information describing
# how to evaluate non-recursive make targets.
#
# Targets are defined by the following variables:
#
#   FILE - The make file to evaluate. This is equivalent to
#      |make -f <FILE>|
#   DIRECTORY - The directory whose Makefile to evaluate. This is
#      equivalent to |make -C <DIRECTORY>|.
#   TARGETS - Targets to evaluate in that make file.
#
# Only 1 of FILE or DIRECTORY may be defined.
#
# For example:
#
# NONRECURSIVE_TARGETS = export libs
# NONRECURSIVE_TARGETS_export = headers
# NONRECURSIVE_TARGETS_export_headers_FILE = /path/to/exports.mk
# NONRECURSIVE_TARGETS_export_headers_TARGETS = $(DIST)/include/foo.h $(DIST)/include/bar.h
# NONRECURSIVE_TARGETS_libs = cppsrcs
# NONRECURSIVE_TARGETS_libs_cppsrcs_DIRECTORY = $(DEPTH)/foo
# NONRECURSIVE_TARGETS_libs_cppsrcs_TARGETS = /path/to/foo.o /path/to/bar.o
#
# Will get turned into the following:
#
# exports::
#     $(MAKE) -C $(DEPTH) -f /path/to/exports.mk $(DIST)/include/foo.h $(DIST)/include/bar.h
#
# libs::
#     $(MAKE) -C $(DEPTH)/foo /path/to/foo.o /path/to/bar.o

ifndef INCLUDED_NONRECURSIVE_MK

define define_nonrecursive_target
$(1)::
	$$(MAKE) -C $(or $(4),$$(DEPTH)) $(addprefix -f ,$(3)) $(2)
endef

$(foreach target,$(NONRECURSIVE_TARGETS), \
    $(foreach entry,$(NONRECURSIVE_TARGETS_$(target)), \
        $(eval $(call define_nonrecursive_target, \
            $(target), \
            $(NONRECURSIVE_TARGETS_$(target)_$(entry)_TARGETS), \
            $(NONRECURSIVE_TARGETS_$(target)_$(entry)_FILE), \
            $(NONRECURSIVE_TARGETS_$(target)_$(entry)_DIRECTORY), \
        )) \
    ) \
)

INCLUDED_NONRECURSIVE_MK := 1
endif

