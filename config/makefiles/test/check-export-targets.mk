# -*- makefile -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

ifdef VERBOSE
  $(warning loading test)
endif

MOZILLA_DIR ?= $(topsrcdir)
checkup = \
  check-final-lib-link-list \
  $(NULL)

checkup: $(checkup)


# <CHECK: final-lib-link-list>
LIBRARY_NAME   = foo# real_data: xptcmd
EXPORT_LIBRARY = foo# real_data: ../..
undefine IS_COMPONENT

test-data = $(CURDIR)/check-export-targets-test-data
FINAL_LINK_LIBS     = $(test-data)
STATIC_LIBRARY_NAME = /dev/null

check-final-lib-link-list: export-gen-final-lib-link-list
	@cat $(test-data)
# </CHECK: final-lib-link-list>


include $(topsrcdir)/config/makefiles/target_export.mk
