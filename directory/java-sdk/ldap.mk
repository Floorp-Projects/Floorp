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
# The package includes com.netscape.sasl (until there is another
# home for it)
#
# You can compile only subsets of the classes by specifying one of the
# following:
#    doc
#    classes
#      LDAPCLASSES
#        MAIN
#        CLIENT
#        OPERS
#        UTIL
#        BER
#      BEANS
#    SASL
#    TOOLS
#    FILTER
#
# Create the JAR files with the following targets:
#    package
#      basepackage
#      filterpackage
#      beanpackage
#      docpackage
#
# The usual mozilla environment variable must be defined:
#  MOZ_SRC  (the root of your CVS tree)
#
# And the Java root directory
#  JAVA_HOME
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
FILTER_CLASS_DEST=$(BASEDIR)/dist/ldapfilt

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
JAASLIB=$(BASEDIR)/ldapjdk/lib/jaas.jar
JAVACLASSPATH:=$(BASEDIR)/ldapjdk$(SEP)$(JAASLIB)$(SEP)$(BASEDIR)/ldapbeans$(SEP)$(JDK)/lib/classes.zip$(SEP)$(CLASSPATH)

SRCDIR=netscape/ldap
BEANDIR=$(BASEDIR)/ldapbeans/netscape/ldap/beans
DISTDIR=$(MCOM_ROOT)/dist
CLASSDIR=$(MCOM_ROOT)/dist/classes
FILTERCLASSDIR=$(MCOM_ROOT)/dist/ldapfilt
CLASSPACKAGEDIR=$(DISTDIR)/packages
PACKAGENAME=javaldap.zip
ifeq ($(DEBUG), 1)
BASEPACKAGENAME=ldapjdk_debug.jar
else
BASEPACKAGENAME=ldapjdk.jar
endif
FILTERJAR=ldapfilt.jar
CLASSPACKAGE=$(CLASSPACKAGEDIR)/$(PACKAGENAME)
BEANPACKAGENAME=ldapbeans.jar
TOOLSTARGETDIR=$(DISTDIR)/tools
TOOLSDIR=$(BASEDIR)/tools
DOCDIR=$(DISTDIR)/doc
BERDOCPACKAGEDIR=$(DISTDIR)/doc/ber
DOCNAME=ldapdoc.zip
DOCPACKAGE=$(CLASSPACKAGEDIR)/$(DOCNAME)
EXAMPLEDIR=$(DISTDIR)/examples
#TESTSRCDIR=$(BASEDIR)/netsite/ldap/java/netscape/ldap/tests
ERRORSDIR=$(CLASSDIR)/netscape/ldap/errors
SASLDIR=com/netscape/sasl
SASLMECHANISMDIR=com/netscape/sasl/mechanisms

ifndef JAVADOC
  JAVADOC=$(JDKBIN)javadoc -classpath "$(JAVACLASSPATH)"
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

BERDOCCLASSES=netscape.ldap.ber.stream

DOCCLASSES=netscape.ldap netscape.ldap.beans netscape.ldap.controls \
	netscape.ldap.util $(TOOLSDIR)/*.java $(BERDOCCLASSES)

all: classes

basics: $(DISTDIR) $(CLASSDIR)
	cp -p manifest.mf $(CLASS_DEST)

classes: LDAPCLASSES BEANS TOOLS

doc: $(DISTDIR) $(DOCDIR) DOCS

berdoc: $(DISTDIR) $(BERDOCPACKAGEDIR) BERDOCS

examples: $(DISTDIR) $(EXAMPLEDIR)/java $(EXAMPLEDIR)/js $(EXAMPLEDIR)/java/beans EXAMPLES

tests: $(CLASSDIR)
	cd $(TESTSRCDIR); $(JAVAC) -d $(CLASS_DEST) *.java

package: basepackage filterpackage beanpackage docpackage

basepackage: $(CLASSPACKAGEDIR)
	cd $(DISTDIR)/classes; rm -f ../packages/$(BASEPACKAGENAME); $(JAR) cvfm ../packages/$(BASEPACKAGENAME) manifest.mf netscape/ldap/*.class netscape/ldap/client/*.class netscape/ldap/client/opers/*.class netscape/ldap/ber/stream/*.class netscape/ldap/controls/*.class netscape/ldap/util/*.class netscape/ldap/errors/*.props com/netscape/sasl/*.class com/netscape/sasl/mechanisms/*.class tools/*.class

beanpackage: $(CLASSPACKAGEDIR)
	cd $(DISTDIR)/classes; rm -f ../packages/$(BEANPACKAGENAME); $(JAR) cvf ../packages/$(BEANPACKAGENAME) netscape/ldap/beans

docpackage: $(DOCDIR) $(CLASSPACKAGEDIR)
	cd $(DOCDIR); rm -f ../packages/$(DOCNAME); $(JAR) cvf ../packages/$(DOCNAME) *.html *.css netscape/ldap/*.html netscape/ldap/beans/*.html netscape/ldap/controls/*.html netscape/ldap/util/*.html netscape/ldap/ber/stream/*.html 

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

SASLMECHANISM: basics
	cd ldapjdk/$(SASLMECHANISMDIR); $(JAVAC) -d "$(CLASS_DEST)" *.java

SASL: basics
	cd ldapjdk/$(SASLDIR); $(JAVAC) -d "$(CLASS_DEST)" *.java

ERRORS: basics $(ERRORSDIR)
	cp -p ldapjdk/$(SRCDIR)/errors/*.props $(ERRORSDIR)

CONTROLS: basics
	cd ldapjdk/$(SRCDIR)/controls; $(JAVAC) -d "$(CLASS_DEST)" *.java

LDAPCLASSES: BER OPERS CLIENT MAIN UTIL CONTROLS ERRORS SASL SASLMECHANISM

BEANS: OTHERBEANS

OTHERBEANS: basics
	cd ldapbeans/$(SRCDIR)/beans; $(JAVAC) -d "$(CLASS_DEST)" *.java

TOOLS: basics
	cd tools; $(JAVAC) -d "$(CLASS_DEST)" *.java

FILTER: $(FILTERCLASSDIR)
	cd ldapfilter/netscape/ldap/util; $(JAVAC) -d "$(FILTER_CLASS_DEST)" *.java

filterpackage: $(CLASSPACKAGEDIR)
	cd "$(FILTER_CLASS_DEST)"; rm -f ../packages/$(FILTERJAR); $(JAR) cvf ../packages/$(FILTERJAR) netscape/ldap/util/*.class

DOCS:
	$(JAVADOC) -d $(DOCDIR) $(DOCCLASSES)

BERDOCS:
	$(JAVADOC) -d $(BERDOCPACKAGEDIR) $(BERDOCCLASSES)

EXAMPLES:
	-cp -p $(EXAMPLESRCDIR)/java/* $(EXAMPLEDIR)/java
	-cp -p $(EXAMPLESRCDIR)/java/beans/* $(EXAMPLEDIR)/java/beans
	-cp -p $(EXAMPLESRCDIR)/java/beans/makejars.* $(CLASSDIR)
	-cp -p $(EXAMPLESRCDIR)/js/* $(EXAMPLEDIR)/js

clean:
	rm -rf $(DISTDIR)

$(CLASSPACKAGEDIR):
	mkdir -p $@

$(DOCDIR):
	mkdir -p $@

$(DISTDIR):
	mkdir -p $@

$(CLASSDIR):
	mkdir -p $@

$(ERRORSDIR):
	mkdir -p $@

$(FILTERCLASSDIR):
	mkdir -p $@
