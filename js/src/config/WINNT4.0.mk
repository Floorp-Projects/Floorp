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
# Config for Windows NT using MS Visual C++ (version?)
#

CC = cl

RANLIB = echo

#.c.o:
#      $(CC) -c -MD $*.d $(CFLAGS) $<

CPU_ARCH = x86 # XXX fixme
GFX_ARCH = win32

# MSVC compiler options for both debug/optimize
# /nologo  - suppress copyright message
# /W3      - Warning level 3
# /Gm      - enable minimal rebuild
# /Z7      - put debug info into the executable, not in .pdb file
# /YX      - automatic precompiled headers
# /GX      - enable C++ exception support
WIN_CFLAGS = /nologo /W3 /Fp$(OBJDIR)/js.pch

# MSVC compiler options for debug builds linked to MSVCRTD.DLL
# /MDd     - link with MSVCRTD.LIB (Dynamically-linked, multi-threaded, debug C-runtime)
# /Od      - minimal optimization
WIN_IDG_CFLAGS = /MDd /Od /Z7 

# MSVC compiler options for debug builds linked to MSVCRT.DLL
# /MD      - link with MSVCRT.LIB (Dynamically-linked, multi-threaded, debug C-runtime)
# /Od      - minimal optimization
WIN_DEBUG_CFLAGS = /MD /Od /Z7 

# MSVC compiler options for release (optimized) builds
# /MD      - link with MSVCRT.LIB (Dynamically-linked, multi-threaded, C-runtime)
# /O2      - Optimize for speed
# /G5      - Optimize for Pentium
WIN_OPT_CFLAGS = /MD /O2

ifdef BUILD_OPT
OPTIMIZER = $(WIN_OPT_CFLAGS)
else
ifdef BUILD_IDG
OPTIMIZER = $(WIN_IDG_CFLAGS)
else
OPTIMIZER = $(WIN_DEBUG_CFLAGS)
endif
endif

OS_CFLAGS = -DXP_PC -DWIN32 -D_WINDOWS -D_WIN32 $(WIN_CFLAGS)
JSDLL_CFLAGS = -DEXPORT_JS_API
OS_LIBS = -lm -lc

PREBUILT_CPUCFG = 1
USE_MSVC = 1

LIB_LINK_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib oldnames.lib /nologo\
 /subsystem:windows /dll /debug /pdb:none\
 /machine:I386

EXE_LINK_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib oldnames.lib /nologo\
 /subsystem:console /debug /pdb:none\
 /machine:I386

# CAFEDIR = t:/cafe
# JCLASSPATH = $(CAFEDIR)/Java/Lib/classes.zip
# JAVAC = $(CAFEDIR)/Bin/sj.exe
# JAVAH = $(CAFEDIR)/Java/Bin/javah.exe
# JCFLAGS = -I$(CAFEDIR)/Java/Include -I$(CAFEDIR)/Java/Include/win32
