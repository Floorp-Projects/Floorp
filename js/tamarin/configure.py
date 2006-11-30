#!/usr/bin/env python
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

