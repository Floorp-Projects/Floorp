# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef INCLUDED_TESTS_MOCHITEST_MK #{

#   $1- test directory name
#   $2- optional: if passed dot used to flatten directory hierarchy copy
# else- relativesrcdir
mochitestdir = \
    $(strip \
      $(if $(2),$(DEPTH)/_tests/testing/mochitest/$1/. \
        ,$(DEPTH)/_tests/testing/mochitest/$1/$(relativesrcdir) \
    ))


ifdef MOCHITEST_FILES
MOCHITEST_DEST := $(call mochitestdir,tests)
INSTALL_TARGETS += MOCHITEST
endif

ifdef MOCHITEST_CHROME_FILES
MOCHITEST_CHROME_DEST := $(call mochitestdir,chrome)
INSTALL_TARGETS += MOCHITEST_CHROME
endif

ifdef MOCHITEST_BROWSER_FILES
MOCHITEST_BROWSER_DEST := $(call mochitestdir,browser)
INSTALL_TARGETS += MOCHITEST_BROWSER
endif

ifdef MOCHITEST_A11Y_FILES
MOCHITEST_A11Y_DEST := $(call mochitestdir,a11y)
INSTALL_TARGETS += MOCHITEST_A11Y
endif

ifdef MOCHITEST_METRO_FILES
MOCHITEST_METRO_DEST := $(call mochitestdir,metro)
INSTALL_TARGETS += MOCHITEST_METRO
endif

ifdef MOCHITEST_ROBOCOP_FILES
MOCHITEST_ROBOCOP_DEST := $(call mochitestdir,tests/robocop,flat_hierarchy)
INSTALL_TARGETS += MOCHITEST_ROBOCOP
endif

INCLUDED_TESTS_MOCHITEST_MK := 1

endif #} INCLUDED_TESTS_MOCHITEST_MK
