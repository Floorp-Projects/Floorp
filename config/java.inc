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

!if !defined(VERBOSE)
.SILENT:
!endif
#//------------------------------------------------------------------------
#//
#// This makefile contains all of the rules necessary to build Java Header
#// and Stub files
#//
#//------------------------------------------------------------------------
!if !defined(CONFIG_JAVA_INC)
CONFIG_JAVA_INC=1

#//------------------------------------------------------------------------
#// Setup to generate Mac java headers (checkout ns/lib/mac/Java
#//------------------------------------------------------------------------

!ifdef MOZ_GENMAC
MAC_JAVA_HEADERS_DIR = $(DEPTH)/lib/mac/Java

#//------------------------------------------------------------------------
#// Figure out how to do the pull.
#//------------------------------------------------------------------------
!if "$(MOZ_BRANCH)" != ""
CVS_BRANCH=-r $(MOZ_BRANCH)
HAVE_BRANCH=1
!else
HAVE_BRANCH=0
!endif

!if "$(MOZ_TIP)" != ""
CVS_BRANCH=-A
!endif


!if "$(MOZ_DATE)" != ""
CVS_BRANCH=-D "$(MOZ_DATE)"
HAVE_DATE=1
!else
HAVE_DATE=0
!endif

!if $(HAVE_DATE) && $(HAVE_BRANCH)
ERR_MESSAGE=$(ERR_MESSAGE)^
Cannot specify both MOZ_BRANCH and MOZ_DATE
!endif

$(MAC_JAVA_HEADERS_DIR):
        @echo +++ Checking out $(MAC_JAVA_HEADERS_DIR) +++
        $(PERL) $(DEPTH)/config/cvsco.pl $(CVS_BRANCH) ns/lib/mac/Java
        @echo +++ Done Checking out $(MAC_JAVA_HEADERS_DIR) +++
!endif 

!ifdef JDK_GEN
!ifdef JAVA_OR_OJI
#//------------------------------------------------------------------------
#//
#// Rule to generate Java header files using javah.
#//
#//------------------------------------------------------------------------
$(JDK_GEN):: display_java_header_msg
	!$(JAVAH) -d _gen $@

!ifdef NSBUILDROOT
LINCS = $(LINCS) -I$(JDK_GEN_DIR) -I$(XPDIST)
!else
LINCS = $(LINCS) -I$(JDK_GEN_DIR)
!endif

display_java_header_msg:
	echo +++ make: Generating java header files...

!ifdef MOZ_GENMAC
$(JDK_GEN)::display_java_header_msg_mac $(MAC_JAVA_HEADERS_DIR)
        echo +++ make: creating JDK header for $@
        !$(JAVAH) -mac -d $(MAC_JAVA_HEADERS_DIR)/_gen $@
         
display_java_header_msg_mac:
        echo +++ make: Generating Macintosh JDK header files +++
!endif
!endif # JAVA_OR_OJI
!endif


!ifdef JDK_GEN
!ifdef JAVA_OR_OJI
#//------------------------------------------------------------------------
#//
#// Rules to generate Java stub files using javah.
#//
#//------------------------------------------------------------------------
$(JDK_GEN)::display_java_stub_msg 
	!$(JAVAH) -stubs -d _stubs $@

display_java_stub_msg:
	echo +++ make: Generating java stub files...

!ifdef MOZ_GENMAC
$(JDK_GEN)::display_java_stub_msg_mac $(MAC_JAVA_HEADERS_DIR)
        echo +++ make: creating JDK stub for $@
        !$(JAVAH) -mac -stubs -d $(MAC_JAVA_HEADERS_DIR)/_stubs $@
         
display_java_stub_msg_mac:
        echo +++ make: Generating Macintosh JDK stub files +++
!endif

export:: $(JDK_GEN)

!endif # JAVA_OR_OJI
!endif

