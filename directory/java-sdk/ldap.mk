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


##############################################################################
#
# Makefile for the LDAP classes
#
# This makefile requires the following variables to be defined (either as
# environment variables or on the make invocation line):
#  - MOZ_SRC      the root of your CVS tree
#  - JAVA_VERSION the Java version in the format n.n (e.g. 1.4)
#  - JAVA_HOME    the Java root directory
#
# A debug compile (java -g) is done by default. You can specify "DEBUG=0" on
# the make line to compile with '-O' option.
#
# --- LDAPJDK --- 
# To compile and package ldapjdk, use 'classes' and 'package' as make targets.
# You can compile only subsets of the classes by specifying one or more
# of the following: MAIN BER OPERS CLIENT CONTROLS UTIL FACTORY ERRORS SASL
# SASLMECHANISM TOOLS.
#
# --- JAVADOC ---
# To create and package javadoc, use 'doc' and 'docpackage' as targets.
#
# --- LDAPBEANS ---
# To compile and package ldapbeans, use 'beanclasses' and 'beanpackage'
# as make targets.
#
# --- LDAPFILTER ---
# To compile the optional ldapfilter package, you must have OROMatcher(R)
# java regular expressions package com.oroinc.text.regex in your CLASSPATH.
# The package is available at http://www.oroinc.com. Use 'filterclasses'
# and 'filterpackage' as make targets. 
#
##############################################################################

.CHECK_VARS:
ifndef MOZ_SRC
	@echo "MOZ_SRC is undefined"
	@echo "   MOZ_SRC=<root directory of your CVS tree>"
	@echo "   Usage example : gmake -f ldap.mk MOZ_SRC=c:\mozilla
	exit 1
else
	@echo "MOZ_SRC is $(MOZ_SRC)"

endif
ifndef JAVA_VERSION
	@echo "JAVA_VERSION is undefined"
	@echo "   JAVA_VERSION=n.n where n.n is 1.1, 1.2, 1.3, 1.4, etc."
	@echo "   Usage example : gmake -f ldap.mk JAVA_VERSION=1.4"
	exit 1
else
	@echo "JAVA_VERSION is $(JAVA_VERSION)"
endif
ifndef JAVA_HOME
ifeq ($(JAVA_VERSION), 1.1)
	@echo "JAVA_HOME is undefined"
	@echo "   JAVA_HOME=<directory where java is installed>"
	@echo "   Usage example : gmake -f ldap.mk JAVA_HOME=c:\jdk1.1.8 JAVA_VERSION=1.1"
	exit 1
else
	@echo "WARNING: JAVA_HOME is undefined; Using java from the PATH, expected version is" $(JAVA_VERSION)
	@java -version
endif
else
	@echo "JAVA_HOME is $(JAVA_HOME)"
endif

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

JSS_LIB=$(BASEDIR)/ldapjdk/lib/jss32_stub.jar
JAASLIB=$(BASEDIR)/ldapjdk/lib/jaas.jar
JSSELIB=$(BASEDIR)/ldapjdk/lib/jnet.jar$(SEP)$(BASEDIR)/ldapjdk/lib/jsse.jar

# Set up the JAVACLASSPATH 
JAVACLASSPATH:=$(CLASSPATH)$(SEP)$(BASEDIR)/ldapjdk
ifeq ($(JAVA_VERSION), 1.1)
    JAVACLASSPATH:=$(JDK)/lib/classes.zip$(SEP)$(JAVACLASSPATH)$(SEP)$(JAASLIB)
else
ifeq ($(JAVA_VERSION), 1.2)
    JAVACLASSPATH:=$(JAVACLASSPATH)$(SEP)$(JAASLIB)$(SEP)$(JSSELIB)$(SEP)$(JSS_LIB)
else
ifeq ($(JAVA_VERSION), 1.3)
    JAVACLASSPATH:=$(JAVACLASSPATH)$(SEP)$(JAASLIB)$(SEP)$(JSSELIB)$(SEP)$(JSS_LIB)
else
    # JDK 1.4 and higher
    JAVACLASSPATH:=$(JAVACLASSPATH)$(SEP)$(JSS_LIB)
endif
endif
endif

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
  JAVADOC=$(JDKBIN)javadoc -classpath "$(JAVACLASSPATH)$(SEP)$(BASEDIR)/ldapbeans"
endif

# Destination for class files and packages
CLASS_DEST=$(BASEDIR)/dist/classes

