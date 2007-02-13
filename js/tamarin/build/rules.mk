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
# The Original Code is [Open Source Virtual Machine].
#
# The Initial Developer of the Original Code is
# Adobe System Incorporated.
# Portions created by the Initial Developer are Copyright (C) 2005-2006
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

# For the moment, pretend that nothing but mac exists, and we can always
# use gcc automatic dependencies.

# A "thing" is any static library, shared library, or program.
# things are made up of CXXSRCS and CSRCS.
#
# By default, we use CPPFLAGS/CFLAGS/CXXFLAGS/LDFLAGS.
# This can be overridden using thingname_CPPFLAGS
#
# If you want to *add* flags (without overriding the defaults), use
# thingname_EXTRA_CPPFLAGS
#
# the default target is "all::". Individual manifest.mk should add
# all:: dependencies for any object that should be made by default.

# STATIC_LIBRARIES:
# By default, the library base name is the thingname. To override, set
# thingname_BASENAME

THINGS = \
  $(STATIC_LIBRARIES) \
  $(SHARED_LIBRARIES) \
  $(PROGRAMS) \
  $(NULL)

$(foreach thing,$(THINGS),$(eval $(call THING_SRCS,$(thing))))
$(foreach lib,$(STATIC_LIBRARIES),$(eval $(call STATIC_LIBRARY_RULES,$(lib))))
$(foreach program,$(PROGRAMS),$(eval $(call PROGRAM_RULES,$(program))))

clean::
	rm -f $(GARBAGE)
