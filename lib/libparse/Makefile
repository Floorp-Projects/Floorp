#!gmake
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

DEPTH		= ../..

MODULE		= parse
LIBRARY_NAME	= parse

CSRCS		= pa_amp.c pa_hash.c pa_hook.c pa_mdl.c pa_parse.c

EXPORTS		= pa_parse.h pa_tags.h pa_amp.h

REQUIRES	= nspr dbm img util layer js hook lay style

include $(DEPTH)/config/rules.mk

ifndef NO_MOCHA
DEFINES		+= -DMOCHA
endif

# XXX this should actually be "pa_hash.c pa_hash.rmap:" and be in the GARBAGE
# XXX list and so on, but we don't have perl everywhere yet...
#
# XXX also, should share string constants between pa_hash.c and pa_hash.rmap

hash: pa_tags.h gentab.pl pa_hash.template
	@echo Generating pa_hash.c from pa_tags.h and pa_hash.template
	@rm -f pa_hash.c
	@./gentab.pl > pa_hash.c
