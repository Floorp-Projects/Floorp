# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef INCLUDED_VERSION_MK
INCLUDED_VERSION_MK=1

# Windows gmake build:
# Build default .rc file if $(RESFILE) isn't defined.
# TODO:
# PBI      : Private build info.  Not used currently.
#            Guessing the best way would be to set an env var.
# BINARY   : Binary name.  Not used currently.
ifeq ($(MOZ_WIDGET_TOOLKIT),windows)
ifndef RESFILE
RCFILE=./module.rc
RESFILE=./module.res
ifdef PROGRAM
_RC_BINARY = $(notdir $(PROGRAM))
else
ifdef _PROGRAM
_RC_BINARY = $(notdir $(_PROGRAM))
else
ifdef SHARED_LIBRARY
_RC_BINARY = $(notdir $(SHARED_LIBRARY))
endif
endif
endif

GARBAGE += $(RESFILE) $(RCFILE)

#dummy target so $(RCFILE) doesn't become the default =P
all::

$(RCFILE): $(RCINCLUDE) $(MOZILLA_DIR)/config/version_win.py
	$(PYTHON3) $(MOZILLA_DIR)/config/version_win.py '$(_RC_BINARY)' '$(RCINCLUDE)'

endif  # RESFILE
endif  # Windows

endif
