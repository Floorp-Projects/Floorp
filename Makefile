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

DEPTH		= .

NSPRDIR		= nsprpub
NSPR20		= 1
MOZILLA_CLIENT	= 1

ifndef NO_MOCHA
DIRS_JS		= js
endif

DIRS		= config coreconf $(NSPRDIR) jpeg dbm xpcom

ifdef MOZ_NETCAST
DIRS		+= netcast
endif

ifdef MOZ_JAVA
DIRS		+= sun-java ldap ifc $(DIRS_JS) nav-java ifc/tools js/jsd
else
DIRS		+= sun-java nav-java $(DIRS_JS)
endif

ifndef NO_SECURITY
DIRS		+= security
endif

DIRS		+= modules lib l10n cmd

ifeq ($(STAND_ALONE_JAVA),1)
DIRS		= config lib/xp $(NSPRDIR) jpeg modules/zlib sun-java ifc js ifc/tools sun-java/java
endif

include $(DEPTH)/config/rules.mk

export:: $(OBJS)

# Running this rule assembles all the SDK source pieces into dist/sdk.
# You'll need to run this rule on every platform to get all the
# binaries (e.g. javah) copied there. You'll also have to do special
# magic on a Mac.
sdk-src::
	$(SDKINSTALL) include/npapi.h $(SDK)/include/
	$(SDKINSTALL) include/jri_md.h $(SDK)/include/
	$(SDKINSTALL) include/jritypes.h $(SDK)/include/
	$(SDKINSTALL) include/jri.h $(SDK)/include/
	$(SDKINSTALL) lib/plugin/npupp.h $(SDK)/include/
	$(SDKINSTALL) sdk/common/*.c* $(SDK)/common/
	$(SDKINSTALL) sun-java/classsrc/$(ZIP_NAME).x $(SDK)/classes/$(ZIP_NAME)
	$(SDKINSTALL) sdk/examples/simple/Source/*.c $(SDK)/examples/simple/Source/
	$(SDKINSTALL) sdk/examples/simple/Source/*.java $(SDK)/examples/simple/Source/
	$(SDKINSTALL) sdk/examples/simple/Source/*.class $(SDK)/examples/simple/Source/
	$(SDKINSTALL) sdk/examples/simple/Source/_gen/*.h $(SDK)/examples/simple/Source/_gen/
	$(SDKINSTALL) sdk/examples/simple/Source/_stubs/*.c $(SDK)/examples/simple/Source/_stubs/
	$(SDKINSTALL) sdk/examples/simple/Unix/makefile.* $(SDK)/examples/simple/Unix/
	$(SDKINSTALL) sdk/examples/simple/Testing/SimpleExample.html $(SDK)/examples/simple/Testing/
	$(SDKINSTALL) sdk/examples/simple/readme.html $(SDK)/examples/simple/
	$(SDKINSTALL) sdk/examples/UnixTemplate/Source/*.c $(SDK)/examples/UnixTemplate/Source/
	$(SDKINSTALL) sdk/examples/UnixTemplate/Testing/Test.html $(SDK)/examples/UnixTemplate/Testing/
	$(SDKINSTALL) sdk/examples/UnixTemplate/Unix/makefile.* $(SDK)/examples/UnixTemplate/Unix/
	$(SDKINSTALL) sdk/examples/UnixTemplate/readme.html $(SDK)/examples/UnixTemplate/

sdk-bin::
	cd sdk; $(MAKE); cd ..
	$(SDKINSTALL) $(DIST)/bin/javah$(BIN_SUFFIX) $(SDK)/bin/$(OS_CONFIG)/
	$(SDKINSTALL) sdk/examples/simple/Source/$(OBJDIR)/npsimple.$(DLL_SUFFIX) $(SDK)/bin/$(OS_CONFIG)/
	$(SDKINSTALL) sdk/examples/UnixTemplate/Source/$(OBJDIR)/nptemplate.$(DLL_SUFFIX) $(SDK)/bin/$(OS_CONFIG)/
