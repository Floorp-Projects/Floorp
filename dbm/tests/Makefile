DEPTH		= ../..
CORE_DEPTH	= ../..

MODULE		= dbm

CSRCS		= lots.c

PROGRAM		= lots

include $(DEPTH)/coreconf/config.mk

ifeq ($(OS_ARCH),WINNT)
DEFINES		+= -DSTDARG
LIBDBM		= ../src/$(PLATFORM)/dbm$(STATIC_LIB_SUFFIX)
else
LIBDBM		= ../src/$(PLATFORM)/libdbm$(STATIC_LIB_SUFFIX)
endif

ifeq ($(OS_ARCH),AIX)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),BSD_386)
CFLAGS		+= -g -I../../../include -DXP_UNIX -g -DBSDI -DHAVE_STRERROR -D__386BSD__ -DDEBUG -DMEMMOVE -D__DBINTERFACE_PRIVATE 
endif

ifeq ($(OS_ARCH),FreeBSD)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),HP-UX)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),IRIX)
CFLAGS		+= -g -I../../../include -DDEBUG -DSTDARG
endif

ifeq ($(OS_ARCH),OSF1)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),Linux)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),NCR)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),SCO_SV)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),SunOS)
CFLAGS		+= -g -I../../../include -D_sun_
endif

ifeq ($(OS_ARCH),UNIXWARE)
CFLAGS		+= -DSTDARG
endif

INCLUDES	+= -I../include

LDFLAGS		= $(LDOPTS) $(LIBDBM)

include $(DEPTH)/coreconf/rules.mk

lots.pure: lots
	purify $(CC) -o lots.pure $(CFLAGS) $(OBJS) $(MYLIBS)

crash: crash.o $(MYLIBS)
	$(CC) -o crash $(CFLAGS) crash.o $(MYLIBS)

crash.pure: crash.o $(MYLIBS)
	purify $(CC) -o crash.pure $(CFLAGS) crash.o $(MYLIBS)

