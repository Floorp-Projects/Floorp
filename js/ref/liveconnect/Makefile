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
# JSRef GNUmake makefile.
#

# Change this!!!
JDK = /u/rchinta/util/java/jdk1.1.5

DEPTH = ..

# Set os+release dependent make variables
OS_ARCH         := $(subst /,_,$(shell uname -s))

# Attempt to differentiate between SunOS 5.4 and x86 5.4
OS_CPUARCH      := $(shell uname -m)
ifeq ($(OS_CPUARCH),i86pc)
OS_RELEASE      := $(shell uname -r)_$(OS_CPUARCH)
else
ifeq ($(OS_ARCH),AIX)
OS_RELEASE      := $(shell uname -v).$(shell uname -r)
else
OS_RELEASE      := $(shell uname -r)
endif
endif

# Virtually all Linux versions are identical.
# Any distinctions are handled in linux.h
ifeq ($(OS_ARCH),Linux)
OS_CONFIG      := Linux_All
else
OS_CONFIG       := $(OS_ARCH)$(OS_OBJTYPE)$(OS_RELEASE)
endif

ASFLAGS         =
DEFINES         =

#NS_USE_NATIVE = 1

include $(DEPTH)/config/$(OS_CONFIG).mk

include config/$(OS_CONFIG).mk

ifdef BUILD_OPT
OPTIMIZER  = -O
DEFINES    += -UDEBUG -DNDEBUG -UDEBUG_$(shell whoami)
OBJDIR_TAG = _OPT
else
ifdef USE_MSVC
OPTIMIZER  = -Zi
else
OPTIMIZER  = -g
endif
DEFINES    += -DDEBUG -DDEBUG_$(shell whoami)
OBJDIR_TAG = _DBG
endif

# Name of the binary code directories
OBJDIR          = $(OS_CONFIG)$(OBJDIR_TAG).OBJ
VPATH           = $(OBJDIR)

# Automatic make dependencies file
DEPENDENCIES    = $(OBJDIR)/.md

define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); mkdir $(@D); fi
endef

INCLUDES   += -I. -I.. -I$(JDK)/include

#
# XCFLAGS may be set in the environment or on the gmake command line
#
CFLAGS          = $(OPTIMIZER) $(OS_CFLAGS) $(DEFINES) $(INCLUDES) \
                  -DJSFILE $(XCFLAGS)
LDFLAGS		+= -lm $(XLDFLAGS)

# For purify
PURE_CFLAGS     = -DXP_UNIX $(OPTIMIZER) $(PURE_OS_CFLAGS) $(DEFINES) \
                  $(INCLUDES) $(XCFLAGS)

#
# LC file lists
#
LC_HFILES =		\
	jsj_hash.h      \
	jsj_private.h   \
	jsjava.h        \
	netscape_javascript_JSObject.h \
	$(NULL)

HFILES = $(LC_HFILES)

LC_CFILES =	\
	jsj.c \
	jsj_JSObject.c \
	jsj_JavaArray.c \
	jsj_JavaClass.c \
	jsj_JavaMember.c \
	jsj_JavaObject.c \
	jsj_JavaPackage.c \
	jsj_array.c \
	jsj_class.c \
	jsj_convert.c \
	jsj_field.c \
	jsj_hash.c \
	jsj_method.c \
	jsj_utils.c \
	$(NULL)

LIB_CFILES  = $(LC_CFILES)

PROG_FILE = js

ifdef USE_MSVC
LIB_OBJS  = $(addprefix $(OBJDIR)/, $(LIB_CFILES:.c=.obj))
PROG_OBJ = $(OBJDIR)/$(PROG_FILE).obj
else
LIB_OBJS  = $(addprefix $(OBJDIR)/, $(LIB_CFILES:.c=.o))
LIB_OBJS  += $(addprefix $(OBJDIR)/, $(LIB_ASFILES:.s=.o))
PROG_OBJ = $(OBJDIR)/$(PROG_FILE).o
endif

ifdef USE_MSVC
LIBRARY = $(OBJDIR)/LiveConnect_g.dll
PROGRAM = $(OBJDIR)/LC_js
else
LIBRARY = $(OBJDIR)/LiveConnect_g.so
PROGRAM = $(OBJDIR)/LC_js
endif

ifdef USE_MSVC
TARGETS = $(LIBRARY)   # $(PROGRAM) not supported for MSVC yet
else
TARGETS = $(LIBRARY) $(PROGRAM)
endif

all: $(TARGETS)

lib: $(LIBRARY)

clean:
	rm -rf $(LIB_OBJS)

clobber:
	rm -rf $(LIB_OBJS) $(PROG_OBJ) $(TARGETS) $(DEPENDENCIES)

depend:
	gcc -MM $(CFLAGS) $(JS_CFILES)

$(OBJDIR)/%: %.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ $(CFLAGS) $*.c $(LDFLAGS)

$(OBJDIR)/%.o: %.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c $(CFLAGS) $*.c

$(PROG_OBJ): ../$(PROG_FILE).c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c $(CFLAGS) -DLIVECONNECT ../$(PROG_FILE).c

$(OBJDIR)/%.o: %.s
	@$(MAKE_OBJDIR)
	$(AS) -o $@ $(ASFLAGS) $*.s

# windows only
$(OBJDIR)/%.obj: %.c
	@$(MAKE_OBJDIR)
	$(CC) -Fo$(OBJDIR)/ -c $(CFLAGS) $*.c

ifeq ($(OS_ARCH),OS2)
$(LIBRARY): $(LIB_OBJS)
	$(AR) $@ $? $(AR_OS2_SUFFIX)
	$(RANLIB) $@
else
ifdef USE_MSVC
$(LIBRARY): $(LIB_OBJS)
	link.exe $(LIB_LINK_FLAGS) /base:0x61000000 \
	    /out:"$@" /pdb:"$(OBJDIR)/js32.pdb" /implib:"$(OBJDIR)/js32.lib" $?
else
#$(LIBRARY): $(LIB_OBJS)
#	$(AR) rv $@ $?
#	$(RANLIB) $@
$(LIBRARY): $(LIB_OBJS)
	ld -G -o $@ $(LIB_OBJS) $(LDFLAGS) $(OS_LIBS) $(OTHER_LIBS) 
endif
endif

$(PROGRAM): $(PROG_OBJ) $(LIBRARY)
	$(CC) -o $@ $(CFLAGS) $(PROG_OBJ) $(LDFLAGS) $(LIBRARY) $(OS_LIBS) $(OTHER_LIBS) 
