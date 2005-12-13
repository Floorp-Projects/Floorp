###############################################################################
# $Id: Makefile,v 1.5 2005/12/13 13:25:32 gerv%gerv.net Exp $
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Netscape DS Plugins. 
#
# The Initial Developer of the Original Code is Leif Hedstrom and Netscape
# Communications Corp. 
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

###############################################################################
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
