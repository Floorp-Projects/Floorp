#!gmake
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

#######################################################################
# (1) Directory specific stuff                                        #
#######################################################################

DEPTH = ../../..

LIBRARY_NAME = EFDisassemble

CSRCS = 	XDisAsm.c				\
			$(NULL)	

LOCAL_MD_EXPORTS_x86 = 	XDisAsm.h				\
			$(NULL)

#######################################################################
# (2) Include "component" configuration information.                  #
#######################################################################

include $(DEPTH)/config/config.mk

#######################################################################
# (3) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################


#######################################################################
# (4) Execute "component" rules. (OPTIONAL)                           #
#######################################################################

include $(DEPTH)/config/rules.mk

#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################

# must make library first
# since the dll has no exported symbols
# no .lib will be created and since
# this piece of shit insists on making
# the fucking shared library lets make it
# happy
libs:: $(LIBRARY) $(SHARED_LIBRARY) 


