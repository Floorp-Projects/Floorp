# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# Copyright (C) 1999 Netscape Communications Corporation.  All Rights
# Reserved.
#
# Makefile for the LDAP classes
#
# An optimized compile is done by default. You can specify "DEBUG=1" on
# the make line to generate debug symbols in the bytecode.
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

# Set up the CLASSPATH automatically,
ifeq ($(ARCH), WINNT)
  JDK := $(subst \,/,$(JAVA_HOME))
  JAR:=$(JDK)/bin/jar
  SEP=;
else
  ifeq ($(ARCH), WIN95)
    JDK := $(subst \,/,$(JAVA_HOME))
    JAR:=$(JDK)/bin/jar
    SEP=;
  else
    JDK := $(JAVA_HOME)
    JAR:=$(JAVA_HOME)/bin/jar
    SEP=:
  endif
endif
JNDILIB:=$(BASEDIR)/ldapsp/lib/jndi.jar
JAVACLASSPATH:=$(CLASS_DEST)$(SEP)$(BASEDIR)/ldapsp$(SEP)$(JDKLIB)$(SEP)$(JNDILIB)$(SEP)$(CLASSPATH)

SRCDIR=com/netscape/jndi/ldap
DISTDIR=$(MCOM_ROOT)/dist
CLASSDIR=$(MCOM_ROOT)/dist/classes
CLASSPACKAGEDIR=$(DISTDIR)/packages
DOCDIR=$(DISTDIR)/doc/ldapsp
ifeq ($(DEBUG), full)
BASEPACKAGENAME=ldapsp_debug.jar
else
BASEPACKAGENAME=ldapsp.jar
endif
CLASSPACKAGE=$(CLASSPACKAGEDIR)/$(PACKAGENAME)

ifndef JAVADOC
  JAVADOC=javadoc -classpath "$(JAVACLASSPATH)"
endif
ifndef JAVAC
  ifdef JAVA_HOME
    JDKBIN=$(JDK)/bin/
  endif
  ifeq ($(DEBUG), 1)
    JAVAC=$(JDKBIN)javac -g -classpath "$(JAVACLASSPATH)"
  else
    JAVAC=$(JDKBIN)javac -O -classpath "$(JAVACLASSPATH)"
  endif
endif

DOCCLASSES=com.netscape.jndi.ldap.controls

all: classes 

doc: $(DISTDIR) $(DOCDIR) DOCS

basics: $(DISTDIR) $(CLASSDIR)

classes: JNDICLASSES 

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

DOCS:
	$(JAVADOC) -d $(DOCDIR) $(DOCCLASSES)

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
