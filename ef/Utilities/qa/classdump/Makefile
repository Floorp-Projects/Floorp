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

#Unix Makefile for classdump

#Variables.
CC = gcc
CFLAGS = -g
SRCS = classdump.c
PROGRAM = classdump
OBJS = classdump.o

#Rules.
$(PROGRAM): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

classdump.o: classdump.c classfile.h classdump.h
	$(CC) -O -c classdump.c
