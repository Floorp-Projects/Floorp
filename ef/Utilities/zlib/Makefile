#!gmake
#
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

DEPTH = ../..

MODULE_NAME = EF

CSRCS = 	adler32.c			\
		compress.c			\
		crc32.c				\
		deflate.c			\
		gzio.c				\
		infblock.c			\
		infcodes.c			\
		inffast.c			\
		inflate.c			\
		inftrees.c			\
		infutil.c			\
		trees.c				\
		uncompr.c			\
		zutil.c				\
		$(NULL)

LOCAL_EXPORTS = deflate.h			\
		infblock.h			\
		infcodes.h			\
		inffast.h			\
		inftrees.h			\
		infutil.h			\
		zconf.h				\
		zlib.h				\
		zutil.h				\
		$(NULL)


include $(DEPTH)/config/config.mk

include $(DEPTH)/config/rules.mk
