# 
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and limitations
# under the License.
# 
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
# Inc. All Rights Reserved. 

DEPTH		= /opt/mozilla
topsrcdir	= $(DEPTH)
srcdir		= .
VPATH		= .

include $(DEPTH)/config/autoconf.mk

MODULE		= javadom
LIBRARY_NAME	= javadom
IS_COMPONENT	= 1

CPPSRCS		= \
	nsJavaDOMImpl.cpp \
	nsJavaDOMFactory.cpp 

include $(topsrcdir)/config/config.mk

JAVA_HOME	= /opt/jdk
INCLUDES	+= -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/solaris
DSO_LDOPTS	+= \
	-L$(JAVA_HOME)/jre/lib/sparc \
	-L$(JAVA_HOME)/jre/lib/sparc/classic \
	-L$(JAVA_HOME)/jre/lib/sparc/native_threads \
	-R$(JAVA_HOME)/jre/lib/sparc \
	-R$(JAVA_HOME)/jre/lib/sparc/classic \
	-R$(JAVA_HOME)/jre/lib/sparc/native_threads \
	-ljvm -lhpi -lsunwjit

EXPORTS 	= \
	nsJavaDOMCID.h \
	nsIJavaDOM.h 

EXPORTS		:= $(addprefix $(srcdir)/, $(EXPORTS))

include $(topsrcdir)/config/rules.mk
