# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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

