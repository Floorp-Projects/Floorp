###############################################################################
# $Id: Makefile,v 1.3 2000/01/14 22:25:14 leif%netscape.com Exp $
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is Netscape DS Plugins. The Initial Developer of the
# Original Code is Leif Hedstrom and Netscape Communications Corp.
# Portions created by Netscape are Copyright (C) Netscape Communications
# Corp. All Rights Reserved.
#
# Contributor(s):
#
#
# DESCRIPTION:
#    Simple Makefile for the DS-3 (and later) plugins.
#
###############################################################################


###############################################################################
# Build configuration is done here!
#
#DS		= /opt/suitespot/plugins/slapd/slapi
DS		= /server/server4/plugins/slapd/slapi

ETAGS		= etags
#ETAGS		= true


###############################################################################
# Common configurations
#
CC		= gcc
SH_OPTS		= -fpic


###############################################################################
# Configuration specific for solaris-2.x.
#
CC		= gcc
SH_OPTS		= -fpic

#CC		= cc
#SH_OPTS	= -Kpic

LD_OPTS		= -G
FLAGS		= -D_REENTRANT

OPT		= -O6 -mcpu=ultrasparc -Wall -DNDEBUG
#OPT		= -O6 -Wall -DNDEBUG
OPT		= -g -Wall -DDEBUG


###############################################################################
# You shouldn't have to touch anything below here.
#
INCLUDE		= -I. -I$(DS)/include
CFLAGS		= $(FLAGS) $(OPT) $(INCLUDE) $(SH_OPTS)
LDFLAGS		= $(LD_OPTS)

TARGETS		= rex_filt.so url_filt.so
INCFILES	= lulu.h
CFILES		= rex_filt.c url_filt.c lulu.c
MISCFILES	= Makefile


###############################################################################
# Main targets.
#
all:		$(TARGETS) TAGS

#mod_dn.so:	mod_dn.o lulu.o lulu.h
#	$(LD) $(LDFLAGS) -o $@ mod_dn.o lulu.o

rex_filt.so:	rex_filt.o lulu.o lulu.h
	$(LD) $(LDFLAGS) -o $@ rex_filt.o lulu.o

url_filt.so:	url_filt.o lulu.o lulu.h
	$(LD) $(LDFLAGS) -o $@ url_filt.o lulu.o


###############################################################################
# Misc targets
#
TAGS:	$(CFILES) $(INCFILES) Makefile
	$(ETAGS) $(CFILES) $(INCFILES)
	@touch TAGS


###############################################################################
# "clean" and distribution targets.
#
realclean: 	distclean
	rm -f $(TARGETS)

distclean:
	rm -f *.o *.so core *.~ *~ TAGS
#	rm -f config.cache config.log config.status config.h
#	rm -rf .stamp-h.in .stamp-h

mostlyclean:
	rm -f *.o $(TARGETS) core *.~ *~

clean:
	rm -f *.o core *.~ *~
