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
# Configuration information for building in the "Core Components" source module
# This is a copy of the config/core config file without ruleset.mk.
# We need to do this until we can convince the config/core folks to not include
# ruleset.mk in their config.mk file
#

include $(DEPTH)/config/arch.mk

include $(DEPTH)/config/command.mk

include $(DEPTH)/config/$(OS_CONFIG).mk

PLATFORM = $(OBJDIR_NAME)

include $(DEPTH)/config/location.mk

include $(DEPTH)/config/prefix.mk

include $(DEPTH)/config/suffix.mk

