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

# Set os+release dependent make variables
OS_ARCH         := $(subst /,_,$(shell uname -s))

# Attempt to differentiate between SunOS 5.4 and x86 5.4
OS_CPUARCH      := $(shell uname -m)
ifeq ($(OS_CPUARCH),i86pc)
OS_RELEASE      := $(shell uname -r)_$(OS_CPUARCH)
else
OS_RELEASE      := $(shell uname -r)
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

include ../../config/$(OS_CONFIG).mk

ifdef BUILD_OPT
OPTIMIZER  += -O
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

#DEFINES += -DJS_THREADSAFE

ifdef JS_NO_THIN_LOCKS
DEFINES += -DJS_USE_ONLY_NSPR_LOCKS
endif

# Name of the binary code directories
OBJDIR          = $(OS_CONFIG)$(OBJDIR_TAG).OBJ
VPATH           = $(OBJDIR)

# Automatic make dependencies file
DEPENDENCIES    = $(OBJDIR)/.md

define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); mkdir $(@D); fi
endef

# Look in OBJDIR to find prcpucfg.h
INCLUDES   = -I../.. -I../../$(OBJDIR)

#
# XCFLAGS may be set in the environment or on the gmake command line
#
CFLAGS          = $(OPTIMIZER) $(OS_CFLAGS) $(DEFINES) $(INCLUDES) \
                  -DJSFILE $(XCFLAGS)
LDFLAGS		= -lm $(XLDFLAGS)

# For purify
PURE_CFLAGS     = -DXP_UNIX $(OPTIMIZER) $(PURE_OS_CFLAGS) $(DEFINES) \
                  $(INCLUDES) $(XCFLAGS)

COMPILE_CFILES =	\
	compile.c	\
	$(NULL)

RUN_CFILES =		\
	run.c		\
	$(NULL)

COMMON_CFILES =		\
	common.c	\
	$(NULL)

ifdef USE_MSVC
COMPILE_OBJS = $(addprefix $(OBJDIR)/, $(COMPILE_CFILES:.c=.obj))
RUN_OBJS = $(addprefix $(OBJDIR)/, $(RUN_CFILES:.c=.obj))
COMMON_OBJS = $(addprefix $(OBJDIR)/, $(COMMON_CFILES:.c=.obj))
else
COMPILE_OBJS  = $(addprefix $(OBJDIR)/, $(COMPILE_CFILES:.c=.o))
RUN_OBJS  = $(addprefix $(OBJDIR)/, $(RUN_CFILES:.c=.o))
COMMON_OBJS  = $(addprefix $(OBJDIR)/, $(COMMON_CFILES:.c=.o))
endif

COMPILE_CFILES_REAL = $(COMPILE_CFILES) $(COMMON_CFILES)
COMPILE_OBJS_REAL   = $(COMPILE_OBJS) $(COMMON_OBJS)
RUN_CFILES_REAL = $(RUN_CFILES) $(COMMON_CFILES)
RUN_OBJS_REAL   = $(RUN_OBJS) $(COMMON_OBJS)

ifdef USE_MSVC
BASE_LIBRARY = $(OBJDIR)/js32.dll
else
BASE_LIBRARY = $(OBJDIR)/libjs.a
endif

LIBRARY = ../../$(BASE_LIBRARY)

COMPILE_PROGRAM = $(OBJDIR)/compile
RUN_PROGRAM = $(OBJDIR)/run

ifdef USE_MSVC
TARGETS = $(LIBRARY)   # $(PROGRAM) not supported for MSVC yet
else
TARGETS = $(LIBRARY) $(COMPILE_PROGRAM) $(RUN_PROGRAM)
endif

all: $(TARGETS)

compile: $(COMPILE_PROGRAM)

clean:
	rm -rf $(OBJS)

clobber:
	rm -rf $(OBJS) $(TARGETS) $(DEPENDENCIES)

depend:
	gcc -MM $(CFLAGS) $(JS_CFILES)

$(OBJDIR)/%: %.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ $(CFLAGS) $*.c $(LDFLAGS)

$(OBJDIR)/%.o: %.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c $(CFLAGS) $*.c

$(OBJDIR)/%.o: %.s
	@$(MAKE_OBJDIR)
	$(AS) -o $@ $(ASFLAGS) $*.s

# windows only
$(OBJDIR)/%.obj: %.c
	@$(MAKE_OBJDIR)
	$(CC) -Fo$(OBJDIR)/ -c $(CFLAGS) $*.c

$(COMPILE_PROGRAM): $(COMPILE_OBJS_REAL) $(LIBRARY)
	$(CC) -o $@ $(CFLAGS) $(COMPILE_OBJS_REAL) $(LIBRARY) \
		$(NSPR_LIBRARY) $(LDFLAGS)

$(RUN_PROGRAM): $(RUN_OBJS_REAL) $(LIBRARY)
	$(CC) -o $@ $(CFLAGS) $(RUN_OBJS_REAL) $(LIBRARY) \
		$(NSPR_LIBRARY) $(LDFLAGS)

$(LIBRARY):
	(cd ../..; gmake $(BASE_LIBRARY))

#$(HFILES) $(CFILES): $(OBJDIR)/prcpucfg.h

#ifdef PREBUILT_CPUCFG
#$(OBJDIR)/prcpucfg.h: prcpucfg.h
#	cp prcpucfg.h $(OBJDIR)
#else
#$(OBJDIR)/prcpucfg.h: $(OBJDIR)/prcpucfg
#	rm -f $@
#	$(OBJDIR)/prcpucfg > $@
#
#$(OBJDIR)/prcpucfg: $(OBJDIR)/prcpucfg.o
#	$(CC) -o $@ $(OBJDIR)/prcpucfg.o
#endif

#
# Hardwire dependencies on jsopcode.def
#
#jsopcode.h jsopcode.c: jsopcode.def

#-include $(DEPENDENCIES)

#TARNAME = jsref.tar
#TARFILES = files `cat files`

#tar:
#	tar cvf $(TARNAME) $(TARFILES)
#	gzip $(TARNAME)

#SUFFIXES: .i
#%.i: %.c
#	$(CC) -C -E $(CFLAGS) $< > $*.i
