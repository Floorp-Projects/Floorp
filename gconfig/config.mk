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

#
# Configuration information for building in the "Core Components" source module
#

#######################################################################
# [1.0] Master "Core Components" source and release <architecture>    #
#       tags                                                          #
#######################################################################

include $(GDEPTH)/gconfig/arch.mk

#######################################################################
# [2.0] Master "Core Components" default command macros               #
#       (NOTE: may be overridden in $(OS_CONFIG).mk)                  #
#######################################################################

include $(GDEPTH)/gconfig/command.mk

#######################################################################
# [3.0] Master "Core Components" <architecture>-specific macros       #
#       (dependent upon <architecture> tags)                          #
#######################################################################

include $(GDEPTH)/gconfig/$(OS_CONFIG).mk

#######################################################################
# [4.0] Master "Core Components" source and release <platform> tags   #
#       (dependent upon <architecture> tags)                          #
#######################################################################

include $(GDEPTH)/gconfig/platform.mk

#######################################################################
# [5.0] Master "Core Components" release <tree> tags                  #
#       (dependent upon <architecture> tags)                          #
#######################################################################

include $(GDEPTH)/gconfig/tree.mk

#######################################################################
# [6.0] Master "Core Components" source and release <component> tags  #
#       NOTE:  A component is also called a module or a subsystem.    #
#       (dependent upon $(MODULE) being defined on the                #
#        command line, as an environment variable, or in individual   #
#        makefiles, or more appropriately, manifest.mn)               #
#######################################################################

include $(GDEPTH)/gconfig/module.mk

#######################################################################
# [7.0] Master "Core Components" release <version> tags               #
#       (dependent upon $(MODULE) being defined on the                #
#        command line, as an environment variable, or in individual   #
#        makefiles, or more appropriately, manifest.mn)               #
#######################################################################

include $(GDEPTH)/gconfig/version.mk

#######################################################################
# [8.0] Master "Core Components" macros to figure out                 #
#       binary code location                                          #
#       (dependent upon <platform> tags)                              #
#######################################################################

include $(GDEPTH)/gconfig/location.mk

#######################################################################
# [9.0] Master "Core Components" <component>-specific source path     #
#       (dependent upon <user_source_tree>, <source_component>,       #
#        <version>, and <platform> tags)                              #
#######################################################################

include $(GDEPTH)/gconfig/source.mk

#######################################################################
# [10.0] Master "Core Components" include switch for support header   #
#        files                                                        #
#        (dependent upon <tree>, <component>, <version>,              #
#         and <platform> tags)                                        #
#######################################################################

include $(GDEPTH)/gconfig/headers.mk

#######################################################################
# [11.0] Master "Core Components" for computing program prefixes      #
#######################################################################

include $(GDEPTH)/gconfig/prefix.mk

#######################################################################
# [12.0] Master "Core Components" for computing program suffixes      #
#        (dependent upon <architecture> tags)                         #
#######################################################################

include $(GDEPTH)/gconfig/suffix.mk

#######################################################################
# [13.0] Master "Core Components" rule set                            #
#        (should always be the last file included by config.mk)       #
#######################################################################

include $(GDEPTH)/gconfig/ruleset.mk
