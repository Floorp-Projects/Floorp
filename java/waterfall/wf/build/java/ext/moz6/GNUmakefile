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
# $Id: GNUmakefile,v 1.1 2001/05/09 17:29:53 edburns%acm.org Exp $
#
# 
# Contributor(s): 
#
#   Nikolay N. Igotti <inn@sparc.spb.su>

BUILDDIR = ../..
JAVASRCDIR = ../../../../java
CLASSDESTDIR = ../../../unix/ext/moz6
ADDCLASSPATH=../../../unix/classes:../../../unix/ext/moz6
PATH_SEP=:

include FileListExt.mk

include $(BUILDDIR)/Defs.gmk
include $(BUILDDIR)/Rules.gmk

all: moz6.jar

moz6.jar: classes
	@if [ -s .classes.list ]; then  \
	echo -n "Creating moz6.jar..." ;\
	cd $(CLASSDESTDIR) ;\
	$(JAR_CMD) cf moz6.jar sun ; \
	$(MV) moz6.jar .. ;\
	echo "   done."; fi


