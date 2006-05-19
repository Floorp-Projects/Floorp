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
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

#######################################################################
# [1] Define preliminary JDK "Core Components" toolset options        #
#######################################################################

# set default JDK java threading model
ifeq ($(JDK_THREADING_MODEL),)
	JDK_THREADING_MODEL     = native_threads
	JDK_THREADING_MODEL_OPT = -native
endif

#######################################################################
# [2] Define platform-independent JDK "Core Components" options       #
#######################################################################

# set default location of the java classes repository
ifeq ($(JAVA_DESTPATH),)
	JAVA_DESTPATH  = $(SOURCE_CLASSES_DIR)
endif

# set default location of the package under the java classes repository
# note that this overrides the default package value in ruleset.mk
ifeq ($(PACKAGE),)
	PACKAGE = .
endif

# set default location of the java source code repository
ifeq ($(JAVA_SOURCEPATH),)
	JAVA_SOURCEPATH	= .
endif

# add JNI directory to default include search path
ifneq ($(JNI_GEN),)
	ifdef NSBUILDROOT
		INCLUDES += -I$(JNI_GEN_DIR) -I$(SOURCE_XP_DIR)
	else
		INCLUDES += -I$(JNI_GEN_DIR)
	endif
endif

#######################################################################
# [3] Define platform-dependent JDK "Core Components" options         #
#######################################################################

# set [Microsoft Windows] platforms (or "vanilla" UNIX platforms)
ifeq ($(OS_ARCH), WINNT)
	ifeq ($(JAVA_HOME),)
		JAVA_HOME = c:\usr\java
	endif

	PATH_SEPARATOR = ;

	JAVA_ARCH = win32

	JAVA_CLASSES = $(JAVA_HOME)\lib\classes.zip

	ifeq ($(JRE_HOME),)
		JRE_HOME = $(JAVA_HOME)
		JRE_CLASSES = $(JAVA_CLASSES)
	else
		ifeq ($(JRE_CLASSES),)
			JRE_CLASSES = $(JRE_HOME)\lib\classes.zip
		endif
	endif

	INCLUDES += -I$(JAVA_HOME)\include
	INCLUDES += -I$(JAVA_HOME)\include\$(JDK_THREADING_MODEL)
	INCLUDES += -I$(JAVA_HOME)\include\$(JAVA_ARCH)

ifeq ($(JDK_DEBUG),1)
	JAVA_LIBS = -LIBPATH:$(JAVA_HOME)\lib javai_g.lib 
else
	JAVA_LIBS = -LIBPATH:$(JAVA_HOME)\lib javai.lib 
endif

	# currently, disable JIT option on this platform
	JDK_JIT_OPT = -nojit
else
	# "vanilla" UNIX platforms
	ifeq ($(JAVA_HOME),)
		JAVA_HOME = /usr/java
	endif

	INCLUDES += -I$(JAVA_HOME)/include

	PATH_SEPARATOR = :
endif

# set [Sun Solaris] platforms
ifeq ($(OS_ARCH), SunOS)
	JAVA_ARCH = solaris

	JAVA_CLASSES = $(JAVA_HOME)/lib/classes.zip

	ifeq ($(JRE_HOME),)
		JRE_HOME = $(JAVA_HOME)
		JRE_CLASSES = $(JAVA_CLASSES)
	else
		ifeq ($(JRE_CLASSES),)
			JRE_CLASSES = $(JRE_HOME)/lib/classes.zip
		endif
	endif

	JAVA_LIBDIR = lib/sparc/$(JDK_THREADING_MODEL)

	# ** IMPORTANT ** having -lthread before -lnspr is critical on solaris when 
	# linking with -ljava as nspr redefines symbols in libthread that cause 
	# JNI executables to fail with assert of bad thread stack values.
	JAVA_CLIBS = -lthread

	INCLUDES += -I$(JAVA_HOME)/include/$(JDK_THREADING_MODEL)
	INCLUDES += -I$(JAVA_HOME)/include/$(JAVA_ARCH)

