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

DEPTH			= ../..

MODULE			= net
LIBRARY_NAME		= net

CSRCS			= cvactive.c \
			  cvcolor.c \
			  cvdisk.c \
			  cvextcon.c \
			  cvproxy.c \
			  cvsimple.c \
			  cvview.c \
			  cvchunk.c \
			  extcache.c \
			  mkaccess.c \
			  mkdaturl.c \
			  mkformat.c \
			  mkfsort.c \
			  mkgopher.c \
			  mkhelp.c \
			  mkinit.c \
			  mkjscfg.c \
			  mkmocha.c \
			  mkparse.c \
			  mkremote.c \
			  mkselect.c \
			  mksockrw.c \
			  mksort.c \
			  mkstream.c \
			  mktrace.c \
			  mkpadpac.c \
			  txview.c \
			  jscookie.c \
			  cvmime.c \
			  cvunzip.c \
			  mkautocf.c \
			  mkcache.c \
			  mkconect.c \
			  mkextcac.c \
			  mkfile.c \
			  mkftp.c \
			  mkgeturl.c \
			  mkhttp.c \
			  mkmemcac.c \
			  mkmessag.c \
			  mkutils.c \
			  prefetch.c \
			  $(NULL)

CPPSRCS			= mkmarimb.cpp

REQUIRES		= netcast parse js dbm nspr security htmldlgs ldap softupdt \
			  img util jtools layer pref java libfont lay style applet \
			  zlib li msg progress

EXPORTS			= extcache.h \
			  mkutils.h \
			  mkselect.h \
			  mkaccess.h \
			  mkautocf.h \
			  mkcache.h \
			  mkformat.h \
			  mkgeturl.h \
			  mkhelp.h \
			  mkstream.h \
			  mktcp.h \
			  mktrace.h \
			  mkpadpac.h \
			  mkmarimb.h \
			  mkparse.h \
			  $(NULL)

include $(DEPTH)/config/config.mk

ifdef MOZ_MAIL_NEWS
CSRCS			+= \
			  mkcertld.c \
			  imap4url.c \
			  mkmailbx.c \
			  mknews.c \
			  mknewsgr.c \
			  mkpop3.c \
			  mksmtp.c \
			  prefetch.c \
			  $(NULL)

CPPSRCS			+= imaphier.cpp \
			  imapearl.cpp \
			  mkimap4.cpp \
			  mkldap.cpp \
			  imappars.cpp \
			  imapbody.cpp \
			  mkabook.cpp \
			  $(NULL)
endif

ifndef NO_UNIX_ASYNC_DNS
CSRCS			+= unix-dns.c
endif

include $(DEPTH)/config/rules.mk

DEFINES			+= -DNEW_DECODERS

ifndef NO_UNIX_ASYNC_DNS
DEFINES			+= -DUNIX_ASYNC_DNS
endif

ifndef NO_UNIX_LDAP
INCLUDES		+= -I$(DIST)/include
DEFINES			+= -DLIBNET_LDAP
endif

#
# This converts mime.types to C source code,
#  and also to an HTML table that shows what's in it.
#
MIME_TYPES		= ./mime.types
MIME_TYPES_UNIX		= ./mime.types-unix
MIME_TYPES_NONUNIX	= ./mime.types-nonunix
AD2C			= sh $(DEPTH)/cmd/xfe/ad2c

#
# Workaround for a SCO compiler bug.
#
# If mkutils.c is compiled optimized, password authorization
# always fails.  Probably due to NET_UUEncode routine.
#
# We could try using GCC 2.7.x instead, since that is the first
# version that can produce SCO ELF binaries.
#
ifeq ($(OS_ARCH),SCOOS)
SCO_OPT_FIX		= -O0
endif

$(OBJDIR)/mkformat.o: mkformat.c mktypes.h

$(LIBRARY): $(OBJS)

$(OBJDIR)/mkutils.o $(OBJDIR)/LITE_mkutils.o: mkutils.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c $(CFLAGS) $(SCO_OPT_FIX) $<


mktypes.h: $(MIME_TYPES) $(MIME_TYPES_UNIX) $(MIME_TYPES_NONUNIX)
	rm -f $@
	@echo "generating $@ from $<..."			; \
	 (echo '/* Generated file - do not edit! */'		; \
	  echo ''						; \
	  cat $(MIME_TYPES) | grep -v '^#' | $(AD2C)		; \
	  echo ''						; \
	  echo '#ifdef XP_UNIX'					; \
	  cat $(MIME_TYPES_UNIX) | grep -v '^#' | $(AD2C)	; \
	  echo '#else  /* !XP_UNIX */'				; \
	  cat $(MIME_TYPES_NONUNIX) | grep -v '^#' | $(AD2C)	; \
	  echo '#endif /* !XP_UNIX */'				; \
	 ) > $@

mime-types.html: $(MIME_TYPES) $(MIME_TYPES_UNIX)
	rm -f $@
	@echo "generating $@ from $<..."				; \
	 (echo '<TITLE>Netscape MIME Types</TITLE>'			; \
	  echo '<H1 ALIGN=CENTER>Netscape MIME Types</H1>'		; \
	  echo '<CENTER>'						; \
	  echo '<TABLE BORDER CELLSPACING=0 CELLPADDING=5>'		; \
	  echo '<TR><TH></TH><TH>Content Type:</TH>'			; \
	  echo '<TH>Extensions:</TH><TH>Description:</TH></TR>'		; \
	  echo '<TR></TR><TR></TR><TR></TR>'	; \
	  cat $(MIME_TYPES) $(MIME_TYPES_UNIX) | grep -v '^#' |		   \
	  sed  's@exts="\([^"]*\)"[ 	]*[\]*@<TR><TD><TT>\1</TT></TD>@;  \
		s@desc="\([^"]*\)"[ 	]*[\]*@<TD>\1</TD>@;		   \
		s@enc=\([^ 	]*\)[ 	]*[\]*$$@<TD>encoding/\1</TD>@;	   \
		s@type=\([^ 	]*\)[ 	]*[\]*$$@<TD>\1</TD>@;		   \
		s@icon=\([^ 	]*\)[ 	]*[\]*$$@<TD><IMG SRC=\1></TD></TR>@'\
	  | sed 's/,/ /g'						  \
	  | awk '{if ( $$0 ~ /^<TR/ ) {printf "%s" , $$0}		  \
		  else {printf "%s\n" , $$0} }'				  \
	  | sed 's@\(<TR>\)\(<TD>.*</TD>\)\(<TD>.*</TD>\)\(<TD>.*</TD>\)\(<TD>.*</TD>\)\(</TR>\)$$@\1\5!\3!\2!\4\6@' \
	  | tr '!' '\012'						  \
	  | sed 's@<TD></TD>@<TD>\&nbsp;</TD>@'				; \
	  echo '</TABLE>'						; \
	  echo '</CENTER>'						; \
	 ) > $@

