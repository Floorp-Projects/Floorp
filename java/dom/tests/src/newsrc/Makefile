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


HOMEDIR=../..
SRCDIR=$(HOMEDIR)/src/newsrc
CLASSDIR=$(HOMEDIR)/classes

JAVAC=javac
TESTPACKAGE=org/mozilla/dom/test
DOMPACKAGE=org/mozilla/dom
DOMPLUGLET_JAR=DOMPLUGLET.jar




all: 
	@echo "******************************************************"
	@echo "Make changes to $(SRCDIR)/mozilla.csh"
	@echo "and source it else it may not compile..."
	@echo "******************************************************"
	@echo ""
	$(JAVAC) -d $(CLASSDIR) $(JAVAFILES) 
	/bin/cp manifest $(CLASSDIR)
	cd $(CLASSDIR); jar cvfm $(DOMPLUGLET_JAR) manifest org
	cd $(CLASSDIR); /bin/cp $(DOMPLUGLET_JAR) $(MOZILLA_FIVE_HOME)/../classes; rm -rf *.class

JAVAFILES= DOMViewerFactory.java

clean:
	for i in ${JAVAFILES} ; do \
		file=$$i; \
		classfile=`echo $$file | sed 's/java/class/g'` ; \
		/bin/rm $(CLASSDIR)/$(TESTPACKAGE)/$$classfile ; done
