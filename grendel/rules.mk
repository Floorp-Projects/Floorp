#!gmake
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License.  You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
# the License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Grendel mail/news client.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation.  Portions created by Netscape are Copyright (C) 1997
# Netscape Communications Corporation.  All Rights Reserved.

# You need these:
#
# http://java.sun.com/products/javamail/
# http://java.sun.com/beans/glasgow/jaf.html
# http://java.sun.com/products/jfc/#download-swing
#
# setenv CLASSPATH ''
# setenv CLASSPATH ${CLASSPATH}:/u/jwz/src/mozilla
# setenv CLASSPATH ${CLASSPATH}:/u/jwz/src/mozilla/grendel
# setenv CLASSPATH ${CLASSPATH}:/u/jwz/tmp/grendel/javamail-1.1/mail.jar
# setenv CLASSPATH ${CLASSPATH}:/u/jwz/tmp/grendel/jaf/activation.jar
# setenv CLASSPATH ${CLASSPATH}:/u/jwz/tmp/grendel/swing-1.1/swingall.jar

# Set these to the correct paths on your system
MOZILLA_BUILD = /disk2/mozilla
GRENDEL_BUILD = /disk2/mozilla/grendel
MOZILLA_HOME = /usr/local/netscape-4.5
JAVAC	= /usr/local/jdk117_v1a/bin/javac
CLASSPATH = /usr/local/jdk117_v1a/lib/classes.zip:/usr/local/swing-1.1/swingall.jar:/usr/local/javamail-1.1/mail.jar:/usr/local/jaf/activation.jar:/usr/local/xml-ea2/xml.jar:$(MOZILLA_HOME)/java/classes/ifc11.jar:$(MOZILLA_HOME)/java/classes/ldap10.jar:$(MOZILLA_BUILD):$(GRENDEL_BUILD)

RM	= rm -f

OBJS	= $(subst .java,.class,$(SRCS))

.SUFFIXES: .java .class

.java.class:
	$(JAVAC) -g -classpath $(CLASSPATH) $*.java

all:: $(OBJS)


#clean::
#	$(RM) $(OBJS)

clean::
	$(RM) *.class

distclean::
	$(RM) *.class *~ core

all clean distclean::
	@sd="$(SUBDIRS)" ;		\
	for dir in $$sd; do		\
	  ( cd $$dir ; $(MAKE) $@ );	\
	done
