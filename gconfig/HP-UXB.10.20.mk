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

#
# On HP-UX 10.10 and 10.20, the default implementation strategy is
# pthreads (actually DCE threads).  Classic nspr is also available.
#

ifeq ($(OS_RELEASE),B.10.20)
	DEFAULT_IMPL_STRATEGY = _PTH
endif

#
# Config stuff for HP-UXB.10.20
#
include $(GDEPTH)/gconfig/HP-UXB.10.mk

OS_CFLAGS		+= -DHPUX10_20

ifeq ($(USE_PTHREADS),1)
	OS_CFLAGS	+= -D_PR_DCETHREADS
endif
