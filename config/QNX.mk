######################################################################
# Config stuff for QNX.
######################################################################

######################################################################
# Version-independent
######################################################################

ARCH			:= qnx
CPU_ARCH		:= x86
GFX_ARCH		:= x

ifdef NS_USE_NATIVE
CC			= cc
CCC			= 
endif
BSDECHO			= echo
EMACS			= true
RANLIB			= true

OS_INCLUDES		= -I/usr/X11/include
G++INCLUDES		=
LOC_LIB_DIR		= 
MOTIF			=
MOTIFLIB		= -lXm
OS_LIBS			=

PLATFORM_FLAGS		= -DQNX -Di386
MOVEMAIL_FLAGS		= -DHAVE_STRERROR
PORT_FLAGS		= -DSW_THREADS -DHAVE_STDDEF_H -DHAVE_STDLIB_H -DNO_CDEFS_H -DNO_LONG_LONG -DNO_ALLOCA -DNO_REGEX -DNO_REGCOMP -DHAS_PGNO_T -DNEED_REALPATH
PDJAVA_FLAGS		=

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS) $(MOVEMAIL_FLAGS)

######################################################################
# Version-specific stuff
######################################################################

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

######################################################################
# Other
######################################################################

ifdef SERVER_BUILD
STATIC_JAVA		= yes
endif
