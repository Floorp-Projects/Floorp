#!gmake
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



DEPTH		= ../../..

MODULE		= java

JJSD		= netscape/jsdebug

#
# JDIRS is dependant on JAVA_DESTPATH in config/rules.m[a]k. 
# Be sure to touch that directory if you add a new directory to
# JDIRS, or else it will not build. XXX
#
JDIRS		= $(JJSD)

JAR_JSD		= jsd10.jar
JAR_JSD_CLASSES	= $(JJSD)

#
# jars to build at install time
#
JARS		= $(JAR_JSD)

include $(DEPTH)/config/rules.mk

JAVA_SOURCEPATH = $(DEPTH)/js/jsd/classes

doc::
	$(JAVADOC) -d $(DIST)/doc netscape.jsdebug

natives_list:: FORCE
	rm -rf $@
	find . -name "*.class" -print | sed 's@\./\(.*\)\.class$$@\1@' | \
		sed 's@/@.@g' | xargs $(JVH) -natives | sort > $@

check_natives:: natives_list
	rm -f found_natives
	nm -B	../$(OBJDIR)/*.o \
	    | egrep "Java.*_stub" | awk '{ print $$3; }' | sort > found_natives
	diff found_natives natives_list

FORCE:
