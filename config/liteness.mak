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
# MOZ_LITE and MOZ_MEDIUM stuff
#
# Does two things 1) Sets environment variables for use by other build scripts.
#                 2) Sets MOZ_LITENESS_FLAGS to be appended to the command lines
#                    for calling the compilers/tools, CFLAGS, RCFLAGS, etc.
#
################################################################################


# Under the new system for MOZ_LITE, MOZ_MEDIUM.  There should be no references to
# MOZ_LITE or MOZ_MEDIUM in the code, either as an #ifdef, or as some other conditional
# if in the build scripts.  Instead, all #ifdefs, !if, etc. should be based on the
# module-specific tag.  E.g. #ifdef MOZ_MAIL_NEWS, #ifdef MOZ_LDAP, etc.
# The reason for this is that we can decide what goes into the LITE and MEDIUM
# builds by just tweaking this file (and the appropriate file for MAC and UNIX).
#
# Originally, I planned on defining MOZ_MEDIUM as being the same as MOZ_LITE + EDITOR,
# but there were still many cases in the code and build scripts where I would have had 
# to do something ugly like "#if !defined(MOZ_LITE) || defined(MOZ_MEDIUM)".  This 
# would be very error prone and difficult to maintain.  I believe defining and
# using a bunch of new symbols reduces the total amount of pain in both the short and
# long term.
#
# IMPORTANT!!  The method of running a build has not changed.  You define 
# MOZ_LITE or MOZ_MEDIUM (not both) in your environment, and start the build.
# You do not have to define the symbols for every module.  E.g. you should never
# have to define EDITOR, or MOZ_MAIL_NEWS in your environment.  The build scripts
# will do this for you, via the file you are looking at right now.



### Here is the list of all possible modules under control of MOZ_LITE/MOZ_MEDIUM,
### Some only affect the build scripts, so are only set as environment variables, and
### not added to MOZ_LITENESS_FLAGS
#
# MOZ_MAIL_NEWS  
# Enables mail and news.
#
# EDITOR
# Enables the editor
#
# MOZ_OFFLINE
# Enables go offline/go online
#
# MOZ_LOC_INDEP
# Location independence
#
# MOZ_TASKBAR
# The "taskbar" or "component bar"
#
# MOZ_LDAP
# Enable LDAP, MOZ_NO_LDAP has been depreciated
#
# MOZ_ADMIN_LIB
# Mission Control
#
# MOZ_COMMUNICATOR_NAME
# Use "Communicator" as opposed to "Navigator" in strings,
# Use the Communicator icon, splash screen, etc. instead of the Navigator one.
# *** IMPORTANT ***  This also controls whether the user agent string has " ;Nav"
# appended to it.  i.e. only append " ;Nav" if MOZ_COMMUNICATOR_NAME is not set.
#
# MOZ_JSD
# Build JS debug code, needs to be turned off when we remove java from build.
#
# MOZ_IFC_TOOLS
# Build ns/ifc/tools.  Should this be the same as MOZ_JSD??
#
# MOZ_NETCAST
# Build netcaster.
#
# MOZ_COMMUNICATOR_IIDS
# For windows, use the COM IIDs for Communicator, as opposed to those for the 
# Navigator-only version. We must have a different set so that multiple versions can
# be installed on the same machine.  We need a more general solution to the problem of
# multiple versions of the interface IDs.
#
# MOZ_COMMUNICATOR_ABOUT
# Use the about: information from Communicator, as opposed to that for the Navigator-only
# version.  We will probably have to make another one for the source-only release.
# 
# MOZ_NAV_BUILD_PREFIX
# For building multiple versions with varying degree of LITEness in the same tree.  
# If true, use "Nav" as the prefix for the directory under ns/dist, else use "WIN".
# Also, if true, build client in, say, NavDbg as oppposed to x86Dbg.
#
# MOZ_COMMUNICATOR_CONFIG_JS
# Use "config.js" instead of the one specific to the Navigator-only version.
#
# MOZ_COPY_ALL_JARS
# Copy all JAR files to the destination directory, else just copy the JARS appropriate for the
# Navigator-only version.
#
# MOZ_SPELLCHK
# Enable the spellchecker.


### MOZ_LITE ###
# NOTE: Doesn't need -DMOZ_LITE anymore.
!if defined(MOZ_LITE)
MOZ_LITENESS_FLAGS=
MOZ_JSD=1
MOZ_NAV_BUILD_PREFIX=1


### MOZ_MEDIUM ###
!elseif defined(MOZ_MEDIUM)
MOZ_LITENESS_FLAGS=-DEDITOR -DMOZ_COMMUNICATOR_IIDS
EDITOR=1
MOZ_JSD=1
MOZ_COMMUNICATOR_IIDS=1
MOZ_COMMUNICATOR_CONFIG_JS=1
MOZ_COPY_ALL_JARS=1


### Full build ###
!else
MOZ_LITENESS_FLAGS=-DMOZ_MAIL_NEWS -DEDITOR -DMOZ_OFFLINE -DMOZ_LOC_INDEP \
                   -DMOZ_TASKBAR -DMOZ_LDAP -DMOZ_ADMIN_LIB \
                   -DMOZ_COMMUNICATOR_NAME -DMOZ_COMMUNICATOR_IIDS \
				   -DMOZ_NETCAST -DMOZ_COMMUNICATOR_ABOUT -DMOZ_SPELLCHK
MOZ_MAIL_NEWS=1
EDITOR=1
MOZ_OFFLINE=1
MOZ_LOC_INDEP=1
MOZ_TASKBAR=1
MOZ_LDAP=1
MOZ_ADMIN_LIB=1
MOZ_COMMUNICATOR_NAME=1
MOZ_JSD=1
MOZ_IFC_TOOLS=1
MOZ_NETCAST=1
MOZ_COMMUNICATOR_IIDS=1
MOZ_COMMUNICATOR_ABOUT=1
MOZ_COMMUNICATOR_CONFIG_JS=1
MOZ_COPY_ALL_JARS=1
MOZ_SPELLCHK=1
!endif
