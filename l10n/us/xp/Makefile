# -*- Mode: Makefile -*-
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

DEPTH		= ../../..
REAL_XPDIR	= $(DEPTH)/lib/xp

HSRCS		= $(wildcard *.html)
MSRCS		= $(wildcard *.msg)

RM		= /bin/rm -f

#
# tr has a bug on linux.
#	tr '[\001-\272]' '[\106-\377]'
# does not translate char ']' So we use our own program to do this
#
TR69		= $(OBJDIR)/tr69

TARGETS		= $(TR69) $(HSRCS:.html=.h) $(MSRCS:.msg=.h) splash.h splash-java.h \
		  about-lite.h about-java-lite.h splash-lite.h splash-java-lite.h

include $(DEPTH)/config/rules.mk

GARBAGE		+= about-lite.html about-java-lite.html $(wildcard splash*.html) \
		   $(wildcard *.pc) $(wildcard *.input) $(wildcard *.h) $(wildcard *~)

.SUFFIXES:	.html .msg

$(TR69):	tr69.c
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),OS2)
	$(CCF) -Fe $@ $<
else
	$(CC) -o $@ $<
endif

#
# Welcome to sHELL....
#
# The awk command before the tee is for Windows, the one after the
# tee is for Unix only.  The about.pc file is for Win16, and the
# about-java.pc is for Win32.  They become .rc files with a trivial
# tweak, if necessary.
#
# Mac:    default
# Win32:  no Mercutio MDEF
# Win16:  no Mercutio MDEF
# Unix:   no Mercutio MDEF, no Macromedia, no Symantec, no Quicktime
#
about.h:	about-all.html
	@echo generating $@ from $<;					\
	NAME=Netscape;							\
	CLASS=Netscape;							\
	RTMP=/tmp/nsrot$$$$;						\
	$(RM) $@ $*.pc;							\
	sed    "s/@NAME@/$$NAME/g;					\
		s/@CLASS@/$$CLASS/g;					\
		s/@PROGNAME@/$(PROGNAME)/g" $< |			\
	awk    'BEGIN { skipline = 0 }					\
		/Mercutio/ { next } { print }' |			\
	tee $$RTMP |							\
	awk    'BEGIN { skipline = 0 }					\
		/about:qtlogo/,/<\/TD>/ { skipline = 1; next }		\
		/about:mmlogo/,/<\/TD>/ { skipline = 1; next }		\
		/about:symlogo/,/<\/TD>/ { skipline = 1; next }		\
		skipline == 1 && $$1 !~ /<\/TD>/ { next }		\
		skipline == 1 && $$1 ~ /<\/TD>/ { skipline = 0; next }	\
		{ print }' |						\
	tee splash.input |						\
	$(TR69) '[\001-\272]' '[\106-\377]' |				\
	od -b |								\
	sed    's/^[0-7][0-7]* *\(.*\)/\\\1/;				\
		s/ /\\/g;						\
		s/\(.*\)/ "\1"/;					\
		s/^ *"\\"$$//' >> $@;					\
	echo generating $*.pc from $<;					\
	$(EMACS) -batch -q -l $(REAL_XPDIR)/html-to-pc.el		\
		-f batch-html-to-pc $* $$RTMP $*.pc;			\
	echo;								\
	$(RM) $$RTMP

