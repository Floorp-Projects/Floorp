DEPTH = ../..

include $(DEPTH)/config/config.mk

CSRCS = \
	alloc.c \
	buckets.c \
	divtest.c \
	hashtable.cpp	\
	maltest.c \
	pagetest.c \
	simpletest.c \
	smtest.c

PROGS = $(addprefix $(OBJDIR)/, $(CSRCS:.c=$(PROG_SUFFIX)))

TARGETS = $(PROGS)

REQUIRES = sm

LDFLAGS += -L$(DIST)/lib

LIBRARIES= sm

include $(DEPTH)/config/rules.mk

LDFLAGS += $(NSPR_LIBS)
