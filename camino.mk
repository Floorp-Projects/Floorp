# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Camino.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2003
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Brian Ryner <bryner@brianryner.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

CVS = cvs
CWD := $(shell pwd)

ifeq "$(CWD)" "/"
CWD := /.
endif

ifneq (, $(wildcard camino.mk))
# Ran from mozilla directory
ROOTDIR   := $(shell dirname $(CWD))
TOPSRCDIR := $(CWD)
else
# Ran from mozilla/.. directory (?)
ROOTDIR   := $(CWD)
TOPSRCDIR := $(CWD)/mozilla
endif

ifndef MAKE
MAKE := gmake
endif

CVS_ROOT_IN_TREE := $(shell cat $(TOPSRCDIR)/CVS/Root 2>/dev/null)
ifneq ($(CVS_ROOT_IN_TREE),)
ifneq ($(CVS_ROOT_IN_TREE),$(CVSROOT))
  CVS_FLAGS := -d $(CVS_ROOT_IN_TREE)
endif
endif

CVSCO = $(strip $(CVS) $(CVS_FLAGS) co $(CVS_CO_FLAGS))
CVSCO_LOGFILE := $(ROOTDIR)/cvsco.log
CVS_CO_LOGFILE := $(shell echo $(CVSCO_LOGFILE) | sed s%//%/%)

ifdef MOZ_CO_TAG
  CVS_CO_FLAGS := -r $(MOZ_CO_TAG)
endif

ifeq "$(origin MOZ_CVS_FLAGS)" "undefined"
  CVS_FLAGS := $(CVS_FLAGS) -q -z 3
else
  CVS_FLAGS := $(MOZ_CVS_FLAGS)
endif

ifeq "$(origin MOZ_CO_FLAGS)" "undefined"
  CVS_CO_FLAGS := $(CVS_CO_FLAGS) -P
else
  CVS_CO_FLAGS := $(CVS_CO_FLAGS) $(MOZ_CO_FLAGS)
endif

ifdef MOZ_CO_DATE
  CVS_CO_DATE_FLAGS := -D "$(MOZ_CO_DATE)"
endif

# export these so they'll be picked up by client.mk
export MOZ_CO_TAG
export MOZ_CO_FLAGS
export MOZ_CO_DATE

CVSCO_CAMINO = $(CVSCO) $(CVS_CO_DATE_FLAGS) mozilla/camino

all: checkout build

checkout::
	cd $(ROOTDIR) && \
	$(CVSCO) $(CVS_CO_DATE_FLAGS) mozilla/client.mk && \
	$(CVSCO) $(CVS_CO_DATE_FLAGS) mozilla/camino/config/mozconfig
	@cd $(ROOTDIR) && $(MAKE) -f mozilla/client.mk checkout
	cd $(ROOTDIR) && $(CVSCO) $(CVS_CO_DATE_FLAGS) mozilla/camino.mk
	@cd $(ROOTDIR) && $(MAKE) -f mozilla/camino.mk real_checkout

real_checkout:
	@failed=.cvs-failed.tmp; \
	rm -f $$failed; \
	cvs_co() { echo "$$@" ; \
	  ("$$@" || touch $$failed) 2>&1 | tee -a $(CVSCO_LOGFILE) && \
	  if test -f $$failed; then false; else true; fi; }; \
	cvs_co $(CVSCO_CAMINO)
	@echo "camino checkout finish: "`date` | tee -a $(CVSCO_LOGFILE)
	@conflicts=`egrep "^C " $(CVSCO_LOGFILE)` ;\
	if test "$$conflicts" ; then \
	  echo "$(MAKE): *** Conflicts during checkout." ;\
	  echo "$$conflicts" ;\
	  echo "$(MAKE): Refer to $(CVSCO_LOGFILE) for full log." ;\
	  false; \
	else true; \
	fi

build_all_dep: alldep
build_all_depend: alldep

build alldep:
	$(MAKE) -f client.mk $@
	cd `$(MAKE) --no-print-directory -f client.mk echo_objdir` && \
	$(MAKE) -C embedding/config && \
	CONFIG_FILES=camino/Makefile ./config.status && \
	$(MAKE) -C camino

clean distclean:
	$(MAKE) -f client.mk $@
	cd `$(MAKE) --no-print-directory -f client.mk echo_objdir` && $(RM) -rf camino/build

.PHONY: checkout real_checkout build clean distclean