!ifdef JAVA_LIBSTUB_FILES
!ifdef JAVA_OR_OJI
#//------------------------------------------------------------------------
#//
#// Rules to generate libstubs.c using javah.
#//
#//------------------------------------------------------------------------
libstubs.c::$(JAVA_LIBSTUB_FILES)
	echo +++ make: Generating libstubs.c...
    $(JAVAH) -o libstubs.c -stubs $**

stubs$(MOZ_BITS).c::$(JAVA_LIBSTUB_FILES)
	echo +++ make: Generating stubs$(MOZ_BITS).c...
    $(JAVAH) -o stubs$(MOZ_BITS).c -stubs $**

!endif # JAVA_OR_OJI
!endif

!ifdef JRI_GEN
!ifdef JAVA_OR_OJI
#//------------------------------------------------------------------------
#//
#// Rule to generate JRI header files using javah.
#//
#//------------------------------------------------------------------------
$(JRI_GEN):: display_jri_header_msg
    !$(JAVAH) -jri -d _jri $@

!ifdef NSBUILDROOT
LINCS = $(LINCS) -I$(JRI_GEN_DIR) -I$(XPDIST)
!else
LINCS = $(LINCS) -I$(JRI_GEN_DIR)
!endif

display_jri_header_msg:
	echo +++ make: Generating JRI header files...

!ifdef MOZ_GENMAC
$(JRI_GEN)::display_jri_header_msg_mac $(MAC_JAVA_HEADERS_DIR)
        echo +++ make: creating JRI header for $@
        !$(JAVAH) -mac -jri -d $(MAC_JAVA_HEADERS_DIR)/_jri $@
         
display_jri_header_msg_mac:
        echo +++ make: Generating Macintosh java JRI header files +++
!endif
!endif # JAVA_OR_OJI
!endif

!ifdef JRI_GEN
!ifdef JAVA_OR_OJI
#//------------------------------------------------------------------------
#//
#// Rules to generate JRI stub files using javah.
#//
#//------------------------------------------------------------------------
$(JRI_GEN)::display_jri_stub_msg
    !$(JAVAH) -stubs -jri -d _jri $@

display_jri_stub_msg:
	echo +++ make: Generating JRI stub files...

!ifdef MOZ_GENMAC
$(JRI_GEN)::display_jri_stub_msg_mac $(MAC_JAVA_HEADERS_DIR)
        echo +++ make: creating JRI stub for $@
        !$(JAVAH) -mac -jri -stubs -d $(MAC_JAVA_HEADERS_DIR)/_jri $@
         
display_jri_stub_msg_mac:
        echo +++ make: Generating Macintosh java JRI stub files +++
!endif

export:: $(JRI_GEN)

!endif # JAVA_OR_OJI
!endif


!ifdef JNI_GEN
!ifdef JAVA_OR_OJI
#//------------------------------------------------------------------------
#//
#// Rule to generate JNI header files using javah.
#//
#//------------------------------------------------------------------------
$(JNI_GEN):: display_jni_header_msg
    !echo $(JAVAH) -jni -d _jni $@
    !$(JAVAH) -jni -d _jni $@

!ifdef NSBUILDROOT
LINCS = $(LINCS) -I$(JNI_GEN_DIR) -I$(XPDIST)
!else
LINCS = $(LINCS) -I$(JNI_GEN_DIR)
!endif

display_jni_header_msg:
	echo +++ make: Generating JNI header files...

!ifdef MOZ_GENMAC
$(JNI_GEN)::display_jni_header_msg_mac $(MAC_JAVA_HEADERS_DIR)
        echo +++ make: creating JRI header for $@
        !$(JAVAH) -mac -jni -d $(MAC_JAVA_HEADERS_DIR)/_jni $@
         
display_jni_header_msg_mac:
        echo +++ make: Generating Macintosh java JNI header files +++
!endif
!endif # JAVA_OR_OJI
export:: $(JNI_GEN)
!endif # JNI_GEN

!endif # CONFIG_JAVA_INC
