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



INCLUDES = -I../..
BIN_SUFFIX = .exe
ifneq ($(subst /,_,$(shell uname -s)),OS2)
.c.o:
	cc -c $(CFLAGS)  $(INCLUDES) $*.c
else
.c.o:
	icc -c -Fo$*.o $(CFLAGS) $(INCLUDES) $*.c
	
endif

all:	fromu$(BIN_SUFFIX) tou$(BIN_SUFFIX) fromu0208$(BIN_SUFFIX) \
		tou0208$(BIN_SUFFIX) xlatgen$(BIN_SUFFIX) xlatgenmac$(BIN_SUFFIX)

xlatgenmac$(BIN_SUFFIX):	xlatgenmac.o xlatgenutil.o
	ILINK -NOFREE -PMTYPE:VIO $?;

xlatgen$(BIN_SUFFIX):	xlatgen.o 
	ILINK -NOFREE -PMTYPE:VIO $?;

fromu$(BIN_SUFFIX):	fromu.o utblutil.o
	ILINK -NOFREE -PMTYPE:VIO $?;

tou$(BIN_SUFFIX):	tou.o utblutil.o
	ILINK -NOFREE -PMTYPE:VIO $?;

fromu0208$(BIN_SUFFIX):	fromu0208.o utblutil.o
	ILINK -NOFREE -PMTYPE:VIO $?;

tou0208$(BIN_SUFFIX):	tou0208.o utblutil.o
	ILINK -NOFREE -PMTYPE:VIO $?;

utblutil.o$(BIN_SUFFIX): ../../umap.h
	ILINK -NOFREE -PMTYPE:VIO $?;

