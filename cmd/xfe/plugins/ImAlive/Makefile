#!gmake
#
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

################################################################################
# 
#
# Template plugin makefile
#
# Platform: Solaris 2.4 and above
#
# The output of the make process will be ImAlive.so
# Install this file either in
#	/usr/lib/netscape/plugins/
#	(or)
#	~/.netscape/plugins/
#	(or) in any convenient directory and point environment variable
#	     NPX_PLUGIN_PATH to point to the directory. It is advisable
#	     that the plugins (.so) are the only files in that directory.
#
# This makefile contains some of our defines for the compiler:
#
# PLUGIN_TRACE	Enable this define to get debug prints whenever the plugin
#		api gets control.
#
################################################################################

# Assuming we are in ns/cmd/xfe/plugins/ImAlive
DEPTH		= ../../../..

CSRCS		= ImAlive.c stubs.c 

LIBRARY_NAME	= ImAlive

include $(DEPTH)/config/rules.mk

PLUGIN_DEFINES	= -DPLUGIN_TRACE

#CFLAGS		+= $(PLUGIN_DEFINES)

GARBAGE		+= Worked.nscp

$(OBJDIR)/stubs.o: npunix.c
