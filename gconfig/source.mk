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

#######################################################################
# Master <component>-specific source import/export directories        #
#######################################################################

#
# <user_source_tree> master import/export directory prefix
#

SOURCE_PREFIX = $(GDEPTH)/dist

#
# <user_source_tree> cross-platform (xp) master import/export directory
#

SOURCE_XP_DIR        = $(SOURCE_PREFIX)

#
# <user_source_tree> cross-platform (xp) import/export directories
#

SOURCE_CLASSES_DIR   = $(SOURCE_XP_DIR)/classes
SOURCE_XPHEADERS_DIR = $(SOURCE_XP_DIR)/public/$(MODULE)
SOURCE_XPPRIVATE_DIR = $(SOURCE_XP_DIR)/private/$(MODULE)

#
# <user_source_tree> machine-dependent (md) master import/export directory
#

SOURCE_MD_DIR        = $(SOURCE_PREFIX)/$(PLATFORM)

#
# <user_source_tree> machine-dependent (md) import/export directories
#

SOURCE_BIN_DIR       = $(SOURCE_MD_DIR)/bin
SOURCE_LIB_DIR       = $(SOURCE_MD_DIR)/lib
SOURCE_MDHEADERS_DIR = $(SOURCE_MD_DIR)/include

#######################################################################
# Master <component>-specific source release directories and files    #
#######################################################################

#
# <user_source_tree> source-side master release directory prefix
#

SOURCE_RELEASE_PREFIX = $(SOURCE_PREFIX)/release

#
# <user_source_tree> cross-platform (xp) source-side master release directory
#

SOURCE_RELEASE_XP_DIR = $(SOURCE_RELEASE_PREFIX)

#
# <user_source_tree> cross-platform (xp) source-side release directories
#

SOURCE_RELEASE_CLASSES_DIR   = classes
SOURCE_RELEASE_XPHEADERS_DIR = include

#
# <user_source_tree> cross-platform (xp) JAR source-side release files
#

XPCLASS_JAR  = xpclass.jar
XPHEADER_JAR = xpheader.jar

#
# <user_source_tree> machine-dependent (md) source-side master release directory
#

SOURCE_RELEASE_MD_DIR = $(PLATFORM)
#
# <user_source_tree> machine-dependent (md) source-side release directories
#

SOURCE_RELEASE_BIN_DIR       = $(PLATFORM)/bin
SOURCE_RELEASE_LIB_DIR       = $(PLATFORM)/lib
SOURCE_RELEASE_MDHEADERS_DIR = $(PLATFORM)/include
SOURCE_RELEASE_SPEC_DIR      = $(SOURCE_RELEASE_MD_DIR)

#
# <user_source_tree> machine-dependent (md) JAR/tar source-side release files
#

MDBINARY_JAR = mdbinary.jar
MDHEADER_JAR = mdheader.jar


# Where to put the results

RESULTS_DIR = $(RELEASE_TREE)/sectools/results

