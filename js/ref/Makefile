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
INCLUDES   = -I$(OBJDIR)

#
# XCFLAGS may be set in the environment or on the gmake command line
#
CFLAGS          = $(OPTIMIZER) $(OS_CFLAGS) $(DEFINES) $(INCLUDES) \
                  -DJSFILE $(XCFLAGS)
LDFLAGS		= -lm $(XLDFLAGS)

# For purify
PURE_CFLAGS     = -DXP_UNIX $(OPTIMIZER) $(PURE_OS_CFLAGS) $(DEFINES) \
                  $(INCLUDES) $(XCFLAGS)

#
# JS file lists
#
PR_HFILES =		\
	prarena.h	\
	prassert.h	\
	prclist.h	\
	prdtoa.h	\
	prhash.h	\
	prlong.h	\
	prmacos.h	\
	prosdep.h	\
	prpcos.h	\
	prprintf.h	\
	prtime.h	\
	prtypes.h	\
	prunixos.h	\
	$(NULL)

JS_HFILES =		\
	jsarray.h	\
	jsatom.h	\
	jsbool.h	\
	jsconfig.h	\
	jscntxt.h	\
	jsdate.h	\
	jsemit.h	\
	jsfun.h		\
	jsgc.h		\
	jsinterp.h	\
	jslock.h	\
	jsmath.h	\
	jsnum.h		\
	jsobj.h		\
	jsopcode.h      \
	jsparse.h	\
	jsprvtd.h	\
	jspubtd.h	\
	jsregexp.h	\
	jsscan.h	\
	jsscope.h	\
	jsscript.h	\
	jsstr.h		\
	jsxdrapi.h	\
	$(NULL)

API_HFILES =		\
	jsapi.h		\
	jsdbgapi.h	\
	$(NULL)

HFILES = $(PR_HFILES) $(JS_HFILES) $(API_HFILES)

PR_CFILES =		\
	prarena.c	\
	prassert.c	\
	prdtoa.c	\
	prhash.c	\
	prlog2.c	\
	prlong.c	\
	prprintf.c	\
	prtime.c	\
	$(NULL)

JS_CFILES =		\
	jsapi.c		\
	jsarray.c	\
	jsatom.c	\
	jsbool.c	\
	jscntxt.c	\
	jsdate.c	\
	jsdbgapi.c	\
	jsemit.c	\
	jsfun.c		\
	jsgc.c		\
	jsinterp.c	\
	jslock.c	\
	jsmath.c	\
	jsnum.c		\
	jsobj.c		\
	jsopcode.c      \
	jsparse.c	\
	jsregexp.c	\
	jsscan.c	\
	jsscope.c	\
	jsscript.c	\
	jsstr.c		\
	jsxdrapi.c	\
	$(NULL)

LIB_CFILES  = $(PR_CFILES) $(JS_CFILES)
LIB_ASFILES := $(wildcard *_$(OS_ARCH).s)
PROG_CFILES = js.c

ifdef USE_MSVC
LIB_OBJS  = $(addprefix $(OBJDIR)/, $(LIB_CFILES:.c=.obj))
PROG_OBJS = $(addprefix $(OBJDIR)/, $(PROG_CFILES:.c=.obj))
else
LIB_OBJS  = $(addprefix $(OBJDIR)/, $(LIB_CFILES:.c=.o))
LIB_OBJS  += $(addprefix $(OBJDIR)/, $(LIB_ASFILES:.s=.o))
PROG_OBJS = $(addprefix $(OBJDIR)/, $(PROG_CFILES:.c=.o))
endif

CFILES = $(LIB_CFILES) $(PROG_CFILES)
OBJS   = $(LIB_OBJS) $(PROG_OBJS)

ifdef USE_MSVC
LIBRARY = $(OBJDIR)/js32.dll
PROGRAM = $(OBJDIR)/js
else
LIBRARY = $(OBJDIR)/libjs.a
PROGRAM = $(OBJDIR)/js
endif

ifdef USE_MSVC
TARGETS = $(LIBRARY)   # $(PROGRAM) not supported for MSVC yet
else
TARGETS = $(LIBRARY) $(PROGRAM)
endif

all: $(TARGETS)

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
$(LIBRARY): $(LIB_OBJS)
	$(AR) rv $@ $?
	$(RANLIB) $@
endif
endif

#NSPR_LIBRARY = ../../dist/$(OBJDIR)/lib/libnspr21.so
NSPR_LIBRARY =

$(PROGRAM): $(PROG_OBJS) $(LIBRARY)
	$(CC) -o $@ $(CFLAGS) $(PROG_OBJS) $(LIBRARY) $(NSPR_LIBRARY) $(LDFLAGS)

$(PROGRAM).pure: $(PROG_OBJS) $(LIBRARY)
	purify $(PUREFLAGS) \
	    $(CC) -o $@ $(PURE_OS_CFLAGS) $(PROG_OBJS) $(LIBRARY) $(LDFLAGS)

$(HFILES) $(CFILES): $(OBJDIR)/prcpucfg.h

ifdef PREBUILT_CPUCFG
$(OBJDIR)/prcpucfg.h: prcpucfg.h
	cp prcpucfg.h $(OBJDIR)
else
$(OBJDIR)/prcpucfg.h: $(OBJDIR)/prcpucfg
	rm -f $@
	$(OBJDIR)/prcpucfg > $@

$(OBJDIR)/prcpucfg: $(OBJDIR)/prcpucfg.o
	$(CC) -o $@ $(OBJDIR)/prcpucfg.o
endif

#
# Hardwire dependencies on jsopcode.def
#
jsopcode.h jsopcode.c: jsopcode.def

-include $(DEPENDENCIES)

TARNAME = jsref.tar
TARFILES = files `cat files`

tar:
	tar cvf $(TARNAME) $(TARFILES)
	gzip $(TARNAME)

SUFFIXES: .i
%.i: %.c
	$(CC) -C -E $(CFLAGS) $< > $*.i
