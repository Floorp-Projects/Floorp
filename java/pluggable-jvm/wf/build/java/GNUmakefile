# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#  
# The Original Code is The Waterfall Java Plugin Module
#  
# The Initial Developer of the Original Code is Sun Microsystems Inc
# Portions created by Sun Microsystems Inc are Copyright (C) 2001
# All Rights Reserved.
# 
# $Id: GNUmakefile,v 1.3 2001/07/12 19:57:33 edburns%acm.org Exp $
# 
# Contributor(s):
# 
#     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
# 

ifndef WFJDKHOME
all classes:
	@echo
	@echo "Set your WFJDKHOME to path to JDK!"
	@echo
else	

BUILDDIR = .
JAVASRCDIR = ../../java
CLASSDESTDIR = ../unix/classes
ADDCLASSPATH = ../unix/classes
SUBDIRS = ext

default: all

include FileListCommon.mk
include FileListUnix.mk

include $(BUILDDIR)/Defs.gmk
include $(BUILDDIR)/Rules.gmk
endif

all: classes exts

release: all wf.jar

wf.jar: 
	@echo -n "Making JAR file....."; \
	cd $(CLASSDESTDIR); \
	$(JAR_CMD) cf wf.jar sun netscape ; \
	$(MV) wf.jar ..; \
	echo "  done."

exts:
	@echo "Making Waterfall extension components"
	@cd ext; \
	$(MAKE)  all

.PHONY : all classes clean exts
