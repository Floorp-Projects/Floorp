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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s): 
# 
# Igor Kushnirskiy idk@eng.sun.com


include $(DEPTH)/config/autoconf.mk
include $(DEPTH)/config/config.mk

ifneq ($(PACKAGE_BUILD),)
	JAVADEV=$(DIST)/javadev
	PLUGLETS_DIR=$(JAVADEV)/example
	MISC_DIR=$(JAVADEV)/misc
	HTML_DIR=$(JAVADEV)/misc
	INSTALL = $(DEPTH)/config/nsinstall -m 666
else 
	PLUGLETS_DIR=$(DIST)/bin/plugins
	MISC_DIR=$(DIST)/bin/res/javadev/pluglets
	HTML_DIR=$(DIST)/bin/res/javadev/pluglets
endif

$(PLUGLET).jar: $(CLASSES) manifest
	$(JDKHOME)/bin/jar cvfm $(PLUGLET).jar manifest *.class

.SUFFIXES: .java .class
.java.class:
	$(JDKHOME)/bin/javac -classpath .:../../classes:$(CLASSPATH):JavaDOM.jar $<
clobber:
	rm -f *.class *.jar

clean : clobber

ifneq ($(PLUGLET),)
      INSTALL_DEPS += install_pluglet
endif

ifneq ($(HTML),)
      INSTALL_DEPS += install_html
endif

ifneq ($(MISC),)
      INSTALL_DEPS += install_misc
endif


install_pluglet: $(PLUGLET).jar
	$(INSTALL) $(PLUGLET).jar $(PLUGLETS_DIR)

install_html :
	$(INSTALL) $(HTML) $(HTML_DIR)

install_misc :
	$(INSTALL) $(MISC) $(MISC_DIR)

install:  $(INSTALL_DEPS)

export: install

