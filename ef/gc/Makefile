#! gmake
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

DEPTH=..

DIRS=include src test

include $(DEPTH)/config/rules.mk

NSPR_HEADERS = $(wildcard $(DEPTH)/nspr20/pr/include/*.h)
NSPR_PRIV = $(wildcard $(DEPTH)/nspr20/pr/include/private/*.h)
NSPR_MD = $(wildcard $(DEPTH)/nspr20/pr/include/md/*.h)
NSPR_OBS = $(wildcard $(DEPTH)/nspr20/pr/include/obsolete/*.h)

import: 
	$(INSTALL) -m 444 $(NSPR_HEADERS) $(PUBLIC)/nspr20/
	$(INSTALL) -m 444 $(NSPR_PRIV) $(PUBLIC)/nspr20/private/
	$(INSTALL) -m 444 $(NSPR_MD) $(PUBLIC)/nspr20/md/
	$(INSTALL) -m 444 $(NSPR_OBS) $(PUBLIC)/nspr20/obsolete/
	$(INSTALL) -m 444 $(DIST)/lib/libnspr21.so $(DIST)/bin/libnspr21.so
	$(INSTALL) -m 444 $(DIST)/include/prcpucfg.h $(PUBLIC)/nspr20/
