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
# The Original Code is the Win32 Version System.
#
# The Initial Developer of the Original Code is Aki Sasaki <asasaki@netscape.com>
# Portions created by the Initial Developer are Copyright (C) 2001
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

!if !defined(CONFIG_RESFILE_INC)
CONFIG_RESFILE_INC=1

!if !defined(RESFILE) && (defined(DLL) || defined(PROGRAM))

#//------------------------------------------------------------------------
#// Build .rc and .res files if $(RESFILE) isn't defined.
#// PBI      : ??
#// DEBUG    : $(MOZ_DEBUG)
#// OFFICIAL : $(MOZILLA_OFFICIAL)
#// MSTONE   : grabs automagically
#// OBJDIR   : $(OBJDIR)
#// MODNAME  : $(MODULE)
#// DEPTH    : $(DEPTH)
#// BINARY   : ??
#// BITS     : $(MOZ_BITS)
#// RCINCLUDE: ??
#// QUIET    : 1
#//------------------------------------------------------------------------

_RC_STRING = -OBJDIR $(OBJDIR) -DEPTH $(DEPTH) -TOPSRCDIR $(topsrcdir) -BITS $(MOZ_BITS) -QUIET 1 -SRCDIR .
!if defined(MOZ_DEBUG)
_RC_STRING = $(_RC_STRING) -DEBUG 1
!endif
!if defined(MOZILLA_OFFICIAL) || defined(BUILD_OFFICIAL)
_RC_STRING = $(_RC_STRING) -OFFICIAL 1
!endif
!if defined(MODULE)
_RC_STRING = $(_RC_STRING) -MODNAME $(MODULE)
!endif

RESFILE=$(OBJDIR)\module.res

all::

$(RESFILE):: $(OBJDIR)\module.rc

$(OBJDIR)\module.rc:
	echo $(PERL) $(topsrcdir)\config\version_win.pl $(_RC_STRING)
	$(PERL) $(topsrcdir)\config\version_win.pl $(_RC_STRING)
	$(RC) $(OBJDIR)\module.rc

clobber_resfile:
    -rm -f $(RESFILE) $(OBJDIR)\module.rc

clobber:: clobber_resfile

!endif
!endif # !CONFIG_RESFILE_INC
