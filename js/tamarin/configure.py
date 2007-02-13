#!/usr/bin/env python
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

#
# This script runs just like a traditional configure script, to do configuration
# testing and makefile generation.

import os.path
import sys

thisdir = os.path.dirname(os.path.abspath(__file__))

# Look for additional modules in our build/ directory.
sys.path.append(thisdir)

import build.configuration
from build.configuration import *
import build.getopt

o = build.getopt.Options()

config = Configuration(thisdir, options = o,
		       sourcefile = 'core/avmplus.h')

buildShell = o.getBoolArg("shell", False)
if (buildShell):
    config.subst("ENABLE_SHELL", 1)

# Get CPP, CC, etc
config.getCompiler()

APP_CPPFLAGS = "-DSOFT_ASSERTS "
APP_CXXFLAGS = ""
OPT_CXXFLAGS = "-Os "
OPT_CPPFLAGS = ""
DEBUG_CPPFLAGS = "-DDEBUG -D_DEBUG "
DEBUG_CXXFLAGS = "-g "

if config.COMPILER_IS_GCC:
    APP_CXXFLAGS = "-fno-exceptions -fno-rtti -Werror -Wall -Wno-reorder -Wno-switch -Wno-invalid-offsetof -fmessage-length=0 -finline-functions -finline-limit=65536 "
else:
    raise Exception("Not implemented")

os = config.getHost()[0]
if os == "darwin":
    APP_CPPFLAGS += "-DTARGET_API_MAC_CARBON=1 -DDARWIN=1 -D_MAC -DTARGET_RT_MAC_MACHO=1 -DUSE_MMAP -D_MAC -D__DEBUGGING__ "
    APP_CXXFLAGS += "-fpascal-strings -faltivec -fasm-blocks -mmacosx-version-min=10.4 -isysroot /Developer/SDKs/MacOSX10.4u.sdk "

if o.getBoolArg("debugger"):
    APP_CPPFLAGS += "-DDEBUGGER "

config.subst("APP_CPPFLAGS", APP_CPPFLAGS)
config.subst("APP_CXXFLAGS", APP_CXXFLAGS)
config.subst("OPT_CPPFLAGS", OPT_CPPFLAGS)
config.subst("OPT_CXXFLAGS", OPT_CXXFLAGS)
config.subst("DEBUG_CPPFLAGS", DEBUG_CPPFLAGS)
config.subst("DEBUG_CXXFLAGS", DEBUG_CXXFLAGS)
config.generate("Makefile")
o.finish()

