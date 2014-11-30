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
_RC_STRING = -QUIET 1 -DEPTH $(DEPTH) -TOPSRCDIR $(MOZILLA_DIR) -OBJDIR . -SRCDIR $(srcdir) -DISPNAME $(MOZ_APP_DISPLAYNAME) -APPVERSION $(MOZ_APP_VERSION)
ifdef MOZILLA_OFFICIAL
_RC_STRING += -OFFICIAL 1
endif
ifdef MOZ_DEBUG
_RC_STRING += -DEBUG 1
endif
ifdef PROGRAM
_RC_STRING += -BINARY $(PROGRAM)
else
ifdef _PROGRAM
_RC_STRING += -BINARY $(_PROGRAM)
else
ifdef SHARED_LIBRARY
_RC_STRING += -BINARY $(SHARED_LIBRARY)
endif
endif
endif
ifdef RCINCLUDE
_RC_STRING += -RCINCLUDE $(srcdir)/$(RCINCLUDE)
endif

GARBAGE += $(RESFILE) $(RCFILE)

#dummy target so $(RCFILE) doesn't become the default =P
all::

$(RCFILE): $(RCINCLUDE) $(MOZILLA_DIR)/config/version_win.pl
	$(PERL) $(MOZILLA_DIR)/config/version_win.pl $(_RC_STRING)

endif  # RESFILE
endif  # Windows

endif
