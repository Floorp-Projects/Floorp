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


#
# The following source code is part of the Microline Widget Library.
# The Microline widget library is made available to Mozilla developers
# under the Netscape Public License (NPL) by Neuron Data.  To learn
# more about Neuron Data, please visit the Neuron Data Home Page at
# http://www.neurondata.com.
#


.SUFFIXES:	.c .o .uil .uid

E1 = demo folder1 folder2 folder3 folder4
E2 = grid1 grid2 grid3 grid4 grid5 grid6
E3 = tree1 tree2 tree3 tree4 prog1 prog2 prog3
E4 = util1

ALLTARGETS = $(E1) $(E2) $(E3) $(E4)
# add 'uil1.uid' and 'uil1' to E4 line above to make uil example

DEPTH = ../../../..

TARGETS = $(addprefix $(OBJDIR)/, $(ALLTARGETS))

include $(DEPTH)/config/rules.mk

INCLUDES += -I..
LDFLAGS += -L../XmL/$(OBJDIR)

LDLIBS = -lXmL -lXm -lXt -lX11

$(OBJDIR)/%: %.c
	@$(MAKE_OBJDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

.uil.uid :
	uil $*.uil -o $*.uid
