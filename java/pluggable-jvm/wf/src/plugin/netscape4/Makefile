# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#  
# The Original Code is The Waterfall Java Plugin Module
#  
# The Initial Developer of the Original Code is Sun Microsystems Inc
# Portions created by Sun Microsystems Inc are Copyright (C) 2001
# All Rights Reserved.
# 
# $Id: Makefile,v 1.2 2001/07/12 19:58:21 edburns%acm.org Exp $
# 
# Contributor(s):
# 
#     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
# 

#!make
################################################################################
# Copyright (c) 1996 Netscape Communications. All rights reserved.
################################################################################
#
# Simple Sample plugin makefile
#
# Platform: Linux 1.2 (ELF)
#
# This makefile contains some of our defines for the compiler:
#
# XP_UNIX	This needs to get defined for npapi.h on unix platforms.
# PLUGIN_TRACE	Enable this define to get debug prints whenever the plugin
#		api gets control.
#		
# WARNING: Motif libraries are built static into the navigator and cannot
#          be accessed from the plugin.
#          
PLUGIN_DEFINES= -DXP_UNIX -DJVMP_USE_SHM
#PLUGIN_DEFINES= -DXP_UNIX -DPLUGIN_TRACE -DJVMP_USE_SHM
#PLUGIN_DEFINES= -DXP_UNIX -I../../../include -I../Source/_gen

CC= gcc
OPTIMIZER= -g
CFLAGS=  -Wall -Wno-unused $(OPTIMIZER) $(PLUGIN_DEFINES) -I. -I../../../public/


SRC=JavaPlugin.c npunix.c
OBJ=JavaPlugin.o npunix.o

SHAREDTARGET=npjvmp.so

default all: $(SHAREDTARGET)

$(SHAREDTARGET): $(OBJ) ../../../build/unix/shmtran.o
	$(CC) -shared -o $(SHAREDTARGET) $(OBJ) ../../../build/unix/shmtran.o $(LDFLAGS) -ldl 

npsimple.o: ../Source/npsimple.c
	$(CC) -c $(CFLAGS) ../Source/npsimple.c

stubs.o: ../Source/stubs.c
	$(CC) -c $(CFLAGS) ../Source/stubs.c

clean:
	$(RM) $(OBJ) $(SHAREDTARGET)