ifeq ($(JDK_DEBUG),1)
	JAVA_LIBS = -L$(JAVA_HOME)/$(JAVA_LIBDIR) -ljava_g $(JAVA_CLIBS)
else
	JAVA_LIBS = -L$(JAVA_HOME)/$(JAVA_LIBDIR) -ljava $(JAVA_CLIBS)
endif

	# enable JIT option on this platform
	JDK_JIT_OPT = -jit
endif

# set [Hewlett Packard HP-UX] platforms
ifeq ($(OS_ARCH), HP-UX)
	# no JIT option available on this platform
	JDK_JIT_OPT =
endif

# set [Redhat Linux] platforms
ifeq ($(OS_ARCH), Linux)
	# no JIT option available on this platform
	JDK_JIT_OPT =
endif

# set [IBM AIX] platforms
ifeq ($(OS_ARCH), AIX)
	# no JIT option available on this platform
	JDK_JIT_OPT =
endif

# set [Digital UNIX] platforms
ifeq ($(OS_ARCH), OSF1)
	# no JIT option available on this platform
	JDK_JIT_OPT =
endif

# set [Silicon Graphics IRIX] platforms
ifeq ($(OS_ARCH), IRIX)
	JAVA_ARCH = irix

	JAVA_CLASSES = $(JAVA_HOME)/lib/dev.jar:$(JAVA_HOME)/lib/rt.jar

	ifeq ($(JRE_HOME),)
		JRE_HOME = $(JAVA_HOME)
		JRE_CLASSES = $(JAVA_CLASSES)
	else
		ifeq ($(JRE_CLASSES),)
			JRE_CLASSES = $(JRE_HOME)/lib/dev.jar:$(JRE_HOME)/lib/rt.jar
		endif
	endif

	JAVA_LIBDIR = lib32/sgi/$(JDK_THREADING_MODEL)

	INCLUDES += -I$(JAVA_HOME)/include/$(JDK_THREADING_MODEL)
	INCLUDES += -I$(JAVA_HOME)/include/$(JAVA_ARCH)

ifeq ($(JDK_DEBUG),1)
	JAVA_LIBS = -L$(JAVA_HOME)/$(JAVA_LIBDIR) -ljava_g
else
	JAVA_LIBS = -L$(JAVA_HOME)/$(JAVA_LIBDIR) -ljava
endif

	# no JIT option available on this platform
	JDK_JIT_OPT =
endif

#######################################################################
# [4] Define remaining JDK "Core Components" default toolset options  #
#######################################################################

# set JDK optimization model
ifneq ($(BUILD_OPT),1)
	JDK_OPTIMIZER_OPT = -g
else
	JDK_OPTIMIZER_OPT = -O
endif

# set minimal JDK debugging model
ifeq ($(JDK_DEBUG),1)
	JDK_DEBUG_SUFFIX = _g
	JDK_DEBUG_OPT    = -debug
else
	JDK_DEBUG_SUFFIX =
	JDK_DEBUG_OPT    =
endif

# set default path to repository for JDK classes
ifeq ($(JDK_CLASS_REPOSITORY_OPT),)
	JDK_CLASS_REPOSITORY_OPT = -d $(JAVA_DESTPATH)
endif

# initialize the JDK heap size option to a default value
ifeq ($(JDK_INIT_HEAP_OPT),)
	JDK_INIT_HEAP_OPT = -ms8m
endif

# define a default JDK classpath
ifeq ($(JDK_CLASSPATH),)
	JDK_CLASSPATH = $(JAVA_DESTPATH)$(PATH_SEPARATOR)$(JAVA_SOURCEPATH)$(PATH_SEPARATOR)$(JAVA_CLASSES)
endif

# by default, override CLASSPATH environment variable using the JDK classpath option with $(JDK_CLASSPATH)
ifeq ($(JDK_CLASSPATH_OPT),)
	JDK_CLASSPATH_OPT = -classpath $(JDK_CLASSPATH)
