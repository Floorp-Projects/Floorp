#! gmake
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

PROGRAM	= mkdepend

include $(DEPTH)/config/config.mk

CSRCS			= 	\
	cppsetup.c		\
	ifparser.c		\
	include.c		\
	main.c			\
	parse.c			\
	pr.c			\
	$(NULL)

CFLAGS += -DINCLUDEDIR=\"/usr/include\" -DOBJSUFFIX=\".$(OBJ_SUFFIX)\" -DNeedVarargsPrototypes $(X11INCLUDES)
MKDEPENDENCIES	= 

include $(DEPTH)/config/rules.mk

program:: $(PROGRAM)