about-java.h:	about-all.html
	@echo generating $@ from $<;					\
	NAME=Netscape;							\
	CLASS=Netscape;							\
	RTMP=/tmp/nsrot$$$$;						\
	$(RM) $@ $*.pc;							\
	sed    "s/@NAME@/$$NAME/g;					\
		s/@CLASS@/$$CLASS/g;					\
		s/@PROGNAME@/$(PROGNAME)/g" $< |			\
	awk    'BEGIN { skipline = 0 }					\
		/Mercutio/ { next } { print }' |			\
	tee $$RTMP |							\
	awk    'BEGIN { skipline = 0 }					\
		/about:qtlogo/,/<\/TD>/ { skipline = 1; next }		\
		/about:mmlogo/,/<\/TD>/ { skipline = 1; next }		\
		/about:symlogo/,/<\/TD>/ { skipline = 1; next }		\
		skipline == 1 && $$1 !~ /<\/TD>/ { next }		\
		skipline == 1 && $$1 ~ /<\/TD>/ { skipline = 0; next }	\
		{ print }' |						\
	tee splash-java.input |						\
	$(TR69) '[\001-\272]' '[\106-\377]' |				\
	od -b |								\
	sed    's/^[0-7][0-7]* *\(.*\)/\\\1/;				\
		s/ /\\/g;						\
		s/\(.*\)/ "\1"/;					\
		s/^ *"\\"$$//' >> $@;					\
	echo generating $*.pc from $<;					\
	$(EMACS) -batch -q -l $(REAL_XPDIR)/html-to-pc.el		\
		-f batch-html-to-pc $* $$RTMP $*.pc;			\
	echo;								\
	$(RM) $$RTMP

about-all.h:	about-all.html
	@echo "Garbage file" > $@

splash.input:		about.h
splash-java.input:	about-java.h

splash.html:		splash.input
	@echo generating $@ from $<...
	@sed 's|<A [Hh]...="[^>]*">||g;s|</A>||g' $< > $@

splash-java.html:	splash-java.input
	@echo generating $@ from $<...
	@sed 's|<A [Hh]...="[^>]*">||g;s|</A>||g' $< > $@

splash-lite.html:	about-lite.html
	@echo generating $@ from $<...
	@sed 's|<A [Hh]...="[^>]*">||g;s|</A>||g' $< > $@

splash-java-lite.html:	about-java-lite.html
	@echo generating $@ from $<...
	@sed 's|<A [Hh]...="[^>]*">||g;s|</A>||g' $< > $@

about-lite.html:	splash.input
	@echo generating $@ from $<...
	@sed 's/Communicator/Navigator/' $< > $@

about-java-lite.html:	splash-java.input
	@echo generating $@ from $<...
	@sed 's/Communicator/Navigator/' $< > $@

#
# Handle the rest of the HTML files.
#
.html.h:
	@echo generating $*.h from $<;					\
	NAME=Netscape;							\
	CLASS=Netscape;							\
	RTMP=/tmp/nsrot$$$$;						\
	$(RM) $*.h $*.pc;						\
	sed    "s/@NAME@/$$NAME/g;					\
		s/@CLASS@/$$CLASS/g;					\
		s/@PROGNAME@/$(PROGNAME)/g"				\
		< $< > $$RTMP;						\
	$(TR69) < $$RTMP |						\
	od -b |								\
	sed    's/^[0-7][0-7]* *\(.*\)/\\\1/;				\
		s/ /\\/g;						\
		s/\(.*\)/ "\1"/;					\
		s/^ *"\\"$$//' >> $*.h;					\
	echo generating $*.pc from $<;					\
	$(EMACS) -batch -q -l $(REAL_XPDIR)/html-to-pc.el		\
		-f batch-html-to-pc $* $$RTMP $*.pc;			\
	echo;								\
	$(RM) $$RTMP

.msg.h:
	@echo generating $*.h from $<;					\
	NAME=Netscape;							\
	CLASS=Netscape;							\
	$(RM) $*.h;							\
	RTMP=/tmp/nsrot$$$$;						\
	sed    "s/@NAME@/$$NAME/g;					\
		s/@CLASS@/$$CLASS/g;					\
		s/@PROGNAME@/$(PROGNAME)/g"				\
		< $< > $$RTMP;						\
	$(TR69) '[\001-\272]' '[\106-\377]' < $$RTMP |			\
	od -b | sed 's/^[0-7][0-7]* *\(.*\)/\\\1/;			\
		s/ /\\/g;s/\(.*\)/ "\1"/;s/^ *"\\"$$//'			\
	  >> $*.h;							\
	$(RM) $$RTMP

$(TARGETS):	Makefile

export::	$(TARGETS)
