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

DEPTH=../../../..

include $(DEPTH)/config/config.mk

LIBRARY_NAME = raptorhtmlforms_s

DEFINES += -D_IMPL_NS_HTML

# Note the sophisticated alphabetical ordering :-|
CPPSRCS =				\
	nsForm.cpp			\
	nsInput.cpp			\
	nsInputButton.cpp		\
	nsInputCheckbox.cpp		\
	nsInputFile.cpp			\
	nsInputFrame.cpp		\
	nsInputImage.cpp		\
	nsInputRadio.cpp		\
	nsInputText.cpp			\
	nsSelect.cpp            \
	$(NULL)

MODULE = raptor

INCLUDES += -I../../../base/src -I../../base/src -I../../style/src

REQUIRES = xpcom raptor dom

include $(DEPTH)/config/rules.mk

