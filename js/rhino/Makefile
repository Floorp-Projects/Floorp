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
# Makefile for javascript in java.
#

# This makefile is intended for packaging releases, and probably isn't
# suitable for production use - it doesn't attempt to do understand
# java dependencies beyond the package level.
#
# The makefiles for the subdirectories included in this package are
# intended to be called by this makefile with the proper CLASSDIR,
# PATH_PREFIX etc. variables.  Makefiles in subdirectories are
# actually executed in the toplevel directory, with the PATH_PREFIX
# variable set to the subdirectory where the makefile is located.
#
# Initial version courtesy Mike Ang.
# Next version by Mike McCabe

# Don't include SHELL define (per GNU manual recommendation) because it
# breaks WinNT (with GNU make) builds.
# SHELL = /bin/sh

# Some things we might want to tweek.

CLASSDIR = classes

PACKAGE_NAME = org.mozilla.javascript
PACKAGE_PATH = org/mozilla/javascript

# jar filenames and the directories that build them.
JS_JAR = js.jar
JS_DIR = $(PACKAGE_PATH)
JSTOOLS_JAR = jstools.jar
JSTOOLS_DIR = $(PACKAGE_PATH)/tools

JARS = $(JS_JAR) $(JSTOOLS_JAR)

# It's not polite to store toplevel files in a tarball or zip files.
# What is the name of the toplevel directory to store files in?
# XXX we should probably add versioning to this.
DIST_DIR = jsjava

# XXX test this with sj
JAVAC = javac

# We don't define JFLAGS but we do export it to child
# builds in case it's defined by the environment.
# To build optimized (with javac) say 'make JFLAGS=-O'

GZIP = gzip
ZIP = zip
UNZIP = unzip

# Shouldn't need to change anything below here.

# For Windows NT builds (under GNU make).
ifeq ($(OS_TARGET), WINNT)
CLASSPATHSEP = '\\;'
else
CLASSPATHSEP = :
endif

# Make compatibility - use these instead of gmake 'export VARIABLE'
EXPORTS = CLASSDIR=$(CLASSDIR) JAVAC=$(JAVAC) JFLAGS=$(JFLAGS) SHELL=$(SHELL) \
	PACKAGE_PATH=$(PACKAGE_PATH) PACKAGE_NAME=$(PACKAGE_NAME)

helpmessage : FORCE
	@echo 'Targets include:'
	@echo '\tall - make jars, examples'
	@echo '\tjars - make js.jar, jstools.jar'
	@echo '\tfast - quick-and-dirty "make jars", for development'
	@echo '\texamples - build the .class files in the examples directory'
	@echo '\tcheck - perform checks on the source.'
	@echo '\tclean - remove intermediate files'
	@echo '\tclobber - make clean, and remove .jar files'
	@echo '\tzip - make a distribution .zip file'
	@echo '\tzip-source - make a distribution .zip file, with source'
	@echo '\ttar - make a distribution .tar.gz file'
	@echo '\ttar-source - make a distribution .tar.gz, with source'
	@echo
	@echo 'Define OS_TARGET to "WINNT" to build on Windows NT with GNU make.'
	@echo

all : jars examples

jars : $(JARS)

fast : fast_$(JS_JAR) $(JSTOOLS_JAR)

# Always call the sub-Makefile - which may decide that the jar is up to date.
$(JS_JAR) : FORCE
	$(MAKE) -f $(JS_DIR)/Makefile JAR=$(@) $(EXPORTS) \
		PATH_PREFIX=$(JS_DIR) \
		CLASSPATH=.

fast_$(JS_JAR) :
	$(MAKE) -f $(JS_DIR)/Makefile JAR=$(JS_JAR) $(EXPORTS) \
		PATH_PREFIX=$(JS_DIR) \
		CLASSPATH=. \
		fast

