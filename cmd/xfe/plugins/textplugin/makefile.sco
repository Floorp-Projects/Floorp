#!make
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


#
# Sample text plugin makefile
#
# Platform: SCO UnixWare 2.01 and above
#
# The output of the make process will be libtextplugin.so
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
# XP_UNIX	This needs to get defined for npapi.h on unix platforms.
# PLUGIN_TRACE	Enable this define to get debug prints whenever the plugin
#		api gets control.
#		
# - dp Suresh <dp@netscape.com>
# Wed May 15 23:03:36 PDT 1996
#
# Makefile ported to SCO 
# - Roger Oberholtzer <roger@seaotter.opq.se.netscape.com>
# Wed, 26 Jun 1996 13:44:31 +0200 (MET DST)
#

# PLUGIN_DEFINES= -DXP_UNIX -DPLUGIN_TRACE
PLUGIN_DEFINES= -DXP_UNIX

CC= cc
OPTIMIZER= -O
CFLAGS= -KPIC $(OPTIMIZER) $(PLUGIN_DEFINES) -I. -I/usr/include -I/usr/X/include

SRC= npunix.c npshell.c 
OBJ= npunix.o npshell.o
SHAREDTARGET=libtextplugin.so

.c.o:
	$(CC) -c $(CFLAGS) $<

default all: $(SHAREDTARGET)

$(SHAREDTARGET): $(OBJ)
	@cc -G -o $(SHAREDTARGET) $(OBJ)

clean:
	$(RM) $(OBJ) $(SHAREDTARGET)