endif

#######################################################################
# [5] Define JDK "Core Components" toolset;                           #
#     (always allow a user to override these values)                  #
#######################################################################

#
# (1) appletviewer, appletviewer_g
#

ifeq ($(APPLETVIEWER),)
	APPLETVIEWER_PROG   = $(JAVA_HOME)/bin/appletviewer$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	APPLETVIEWER_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	APPLETVIEWER_FLAGS += $(JDK_DEBUG_OPT)
	APPLETVIEWER_FLAGS += $(JDK_JIT_OPT)
	APPLETVIEWER        = $(APPLETVIEWER_PROG) $(APPLETVIEWER_FLAGS)
endif

#
# (2) jar, jar_g
#

ifeq ($(JAR),)
	JAR_PROG   = $(JAVA_HOME)/bin/jar$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	JAR_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAR        = $(JAR_PROG) $(JAR_FLAGS)
endif

#
# (3) java, java_g
#

ifeq ($(JAVA),)
	JAVA_PROG   = $(JAVA_HOME)/bin/java$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	JAVA_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVA_FLAGS += $(JDK_DEBUG_OPT)
	JAVA_FLAGS += $(JDK_CLASSPATH_OPT)
	JAVA_FLAGS += $(JDK_INIT_HEAP_OPT)
	JAVA_FLAGS += $(JDK_JIT_OPT)
	JAVA        = $(JAVA_PROG) $(JAVA_FLAGS) 
endif

#
# (4) javac, javac_g
#

ifeq ($(JAVAC),)
	JAVAC_PROG   = $(JAVA_HOME)/bin/javac$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	JAVAC_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVAC_FLAGS += $(JDK_OPTIMIZER_OPT)
	JAVAC_FLAGS += $(JDK_DEBUG_OPT)
	JAVAC_FLAGS += $(JDK_CLASSPATH_OPT)
	JAVAC_FLAGS += -J$(JDK_INIT_HEAP_OPT)
	JAVAC_FLAGS += $(JDK_CLASS_REPOSITORY_OPT)
	JAVAC        = $(JAVAC_PROG) $(JAVAC_FLAGS)
endif

#
# (5) javadoc, javadoc_g
#

ifeq ($(JAVADOC),)
	JAVADOC_PROG   = $(JAVA_HOME)/bin/javadoc$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	JAVADOC_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVADOC_FLAGS += $(JDK_CLASSPATH_OPT)
	JAVADOC_FLAGS += -J$(JDK_INIT_HEAP_OPT)
	JAVADOC        = $(JAVADOC_PROG) $(JAVADOC_FLAGS)
endif

#
# (6) javah, javah_g
#

ifeq ($(JAVAH),)
	JAVAH_PROG   = $(JAVA_HOME)/bin/javah$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	JAVAH_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVAH_FLAGS += $(JDK_CLASSPATH_OPT)
	JAVAH        = $(JAVAH_PROG) $(JAVAH_FLAGS)
endif

#
# (7) javakey, javakey_g
#

ifeq ($(JAVAKEY),)
	JAVAKEY_PROG   = $(JAVA_HOME)/bin/javakey$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	JAVAKEY_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVAKEY        = $(JAVAKEY_PROG) $(JAVAKEY_FLAGS)
endif

#
# (8) javap, javap_g
#

ifeq ($(JAVAP),)
	JAVAP_PROG   = $(JAVA_HOME)/bin/javap$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	JAVAP_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVAP_FLAGS += $(JDK_CLASSPATH_OPT)
	JAVAP_FLAGS += -J$(JDK_INIT_HEAP_OPT)
	JAVAP        = $(JAVAP_PROG) $(JAVAP_FLAGS)
endif

#
# (9) javat, javat_g
#

ifeq ($(JAVAT),)
	JAVAT_PROG   = $(JAVA_HOME)/bin/javat$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	JAVAT_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVAT        = $(JAVAT_PROG) $(JAVAT_FLAGS)
