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



DEPTH		= ../../../..

MODULE		= java

#
# the idea is that the install phase for those modules will
# make their own signed jar files
# there are some exceptions here that we deal with
# stuff in modules and in ns/js/jsj
#
JMODS		= netscape/javascript netscape/javascript/adapters

#
# JDIRS is dependant on JAVA_DESTPATH in config/rules.m[a]k. 
# Be sure to touch that directory if you add a new directory to
# JDIRS, or else it will not build. FIXME
#
JDIRS		= $(JMODS)

JAR_JSJ		= jsj10.jar
JAR_JSJ_CLASSES	= $(JMODS)  

#
# jars to build at install time
#
JARS		= $(JAR_JSJ)

include $(DEPTH)/config/rules.mk

JAVA_SOURCEPATH	= $(DEPTH)/js/src/liveconnect/classes

doc::
	$(JAVADOC) -d $(DIST)/doc netscape.javascript

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
