#
# Copyright (c) 1996-1998.  Netscape Communications Corporation.  All
# rights reserved.
#
#
# Win32 GNU Makefile for Directory SDK examples
#
# build the examples by typing:  gmake -f win32.mak
# SSL examples are not built by default.  Use 'gmake -f win32.mak ssl'
# NSPR examples are not built by default.  Use 'gmake -f win32.mak nspr'
#


# For Win32 (NT)
LDAPLIB=nsldap32v50
SSLDAPLIB=nsldapssl32v50
PRLDAPLIB=nsldappr32v50
NSPRLIB=libnspr4
EXTRACFLAGS=-nologo -W3 -GT -GX -D_X86_ -Dx386 -DWIN32 -D_WINDOWS -c -Od
EXTRALDFLAGS=/NOLOGO /PDB:NONE /DEBUG /DEBUGTYPE:BOTH /SUBSYSTEM:console /FIXED:NO


###############################################################################
# You should not need to change anything below here....

INTERNAL_LIBLDAP_HEADERS=$(wildcard ../libraries/libldap/*.h)
ifeq (,$(findstring h, $(INTERNAL_LIBLDAP_HEADERS)))
IN_SRC_TREE=0
else
IN_SRC_TREE=1
endif

ifneq ($(IN_SRC_TREE),1)
# we are not in the C SDK source tree... so must be in a binary distribution
INCDIR=../include
LIBDIR=../lib
NSPRINCDIR=../include
NSPRLIBDIR=../lib

else
# we are in the C SDK source tree... paths to headers and libs are different
NS_DEPTH	= ../../..
LDAP_SRC	= ..
NSCP_DISTDIR	= ../../../../dist
NSPR_TREE	= ../..
MOD_DEPTH	= ../..

ifeq ($(HAVE_CCONF), 1)
COMPS_FROM_OBJDIR=1
endif

include $(NSPR_TREE)/config/config.mk

ifeq ($(COMPS_FROM_OBJDIR),1)
NSPR_DISTDIR=$(NSCP_DISTDIR)/$(OBJDIR_NAME)
else
NSPR_DISTDIR=$(NSCP_DISTDIR)
endif

INCDIR=$(NSCP_DISTDIR)/public/ldap
LIBDIR=$(NSCP_DISTDIR)/$(OBJDIR_NAME)/lib
NSPRINCDIR=$(NSPR_DISTDIR)/include
NSPRLIBDIR=$(NSPR_DISTDIR)/lib
endif

SYSLIBS=wsock32.lib kernel32.lib user32.lib shell32.lib
LIBS=$(LIBDIR)/$(LDAPLIB).lib $(LIBDIR)/$(SSLDAPLIB).lib $(LIBDIR)/$(PRLDAPLIB).lib $(NSPRLIBDIR)/$(NSPRLIB).lib 

CC=cl
OPTFLAGS=-MD
CFLAGS=$(OPTFLAGS) $(EXTRACFLAGS) -I$(INCDIR) -I$(NSPRINCDIR)
LINK=link
LDFLAGS=$(EXTRALDFLAGS)


PROGS=search asearch csearch psearch rdentry getattrs srvrsort modattrs add del compare modrdn ppolicy getfilt crtfilt
EXES=$(addsuffix .exe,$(PROGS))

SSLPROGS=ssnoauth ssearch
SSLEXES=$(addsuffix .exe,$(SSLPROGS))

NSPRPROGS=nsprio
NSPREXES=$(addsuffix .exe,$(NSPRPROGS))

ALLEXES= $(EXES) $(SSLEXES) $(NSPREXES)

standard:	$(EXES)

ssl:		$(SSLEXES)

nspr:		$(NSPREXES)

all:		$(ALLEXES)

search.obj:	examples.h

csearch.obj:	examples.h

psearch.obj:	examples.h

ssearch.obj:	examples.h

ssnoauth.obj:	examples.h

rdentry.obj:	examples.h

getattrs.obj:	examples.h

srvrsort.obj:	examples.h

modattrs.obj:	examples.h

asearch.obj:	examples.h

add.obj:	examples.h

del.obj:	examples.h

compare.obj:	examples.h

modrdn.obj:	examples.h

ppolicy.obj:	examples.h

getfilt.obj:	examples.h

crtfilt.obj:	examples.h

nsprio.obj:	examples.h

runall:		$(EXES)
		@for i in $(PROGS); do \
		    echo '-------------------------------------------------'; \
		    ./$$i; \
		done

clean:
		rm -f $(ALLEXES) *.obj

%.obj : %.c
		$(CC) $(CFLAGS) $< -Fo$@


%.exe : %.obj
		$(LINK) -OUT:$@ $(LDFLAGS) $(SYSLIBS) $< $(LIBS)