endif

#
# (10) javaverify, javaverify_g
#

ifeq ($(JAVAVERIFY),)
	JAVAVERIFY_PROG   = $(JAVA_HOME)/bin/javaverify$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	JAVAVERIFY_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JAVAVERIFY        = $(JAVAVERIFY_PROG) $(JAVAVERIFY_FLAGS)
endif

#
# (11) javaw, javaw_g
#

ifeq ($(JAVAW),)
	jJAVAW_PROG   = $(JAVA_HOME)/bin/javaw$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	jJAVAW_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	jJAVAW_FLAGS += $(JDK_DEBUG_OPT)
	jJAVAW_FLAGS += $(JDK_CLASSPATH_OPT)
	jJAVAW_FLAGS += $(JDK_INIT_HEAP_OPT)
	jJAVAW_FLAGS += $(JDK_JIT_OPT)
	jJAVAW        = $(JAVAW_PROG) $(JAVAW_FLAGS)
endif

#
# (12) jdb, jdb_g
#

ifeq ($(JDB),)
	JDB_PROG   = $(JAVA_HOME)/bin/jdb$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	JDB_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JDB_FLAGS += $(JDK_DEBUG_OPT)
	JDB_FLAGS += $(JDK_CLASSPATH_OPT)
	JDB_FLAGS += $(JDK_INIT_HEAP_OPT)
	JDB_FLAGS += $(JDK_JIT_OPT)
	JDB        = $(JDB_PROG) $(JDB_FLAGS)
endif

#
# (13) jre, jre_g
#

ifeq ($(JRE),)
	JRE_PROG   = $(JAVA_HOME)/bin/jre$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	JRE_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JRE_FLAGS += $(JDK_CLASSPATH_OPT)
	JRE_FLAGS += $(JDK_INIT_HEAP_OPT)
	JRE_FLAGS += $(JDK_JIT_OPT)
	JRE        = $(JRE_PROG) $(JRE_FLAGS) 
endif

#
# (14) jrew, jrew_g
#

ifeq ($(JREW),)
	JREW_PROG   = $(JAVA_HOME)/bin/jrew$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	JREW_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	JREW_FLAGS += $(JDK_CLASSPATH_OPT)
	JREW_FLAGS += $(JDK_INIT_HEAP_OPT)
	JREW_FLAGS += $(JDK_JIT_OPT)
	JREW        = $(JREW_PROG) $(JREW_FLAGS) 
endif

#
# (15) native2ascii, native2ascii_g
#

ifeq ($(NATIVE2ASCII),)
	NATIVE2ASCII_PROG   = $(JAVA_HOME)/bin/native2ascii$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	NATIVE2ASCII_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	NATIVE2ASCII        = $(NATIVE2ASCII_PROG) $(NATIVE2ASCII_FLAGS) 
endif

#
# (16) rmic, rmic_g
#

ifeq ($(RMIC),)
	RMIC_PROG   = $(JAVA_HOME)/bin/rmic$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	RMIC_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	RMIC_FLAGS += $(JDK_OPTIMIZER_OPT)
	RMIC_FLAGS += $(JDK_CLASSPATH_OPT)
	RMIC        = $(RMIC_PROG) $(RMIC_FLAGS) 
endif

#
# (17) rmiregistry, rmiregistry_g
#

ifeq ($(RMIREGISTRY),)
	RMIREGISTRY_PROG   = $(JAVA_HOME)/bin/rmiregistry$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	RMIREGISTRY_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	RMIREGISTRY        = $(RMIREGISTRY_PROG) $(RMIREGISTRY_FLAGS) 
endif

#
# (18) serialver, serialver_g
#

ifeq ($(SERIALVER),)
	SERIALVER_PROG   = $(JAVA_HOME)/bin/serialver$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)
	SERIALVER_FLAGS  = $(JDK_THREADING_MODEL_OPT)
	SERIALVER        = $(SERIALVER_PROG) $(SERIALVER_FLAGS) 
endif
 
