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


#
# The following source code is part of the Microline Widget Library.
# The Microline widget library is made available to Mozilla developers
# under the Netscape Public License (NPL) by Neuron Data.  To learn
# more about Neuron Data, please visit the Neuron Data Home Page at
# http://www.neurondata.com.
#

##########################################################################
#
# Name:			Makefile
# Description:	Makefile for Microline Widget library
# Author:		Ramiro Estrugo <ramiro@netscape.com>
#
##########################################################################

DEPTH			= ../../../..

# XmL headers are exported to dist/public/Microline/XmL
MODULE			= Microline/XmL

LIBRARY_NAME	= XmL

# There are unused widgets.  They are currently not needed to build Mozilla,
# but that might change in the future.
ifdef XFE_WIDGETS_BUILD_UNUSED

XFE_EXTRA_DEFINES += -DXFE_WIDGETS_BUILD_UNUSED

UNUSED_CSRCS	= \
				Progress.c \
				$(NULL)

UNUSED_EXPORTS	= \
				Progress.h \
				ProgressP.h \
				$(NULL)
endif

CSRCS			= \
				$(UNUSED_CSRCS) \
				Folder.c \
				Grid.c \
				Tree.c \
				XmL.c \
				$(NULL)

REQUIRES		= Microline

EXPORTS			= \
				$(UNUSED_EXPORTS) \
				Folder.h \
				FolderP.h \
				Grid.h \
				GridP.h \
				Tree.h \
				TreeP.h \
				XmL.h \
				$(NULL)

include $(DEPTH)/config/rules.mk

DEFINES		+= $(XFE_EXTRA_DEFINES)
