#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

DEPTH	= ..

include $(DEPTH)/config/config.mk

ifndef NO_X11
DIRS = xfe
else

# Rhapsody needs to be able to go to both xfe (Mozilla/x11)
# and ybfe (NGLayout).
ifeq ($(OS_ARCH),Rhapsody)
DIRS = ybfe
endif

endif

include $(DEPTH)/config/rules.mk

