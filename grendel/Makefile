#!gmake
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License.  You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
# the License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Grendel mail/news client.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation.  Portions created by Netscape are Copyright (C) 1997
# Netscape Communications Corporation.  All Rights Reserved.

SUBDIRS=		\
	addressbook	\
	calypso		\
	composition	\
	dnd		\
	filters		\
	integrator	\
	mime		\
	prefs		\
	search		\
	storage		\
	ui		\
	util		\
	view		\
	widgets		\
	$(NULL)

SRCS=				\
	Main.java		\
	TestFolderViewer.java	\
	$(NULL)

#	SelfTest.java		\


include rules.mk

TARFILE=/tmp/grendel.tar.gz
tar::
	@echo writing $(TARFILE)... ;			\
	dir=`pwd | sed 's@^.*/\([^/]*\)$$@\1@'` ;	\
	cd .. ; tar -vcf - $$dir			\
		--exclude '*.class'			\
		--exclude '*.bak'			\
		--exclude '*~'				\
		--exclude 'core'			\
		--exclude '*.orig'			\
	| gzip -v9 > $(TARFILE)
