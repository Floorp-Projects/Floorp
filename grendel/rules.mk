#!gmake
#
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
# The Original Code is the Grendel mail/news client.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation.  Portions created by Netscape are
# Copyright (C) 1997 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

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
MOZILLA_BUILD = /export/home/grail/codemonkey/mozilla/mozilla
GRENDEL_BUILD = /export/home/grail/codemonkey/mozilla/grendel
MOZILLA_HOME = /usr/local/netscape-4.5
JAVAC	= javac
LIBS	= $(TOPDIR)/extlib/mail.jar:$(TOPDIR)/extlib/activation.jar:$(TOPDIR)/extlib/jaxp.jar:$(TOPDIR)/extlib/parser.jar:$(TOPDIR)/extlib/jhall.jar:$(TOPDIR)/extlib/ldapjdk.jar:$(TOPDIR)/extlib/OROMatcher.jar
RM	= rm -f
DISTDIR = $(TOPDIR)/dist/classes

OBJS	= $(subst .java,.class,$(SRCS))

.SUFFIXES: .java .class

.java.class:
	$(JAVAC) -J-mx64m -classpath $(LIBS) -sourcepath $(TOPDIR)/sources -d $(DISTDIR) -g $*.java

all:: $(OBJS)

distclean::
	$(RM) *.class *~ core

all clean distclean resources::
	@sd="$(SUBDIRS)" ;		\
	for dir in $$sd; do		\
	  ( cd $$dir ; echo "Making $@ in $$dir" ; $(MAKE) $@ );	\
	done

run::
	java -classpath $(DISTDIR):extlib/OROMatcher.jar:extlib/activation.jar:extlib/jaxp.jar:extlib/jhall.jar:extlib/ldapjdk.jar:extlib/mail.jar:extlib/parser.jar grendel.Main

