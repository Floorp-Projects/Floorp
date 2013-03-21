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
# else- determine relative path
mochitestdir = \
    $(strip \
      $(if $(2),$(DEPTH)/_tests/testing/mochitest/$1/. \
        ,$(if $(value relativesrcdir) \
          ,$(DEPTH)/_tests/testing/mochitest/$1/$(relativesrcdir) \
          ,$(DEPTH)/_tests/testing/mochitest/$1/$(subst $(topsrcdir),,$(srcdir)) \
    )))


define mochitest-libs-rule-template
libs:: $$($(1))
	$$(call install_cmd,$$(foreach f,$$^,"$$(f)") $$(call mochitestdir,$(2),$(3)))
endef

# Provide support for modules with such a large number of tests that
# installing them with a single $(INSTALL) invocation would overflow
# command-line length limits on some operating systems.
ifdef MOCHITEST_FILES_PARTS
ifdef MOCHITEST_FILES
$(error You must define only one of MOCHITEST_FILES_PARTS or MOCHITEST_FILES)
endif
$(foreach part,$(MOCHITEST_FILES_PARTS),$(eval $(call mochitest-libs-rule-template,$(part),tests)))
endif

ifdef MOCHITEST_FILES
$(eval $(call mochitest-libs-rule-template,MOCHITEST_FILES,tests))
endif

ifdef MOCHITEST_CHROME_FILES
$(eval $(call mochitest-libs-rule-template,MOCHITEST_CHROME_FILES,chrome))
endif

ifdef MOCHITEST_BROWSER_FILES_PARTS
ifdef MOCHITEST_BROWSER_FILES
$(error You must define only one of MOCHITEST_BROWSER_FILES_PARTS or MOCHITEST_BROWSER_FILES)
endif
$(foreach part,$(MOCHITEST_BROWSER_FILES_PARTS),$(eval $(call mochitest-libs-rule-template,$(part),browser)))
endif

ifdef MOCHITEST_BROWSER_FILES
$(eval $(call mochitest-libs-rule-template,MOCHITEST_BROWSER_FILES,browser))
endif

ifdef MOCHITEST_A11Y_FILES
$(eval $(call mochitest-libs-rule-template,MOCHITEST_A11Y_FILES,a11y))
endif

ifdef MOCHITEST_ROBOCOP_FILES
$(eval $(call mochitest-libs-rule-template,MOCHITEST_ROBOCOP_FILES,tests/robocop,flat_hierarchy))
endif

ifdef MOCHITEST_WEBAPPRT_CHROME_FILES
$(eval $(call mochitest-libs-rule-template,MOCHITEST_WEBAPPRT_CHROME_FILES,webapprtChrome))
endif

INCLUDED_TESTS_MOCHITEST_MK := 1

endif #} INCLUDED_TESTS_MOCHITEST_MK
