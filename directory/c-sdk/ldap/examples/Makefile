# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#
#
# UNIX Makefile for Directory SDK examples
#
# SSL examples are not built by default.  Use 'make ssl' to build them.
# NSPR examples are not built by default.  Use 'make nspr' to build them.
#

###############################################################################
# If you are not building on Solaris, you will need to comment out the
# Solaris section and uncomment the appropriate section for your platform.
#
# Chances are you will need to do a little bit of work in order to get the
# examples to compile.  If you are not compiling on Solaris, use the
# Solaris builds as a model for the libraries you need etc.  Chances are 
# they are the most up-to-date
#

# For Solaris (32 bit)
EXTRACFLAGS=
EXTRALDFLAGS=-lsocket -lnsl 
EXTRANSPRLDFLAGS=-mt
SLDAPLIB=ldap50 -lssldap50 -lssl3 -lnss3 
LDAPLIB=ldap50
LDAPPRLIB=prldap50
NSPRLIB=nspr4
CC=cc

# For Solaris (64 bit)
#EXTRACFLAGS=
#EXTRALDFLAGS=-lsocket -lnsl 
#EXTRANSPRLDFLAGS=-mt
#SLDAPLIB=ldap50 -lssldap50 -lssl3 -lnss3 
#LDAPLIB=ldap50
#LDAPPRLIB=prldap50
#NSPRLIB=nspr4
#CC=cc -xarch=v9

# For HP/UX (32 bit)
#EXTRACFLAGS=-Dhpux -D_HPUX_SOURCE -D_REENTRANT -Aa
#EXTRALDFLAGS=-Wl,+s+b
#SLDAPLIB=ldap50 -lssldap50 -lssl3 -lnss3 
#LDAPLIB=ldap50
#LDAPPRLIB=prldap50
#NSPRLIB=nspr4
#CC=cc

# For HP/UX (64 bit)
#EXTRACFLAGS=-DHPUX11 -DIS_64 +e +DA2.0W +DChpux +DS2.0 
#EXTRACFLAGS+=-D_LARGEFILE64_SOURCE -D_PR_HAVE_OFF64_T
#EXTRALDFLAGS=-lpthread +DA2.0W +DS2.0
#SLDAPLIB=ldap50 -lssldap50 -lssl3 -lnss3 
#LDAPLIB=ldap50
#LDAPPRLIB=prldap50
#NSPRLIB=nspr4
#CC=/opt/ansic/bin/cc


# For IRIX
#EXTRACFLAGS=
#EXTRALDFLAGS=
#SLDAPLIB=ldap50 -lssldap50 -lssl3 -lnss3 
#LDAPLIB=ldap50
#LDAPPRLIB=prldap50
#NSPRLIB=nspr4
#CC=cc

# For AIX
#EXTRACFLAGS=
#EXTRALDFLAGS=-brtl
#SLDAPLIB=ldap50 -lssldap50 -lssl3 -lnss3 
#LDAPLIB=ldap50
#LDAPPRLIB=prldap50
#NSPRLIB=nspr4
#CC=cc

# For Digital UNIX 4.0
#EXTRACFLAGS=
#EXTRALDFLAGS=-taso -rpath ../lib
#SLDAPLIB=ldap50 -lssldap50 -lssl3 -lnss3 
#LDAPLIB=ldap50
#LDAPPRLIB=prldap50
#NSPRLIB=nspr4
#CC=cc

# For Linux 2.2
#EXTRACFLAGS=
#EXTRALDFLAGS=-lpthread
#LDAPLIB=ldapssl50
#LDAPLIB=ldap50
#LDAPPRLIB=prldap50
#NSPRLIB=nspr4
#CC=gcc

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

LIBS=-L$(LIBDIR) -l$(LDAPLIB) $(EXTRALDFLAGS)
NSPRLIBS=$(EXTRANSPRLDFLAGS) -L$(NSPRLIBDIR) -l$(NSPRLIB) -l$(LDAPPRLIB)
SLIBS=-L$(LIBDIR) $(NSPRLIBS) -l$(SLDAPLIB) $(EXTRALDFLAGS)
OPTFLAGS=-g
CFLAGS=$(OPTFLAGS) -I$(INCDIR) -I$(NSPRINCDIR) $(EXTRACFLAGS)
NSPRCFLAGS=-I$(NSPRINCDIR)

PROGS=search asearch csearch psearch rdentry getattrs srvrsort modattrs add del compare modrdn ppolicy getfilt crtfilt

SSLPROGS=ssnoauth ssearch

NSPRPROGS=nsprio

ALLPROGS= $(PROGS) $(SSLPROGS) $(NSPRPROGS)

standard:	$(PROGS)

ssl:		$(SSLPROGS)

nspr:		$(NSPRPROGS)

all:		$(ALLPROGS)

purify:		$(PROGS)
		make clean; make CC="purify $(CC)"

search:		search.o
		$(CC) -o search search.o $(LIBS)

search.o:	examples.h

csearch:	csearch.o
		$(CC) -o csearch csearch.o $(LIBS)

csearch.o:	examples.h

psearch:	psearch.o
		$(CC) -o psearch psearch.o $(LIBS)

psearch.o:	examples.h

ssearch:	ssearch.o
		$(CC) -o ssearch ssearch.o $(SLIBS)

ssearch.o:	examples.h

ssnoauth:	ssnoauth.o
		$(CC) -o ssnoauth ssnoauth.o $(SLIBS)

ssnoauth.o:	examples.h

rdentry:	rdentry.o
		$(CC) -o rdentry rdentry.o $(LIBS)

rdentry.o:	examples.h

getattrs:	getattrs.o
		$(CC) -o getattrs getattrs.o $(LIBS)

getattrs.o:	examples.h

srvrsort:	srvrsort.o
		$(CC) -o srvrsort srvrsort.o $(LIBS)

srvrsort.o:	examples.h

modattrs:	modattrs.o
		$(CC) -o modattrs modattrs.o $(LIBS)

modattrs.o:	examples.h

asearch:	asearch.o
		$(CC) -o asearch asearch.o $(LIBS)

asearch.o:	examples.h

add:		add.o
		$(CC) -o add add.o $(LIBS)

add.o:		examples.h

del:		del.o
		$(CC) -o del del.o $(LIBS)

del.o:		examples.h

compare:	compare.o
		$(CC) -o compare compare.o $(LIBS)

compare.o:	examples.h

modrdn:		modrdn.o
		$(CC) -o modrdn modrdn.o $(LIBS)

modrdn.o:	examples.h

ppolicy:	ppolicy.o
		$(CC) -o ppolicy ppolicy.o $(LIBS)

ppolicy.o:	examples.h

getfilt:	getfilt.o
		$(CC) -o getfilt getfilt.o $(LIBS)

getfilt.o:	examples.h

crtfilt:	crtfilt.o
		$(CC) -o crtfilt crtfilt.o $(LIBS)

crtfilt.o:	examples.h

nsprio:		nsprio.o
		$(CC) -o nsprio nsprio.o $(LIBS) $(NSPRLIBS)

nsprio.o:	examples.h

runall:		$(PROGS)
		@for i in $(PROGS); do \
		    echo "-------------------------------------------------"; \
		    echo "Executing $$i example..."; \
		    ./$$i; \
		    echo "$$i example done."; \
		done

clean:
		/bin/rm -f $(ALLPROGS) *.o a.out core
