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
# $Id: GNUmakefile,v 1.1 2001/07/12 20:25:24 edburns%acm.org Exp $
# 
# Contributor(s):
# 
#     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
# 

BUILDDIR = ../..
JAVASRCDIR = ../../../../java
CLASSDESTDIR = ../../../unix/ext/av
ADDCLASSPATH=../../../unix/classes:../../../unix/ext/av

include FileListExt.mk

include $(BUILDDIR)/Defs.gmk
include $(BUILDDIR)/Rules.gmk

all: av.jar

av.jar: classes
	@if [ -s .classes.list ]; then  \
	echo -n "Creating av.jar..."; \
	cd $(CLASSDESTDIR); \
	$(RM) manifest.tmp; \
	echo "Factory-Class: sun.jvmp.av.AVFactory" >> manifest.tmp ; \
	$(JAR_CMD) cfm av.jar manifest.tmp sun ; \
	$(MV) av.jar .. ;\
	echo "   done."; fi


