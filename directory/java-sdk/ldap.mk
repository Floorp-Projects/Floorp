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
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
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
JAVACLASSPATH:=$(BASEDIR)/ldapjdk$(SEP)$(BASEDIR)/ldapbeans$(SEP)$(BASEDIR)/ldapfilter$(SEP)$(CLASSPATH)

SRCDIR=netscape/ldap
BEANDIR=$(BASEDIR)/ldapbeans/netscape/ldap/beans
DISTDIR=$(MCOM_ROOT)/dist
CLASSDIR=$(MCOM_ROOT)/dist/classes
CLASSPACKAGEDIR=$(DISTDIR)/packages
PACKAGENAME=javaldap.zip
ifeq ($(DEBUG), 1)
BASEPACKAGENAME=ldapjdk_debug.jar
else
BASEPACKAGENAME=ldapjdk.jar
endif
CLASSPACKAGE=$(CLASSPACKAGEDIR)/$(PACKAGENAME)
ERRORSDIR=$(CLASSDIR)/netscape/ldap/errors
SASLDIR=com/netscape/sasl

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

all: classes

basics: $(DISTDIR) $(CLASSDIR)
	cp -p manifest.mf $(CLASS_DEST)

classes: LDAPCLASSES BEANS TOOLS

basepackage: $(CLASSPACKAGEDIR)
	cd $(DISTDIR)/classes; rm -f ../packages/$(BASEPACKAGENAME); $(JAR) cvfm ../packages/$(BASEPACKAGENAME) manifest.mf netscape/ldap/*.class netscape/ldap/client/*.class netscape/ldap/client/opers/*.class netscape/ldap/ber/stream/*.class netscape/ldap/controls/*.class netscape/ldap/util/*.class netscape/ldap/errors/*.props com/netscape/sasl/*.class tools/*.class

MAIN: basics
	cd ldapjdk/$(SRCDIR); $(JAVAC) -d "$(CLASS_DEST)" *.java

CLIENT: basics
	cd ldapjdk/$(SRCDIR)/client; $(JAVAC) -d "$(CLASS_DEST)" *.java

OPERS: basics
	cd ldapjdk/$(SRCDIR)/client/opers; $(JAVAC) -d "$(CLASS_DEST)" *.java

BER: basics
	cd ldapjdk/$(SRCDIR)/ber/stream; $(JAVAC) -d "$(CLASS_DEST)" *.java

UTIL: basics
	cd ldapjdk/$(SRCDIR)/util; $(JAVAC) -d "$(CLASS_DEST)" *.java

SASL: basics
	cd ldapjdk/$(SASLDIR); $(JAVAC) -d "$(CLASS_DEST)" *.java

ERRORS: basics $(ERRORSDIR)
	cp -p ldapjdk/$(SRCDIR)/errors/*.props $(ERRORSDIR)

CONTROLS: basics
	cd ldapjdk/$(SRCDIR)/controls; $(JAVAC) -d "$(CLASS_DEST)" *.java

LDAPCLASSES: BER OPERS CLIENT MAIN UTIL CONTROLS ERRORS SASL

BEANS: OTHERBEANS

OTHERBEANS: basics
	cd ldapbeans/$(SRCDIR)/beans; $(JAVAC) -d "$(CLASS_DEST)" *.java

TOOLS: basics
	cd tools; $(JAVAC) -d "$(CLASS_DEST)" *.java

clean:
	rm -rf $(DISTDIR)

$(CLASSPACKAGEDIR):
	mkdir -p $@

$(DOCPACKAGEDIR):
	mkdir -p $@

$(DISTDIR):
	mkdir -p $@

$(CLASSDIR):
	mkdir -p $@

$(ERRORSDIR):
	mkdir -p $@

