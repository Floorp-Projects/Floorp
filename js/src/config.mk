ifdef JS_DIST
DIST = $(JS_DIST)
else
DIST = $(DEPTH)/../../dist/$(OBJDIR)
endif

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
ifeq ($(OS_ARCH),dgux)
OS_CONFIG      := dgux
else
OS_CONFIG       := $(OS_ARCH)$(OS_OBJTYPE)$(OS_RELEASE)
endif
endif

ASFLAGS         =
DEFINES         =

ifeq ($(OS_ARCH), WINNT)
INSTALL = nsinstall
else
INSTALL	= $(DEPTH)/../../dist/$(OBJDIR)/bin/nsinstall
endif

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

include $(DEPTH)/config/$(OS_CONFIG).mk

# Name of the binary code directories
OBJDIR          = $(OS_CONFIG)$(OBJDIR_TAG).OBJ
VPATH           = $(OBJDIR)

# Automatic make dependencies file
DEPENDENCIES    = $(OBJDIR)/.md

