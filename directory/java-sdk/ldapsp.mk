# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#


#
# Makefile for the LDAPSP classes
#
# A debug compile (java -g) is done by default. You can specify "DEBUG=0" on
# the make line to compile with '-O' option.
#

ARCH := $(shell uname -s)

MCOM_ROOT=.
ifeq ($(ARCH), WINNT)
   MOZ_DIR:=$(subst \,/,$(MOZ_SRC))
   BASEDIR:=$(MOZ_DIR)/mozilla/directory/java-sdk
else
  ifeq ($(ARCH), WIN95)
    MOZ_DIR:=$(subst \,/,$(MOZ_SRC))
    BASEDIR:=$(MOZ_DIR)/mozilla/directory/java-sdk
  else
    BASEDIR := $(shell cd $(MCOM_ROOT); pwd)
  endif
endif
# Destination for class files and packages
CLASS_DEST=$(BASEDIR)/dist/classes

ifeq ($(ARCH), WINNT)
  JDK := $(subst \,/,$(JAVA_HOME))
  SEP=;
else
  ifeq ($(ARCH), WIN95)
    JDK := $(subst \,/,$(JAVA_HOME))
    SEP=;
  else
    JDK := $(JAVA_HOME)
    SEP=:
  endif
endif

JNDILIB:=$(BASEDIR)/ldapsp/lib/jndi.jar
JAVACLASSPATH:=$(CLASS_DEST)$(SEP)$(BASEDIR)/ldapsp$(SEP)$(JDK)/lib/classes.zip$(SEP)$(JNDILIB)$(SEP)$(CLASSPATH)

SRCDIR=com/netscape/jndi/ldap
DISTDIR=$(MCOM_ROOT)/dist
CLASSDIR=$(MCOM_ROOT)/dist/classes
CLASSPACKAGEDIR=$(DISTDIR)/packages
DOCDIR=$(DISTDIR)/doc/ldapsp
BASEPACKAGENAME=ldapsp.jar
CLASSPACKAGE=$(CLASSPACKAGEDIR)/$(PACKAGENAME)
DOCCLASSES=com.netscape.jndi.ldap.controls

ifdef JAVA_HOME
    JDKBIN=$(JDK)/bin/
endif

ifndef JAVAC
  ifndef DEBUG
     #defualt mode is debug (-g)
    JAVAC=$(JDKBIN)javac -g -classpath "$(JAVACLASSPATH)"
  else
  ifeq ($(DEBUG), 1)
    JAVAC=$(JDKBIN)javac -g -classpath "$(JAVACLASSPATH)"
  else
    JAVAC=$(JDKBIN)javac -O -classpath "$(JAVACLASSPATH)"
  endif
  endif
endif

ifndef JAR
    JAR:=$(JDKBIN)jar
endif

ifndef JAVADOC
  JAVADOC=$(JDKBIN)javadoc -classpath "$(JAVACLASSPATH)"
endif

all: classes 

doc: $(DISTDIR) $(DOCDIR)
	$(JAVADOC) -d $(DOCDIR) $(DOCCLASSES)

basics: $(DISTDIR) $(CLASSDIR)

classes: JNDICLASSES 

package: basepackage

basepackage: $(CLASSPACKAGEDIR)
	cd $(DISTDIR)/classes; rm -f ../packages/$(BASEPACKAGENAME); $(JAR) cvf ../packages/$(BASEPACKAGENAME) com/netscape/jndi/ldap/*.class com/netscape/jndi/ldap/common/*.class com/netscape/jndi/ldap/schema/*.class com/netscape/jndi/ldap/controls/*.class

MAIN: basics
	cd ldapsp/$(SRCDIR); $(JAVAC) -d $(CLASS_DEST) *.java

SCHEMA: basics
	cd ldapsp/$(SRCDIR)/schema; $(JAVAC) -d $(CLASS_DEST) *.java

COMMON: basics
	cd ldapsp/$(SRCDIR)/common; $(JAVAC) -d $(CLASS_DEST) *.java

CONTROLS: basics
	cd ldapsp/$(SRCDIR)/controls; $(JAVAC) -d $(CLASS_DEST) *.java

JNDICLASSES: COMMON CONTROLS SCHEMA MAIN

clean:
	rm -rf $(DISTDIR)/classes/com/netscape/jndi/ldap

$(CLASSPACKAGEDIR):
	mkdir -p $@

$(DOCDIR):
	mkdir -p $@

$(DISTDIR):
	mkdir -p $@

$(CLASSDIR):
	mkdir -p $@
