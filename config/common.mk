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
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#

INCLUDED_COMMON_MK = 1

######################################################################
# Cross-platform defines used on all platforms (in theory)
######################################################################

#
# The VERSION_NUMBER is suffixed onto the end of the DLLs we ship.
# Since the longest of these is 5 characters without the suffix,
# be sure to not set VERSION_NUMBER to anything longer than 3 
# characters for Win16's sake.
#
# Also... If you change this value, there are several other places
# you'll need to change (because they're not reached by this 
# variable): 
#	sun-java/nsjava/nsjava32.def
#	sun-java/nsjava/nsjava16.def
#	sun-java/classsrc/sun/audio/AudioDevice.java
#	sun-java/classsrc/sun/awt/windows/WToolkit.java
#
VERSION_NUMBER		= 50

# jar for core java classes:
JAR_NAME		= java$(VERSION_NUMBER).jar

# jar for navigator-specific java code:
NAV_JAR_NAME		= nav$(VERSION_NUMBER).jar

######################################################################
# Cross-Platform Java Stuff
######################################################################
# java interpreter

# get class files from the directory they are compiled to
JAVA_CLASSPATH		= $(JAVAC_ZIP)$(PATH_SEPARATOR)$(JAVA_DESTPATH)

JAVA_FLAGS		= -classpath $(JAVA_CLASSPATH) -ms8m
JAVA			= $(JAVA_PROG) $(JAVA_FLAGS) 

#
# NOTE: If a new DLL is being added to this define you will have to update
#       ns/sun-java/include/javadefs.h in order not to break win16.
#
JAVA_DEFINES		= -DJAR_NAME=\"$(JAR_NAME)\" -DJRTDLL=\"$(JRTDLL)\" -DMMDLL=\"$(MMDLL)\" \
			  -DAWTDLL=\"$(AWTDLL)\" -DJITDLL=\"$(JITDLL)\" -DJPWDLL=\"$(JPWDLL)\"

######################################################################
# javac

#
# java wants '-ms8m' and kaffe wants '-ms 8m', so this needs to be
# overridable.
#
JINT_FLAGS		= -ms8m

# to run the compiler in the interpreter
JAVAC_PROG		= $(JINT_FLAGS) $(PDJAVA_FLAGS) -classpath $(JAVAC_ZIP) sun.tools.javac.Main
JAVAC			= $(JAVA_PROG) $(JAVAC_PROG) $(JAVAC_FLAGS)

# std set of options passed to the compiler
JAVAC_FLAGS		= -classpath $(JAVAC_CLASSPATH) $(JAVAC_OPTIMIZER) -d $(JAVA_DESTPATH)

#
# The canonical Java classpath is:
# JAVA_DESTPATH, JAVA_SOURCEPATH, JAVA_LIBS
# 
# appropriately delimited, in that order
#
JAVAC_CLASSPATH		= $(JAVAC_ZIP)$(PATH_SEPARATOR)$(JAVA_DESTPATH)$(PATH_SEPARATOR)$(JAVA_SOURCEPATH)

######################################################################
# javadoc

# Rules to build java .html files from java source files

JAVADOC_PROG		= $(JAVA) sun.tools.javadoc.Main
JAVADOC_FLAGS		= -classpath $(JAVAC_CLASSPATH)
JAVADOC			= $(JAVADOC_PROG) $(JAVADOC_FLAGS)

######################################################################
# javah

JAVAH_FLAGS		= -classpath $(JAVAC_ZIP)$(PATH_SEPARATOR)$(JAVA_DESTPATH)
JAVAH			= $(JAVAH_PROG) $(JAVAH_FLAGS)

######################################################################
# jmc

JMCSRCDIR		= $(DIST)/_jmc
JMC_PROG		= $(JAVA) netscape.tools.jmc.Main
JMC_CLASSPATH		= $(JMCSRCDIR)$(PATH_SEPARATOR)$(JAVAC_CLASSPATH)
JMC_FLAGS		= -classpath $(JMC_CLASSPATH) -verbose
JMC			= $(JMC_PROG) $(JMC_FLAGS)

######################################################################
# zip

ZIP			= $(ZIP_PROG) $(ZIP_FLAGS)

######################################################################
# idl2java

ORBTOOLS		= $(DEPTH)/modules/iiop/tools/orbtools.zip
ORB_CLASSPATH		= $(ORBTOOLS)$(PATH_SEPARATOR)$(JAVA_CLASSPATH)

IDL2JAVA_PROG		= $(JAVA_PROG)
IDL2JAVA_FLAGS		= -classpath $(ORB_CLASSPATH) pomoco.tools.idl2java
IDL2JAVA		= $(IDL2JAVA_PROG) $(IDL2JAVA_FLAGS)

######################################################################
# lex and yacc

JAVALEX_PROG		= $(JAVA_PROG) -classpath $(ORB_CLASSPATH) sbktech.tools.jax.driver
JAVALEX_FLAGS		=
JAVALEX			= $(JAVALEX_PROG) $(JAVALEX_FLAGS)

JAVACUP_PROG		= $(JAVA_PROG) -classpath $(ORB_CLASSPATH) java_cup.Main
JAVACUP_FLAGS		=
JAVACUP			= $(JAVACUP_PROG) $(JAVACUP_FLAGS)