SRCDIR=netscape/ldap
DISTDIR=$(MCOM_ROOT)/dist
CLASSDIR=$(MCOM_ROOT)/dist/classes
CLASSPACKAGEDIR=$(DISTDIR)/packages
BASEPACKAGENAME=ldapjdk.jar

TOOLSTARGETDIR=$(DISTDIR)/tools
TOOLSDIR=$(BASEDIR)/tools

ERRORSDIR=$(CLASSDIR)/netscape/ldap/errors
SASLDIR=com/netscape/sasl
SASLMECHANISMDIR=com/netscape/sasl/mechanisms

all: classes

basics: .CHECK_VARS $(DISTDIR) $(CLASSDIR)

classes: LDAPCLASSES

package: basepackage

basepackage: $(CLASSPACKAGEDIR)
	cd $(DISTDIR)/classes; rm -f ../packages/$(BASEPACKAGENAME); $(JAR) cvf ../packages/$(BASEPACKAGENAME) netscape/ldap/*.class netscape/ldap/client/*.class netscape/ldap/client/opers/*.class netscape/ldap/ber/stream/*.class netscape/ldap/controls/*.class netscape/ldap/factory/*.class netscape/ldap/util/*.class netscape/ldap/errors/*.props com/netscape/sasl/*.class com/netscape/sasl/mechanisms/*.class *.class

MAIN: basics
	cd ldapjdk/$(SRCDIR); $(JAVAC) -d "$(CLASS_DEST)" *.java

FACTORY: basics
ifneq ($(JAVA_VERSION), 1.1)
	cd ldapjdk/$(SRCDIR)/factory; $(JAVAC) -d "$(CLASS_DEST)" *.java
endif

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

TOOLS: basics
	cd tools; $(JAVAC) -d "$(CLASS_DEST)" *.java

LDAPCLASSES: BER OPERS CLIENT MAIN FACTORY UTIL CONTROLS ERRORS SASL SASLMECHANISM TOOLS

##########################################################################
# JAVADOC
##########################################################################
DOCDIR=$(DISTDIR)/doc
DOCNAME=ldapdoc.zip
DOCPACKAGE=$(CLASSPACKAGEDIR)/$(DOCNAME)

BERDOCCLASSES=netscape.ldap.ber.stream
SASLDOCCLASSES=com.netscape.sasl com.netscape.sasl.mechanisms
ifneq ($(JAVA_VERSION), 1.1)
FACTORYDOCCLASSES := netscape.ldap.factory
endif

DOCCLASSES=netscape.ldap netscape.ldap.beans netscape.ldap.controls \
	netscape.ldap.util $(FACTORYDOCCLASSES) \
	$(SASLDOCCLASSES) $(TOOLSDIR)/*.java $(BERDOCCLASSES)

doc: $(DISTDIR) $(DOCDIR)
	$(JAVADOC) -d $(DOCDIR) $(DOCCLASSES)

docpackage: $(DOCDIR) $(CLASSPACKAGEDIR)
	cd $(DOCDIR); rm -f ../packages/$(DOCNAME); $(JAR) cvf ../packages/$(DOCNAME) *.html *.css netscape/ldap/*.html netscape/ldap/beans/*.html netscape/ldap/controls/*.html netscape/ldap/util/*.html netscape/ldap/ber/stream/*.html 


##########################################################################
# LDAPBEANS
##########################################################################
BEANDIR=$(BASEDIR)/ldapbeans/netscape/ldap/beans
BEANPACKAGENAME=ldapbeans.jar

beanclasses: basics
	cd ldapbeans/$(SRCDIR)/beans; $(JAVAC) -d "$(CLASS_DEST)" *.java

beanpackage: $(CLASSPACKAGEDIR)
	cd $(DISTDIR)/classes; rm -f ../packages/$(BEANPACKAGENAME); $(JAR) cvf ../packages/$(BEANPACKAGENAME) netscape/ldap/beans

##########################################################################
# Filter package
##########################################################################
FILTERCLASSDIR=$(MCOM_ROOT)/dist/ldapfilt
FILTER_CLASS_DEST=$(BASEDIR)/dist/ldapfilt
FILTERJAR=ldapfilt.jar

filterclasses: $(FILTERCLASSDIR)
	cd ldapfilter/netscape/ldap/util; $(JAVAC) -d "$(FILTER_CLASS_DEST)" *.java

filterpackage: $(CLASSPACKAGEDIR)
	cd "$(FILTER_CLASS_DEST)"; rm -f ../packages/$(FILTERJAR); $(JAR) cvf ../packages/$(FILTERJAR) netscape/ldap/util/*.class


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