$(JSTOOLS_JAR) : $(JS_JAR) FORCE
	$(MAKE) -f $(JSTOOLS_DIR)/Makefile JAR=$(@) $(EXPORTS) \
		PATH_PREFIX=$(JSTOOLS_DIR) \
		CLASSPATH=./$(JS_JAR)$(CLASSPATHSEP).

examples : $(JS_JAR) FORCE
	$(MAKE) -f examples/Makefile $(EXPORTS) \
		PATH_PREFIX=examples \
		CLASSPATH=./$(JS_JAR)

# We ask the subdirs to update their MANIFESTs
MANIFEST : FORCE
	$(MAKE) -f $(JS_DIR)/Makefile JAR=$(JS_JAR) $(EXPORTS) \
		PATH_PREFIX=$(JS_DIR) $(JS_DIR)/MANIFEST
	$(MAKE) -f $(JSTOOLS_DIR)/Makefile JAR=$(JSTOOLS_JAR) $(EXPORTS) \
		PATH_PREFIX=$(JSTOOLS_DIR) $(JSTOOLS_DIR)/MANIFEST
	$(MAKE) -f examples/Makefile $(EXPORTS) \
		PATH_PREFIX=examples examples/MANIFEST
# so ls below always has something to work on
	touch MANIFEST
# examples/Makefile doesn't get included in the
# MANIFEST file, (which is used to create the non-source distribution) so
# we include it here.
	cat examples/MANIFEST $(JS_DIR)/MANIFEST \
		$(JSTOOLS_DIR)/MANIFEST \
		| xargs ls MANIFEST README.html \
		  $(JARS) \
		  Makefile examples/Makefile \
		    > $(@)

# Make a MANIFEST file containing only the binaries and documentation.
# This could be abstracted further...
MANIFEST_binonly : MANIFEST
	cat examples/MANIFEST \
	| xargs ls $(JARS) README.html MANIFEST > MANIFEST

# A subroutine - not intended to be called from outside the makefile.
do_zip : 
# Make sure we get a fresh one
	- rm -r $(DIST_DIR)
	- mkdir $(DIST_DIR)
	- rm    $(DIST_DIR).zip
	cat MANIFEST | xargs $(ZIP) -0 -q $(DIST_DIR).zip
	mv $(DIST_DIR).zip $(DIST_DIR)
	cd $(DIST_DIR) ; \
		$(UNZIP) -q $(DIST_DIR).zip ; \
		rm $(DIST_DIR).zip
	$(ZIP) -r -9 -q $(DIST_DIR).zip $(DIST_DIR)
	- rm -r $(DIST_DIR)

zip : check jars examples MANIFEST_binonly do_zip

zip-source : check jars examples MANIFEST do_zip

# A subroutine - not intended to be called from outside the makefile.
do_tar :
	- rm -r $(DIST_DIR)
	- mkdir $(DIST_DIR)
	- rm $(DIST_DIR).tar $(DIST_DIR).tar.gz
	cat MANIFEST | xargs tar cf $(DIST_DIR).tar
	mv $(DIST_DIR).tar $(DIST_DIR)
	cd $(DIST_DIR) ; \
		tar xf $(DIST_DIR).tar ; \
		rm $(DIST_DIR).tar
	tar cf $(DIST_DIR).tar $(DIST_DIR)
	- rm -r $(DIST_DIR)
	$(GZIP) -9 $(DIST_DIR).tar

tar: check jars examples MANIFEST_binonly do_tar

tar-source : check jars examples MANIFEST do_tar

# These commands just get passed to the respective sub-Makefiles.
clean clobber check:
	$(MAKE) -f $(JS_DIR)/Makefile $(EXPORTS) JAR=$(JS_JAR) \
		PATH_PREFIX=$(JS_DIR) $(@)
	$(MAKE) -f $(JSTOOLS_DIR)/Makefile $(EXPORTS) JAR=$(JSTOOLS_JAR) \
		PATH_PREFIX=$(JSTOOLS_DIR) $(@)
	$(MAKE) -f examples/Makefile $(EXPORTS) PATH_PREFIX=examples $(@)

#emulate .PHONY
FORCE :
