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

MODULE		= i18n
LIBRARY_NAME	= i18n

DIRS		= unicode

CSRCS		= autokr.c \
		  autocvt.c \
		  b52cns.c \
		  cns2b5.c \
		  cvchcode.c \
		  detectu2.c \
		  euc2jis.c \
		  euc2sjis.c \
		  euckr2is.c \
		  is2euckr.c \
		  intl_csi.c \
		  jis2oth.c \
		  net_junk.c \
		  nscstr.c \
		  metatag.c \
		  sjis2euc.c \
		  sjis2jis.c \
		  ugen.c \
		  ugendata.c \
		  ucs2.c \
		  umap.c \
		  unicvt.c \
		  uscan.c \
		  fontencd.c \
		  csnamefn.c \
		  csnametb.c \
		  sbconvtb.c \
		  acptlang.c \
		  csstrlen.c \
		  sblower.c \
		  dblower.c \
		  kinsokud.c \
		  kinsokuf.c \
		  katakana.c \
		  $(NULL)

REQUIRES	= i18n dbm nspr img util layer pref js

CSRCS		+= doc_ccc.c fe_ccc.c intlcomp.c mime2fun.c

include $(DEPTH)/config/rules.mk

DEFINES		+= -DHAS_FE

$(LIBRARY): $(OBJS)

