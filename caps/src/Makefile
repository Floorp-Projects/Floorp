#!gmake
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

DEPTH	= ../..

include $(DEPTH)/config/config.mk

MODULE		= caps

LIBRARY_NAME	= caps

CPPSRCS		=	        \
                nsUserDialogHelper.cpp            \
		nsZig.cpp                         \
		nsPrincipal.cpp                   \
		nsPrivilege.cpp                   \
		nsPrivilegeManager.cpp            \
		nsPrivilegeTable.cpp              \
		nsSystemPrivilegeTable.cpp        \
		nsTarget.cpp                      \
		nsUserTarget.cpp                  \
		admin.cpp                         \
		nsCaps.cpp                        \
		nsCCapsManager.cpp		  \
		nsCCertPrincipal.cpp		  \
		nsCCodebasePrincipal.cpp	  \
		nsCCapsManagerFactory.cpp	  \
		nsCCodeSourcePrincipal.cpp        \
		$(NULL)

REQUIRES	= nspr xpcom security layer js jar zlib pref img util rdf caps 

CSRCS		=	        \
		nsZip.c                         \
		nsLoadZig.c                     \
		jpermission.c                   \
		$(NULL)

include $(DEPTH)/config/rules.mk

INCLUDES	+=	-I$(DEPTH)/include \
		 	-I$(PUBLIC)/public \
			$(NULL)

