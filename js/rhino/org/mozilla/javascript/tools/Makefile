#! gmake
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
# Copyright (C) 1999 Netscape Communications Corporation.  All Rights
# Reserved.

#
# Makefile for the javascript tools.  
#
# This Makefile is notintended to be called from the toplevel Makefile.
#

# Might be good to make the file list explicit to keep garbage from
# creeping in.
SOURCES = $(PATH_PREFIX)/*.java \
	$(PATH_PREFIX)/shell/*.java \
 	$(PATH_PREFIX)/jsc/*.java

RESOURCEDIR = $(PATH_PREFIX)/resources
RESOURCES = $(RESOURCEDIR)/*.properties

# This must be evaluated in some context where the classes can be
# found; we can't use a simple translation from sources, because a
# .java file may produce more than one .class file.
CLASSES = $(PATH_PREFIX)/*.class \
	$(PATH_PREFIX)/shell/*.class \
 	$(PATH_PREFIX)/jsc/*.class

# A class or set of classes as visible from the top level.  For want
# of ${subst ,,}.  This variable is only used to trigger dependency
# analysis, and multiple words confuse gmake, so it can be smaller
# than the full set of sources.  (We assume we'll never need to do the
# same thing with RESOURCES.)
TLCLASS = $(CLASSDIR)/$(PATH_PREFIX)/*.class


$(JAR) : $(TLCLASS) $(CLASSDIR)/$(RESOURCES)
	cd $(CLASSDIR) ; \
		jar cf ../$(JAR) $(CLASSES) $(RESOURCES)

$(TLCLASS) : $(SOURCES)
	- mkdir -p $(CLASSDIR)
	$(JAVAC) $(JFLAGS) -d $(CLASSDIR) $(SOURCES)

$(CLASSDIR)/$(RESOURCES) : $(RESOURCES)
	- mkdir -p $(CLASSDIR)/$(RESOURCEDIR)
	cp $(RESOURCES) $(CLASSDIR)/$(RESOURCEDIR)

clean : 
 	- cd $(CLASSDIR)
 	- rm $(CLASSES)
	- rm $(PATH_PREFIX)/message.ids \
		$(PATH_PREFIX)/property.ids \
		$(PATH_PREFIX)/MANIFEST

clobber : clean
	- rm $(JAR)

$(PATH_PREFIX)/MANIFEST : $(SOURCES) $(RESOURCES) $(PATH_PREFIX)/Makefile
	ls $(SOURCES) $(RESOURCES) $(PATH_PREFIX)/Makefile \
		> $(@)

# A sed/grep regular expression.
MESSAGE_PREFIX = msg\.

$(PATH_PREFIX)/message.ids : $(SOURCES)
	grep '$(MESSAGE_PREFIX)' $(SOURCES) |\
	sed -e 's/.*\"\($(MESSAGE_PREFIX)\)\([^\"]*\).*/\1\2/' | \
	sort | uniq > $(PATH_PREFIX)/message.ids

$(PATH_PREFIX)/property.ids : $(RESOURCES)
	grep '$(MESSAGE_PREFIX)' $(RESOURCES) |\
	sed -e 's/.*\($(MESSAGE_PREFIX)\)\([^ =]*\).*/\1\2/' |\
	sort > $(PATH_PREFIX)/property.ids

check : $(PATH_PREFIX)/message.ids $(PATH_PREFIX)/property.ids FORCE
	- diff $(PATH_PREFIX)/message.ids $(PATH_PREFIX)/property.ids
# look for unmatched single quotes ... seems to fail when none!
#	- sed -e s/\'\'//g $($RESOURCES) | grep \'

# Emulate .PHONY
FORCE :
