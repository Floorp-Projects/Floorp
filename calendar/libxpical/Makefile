# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is OEone Calendar Code, released October 31st, 2001.
#
# The Initial Developer of the Original Code is
# OEone Corporation.
# Portions created by OEone Corporation are Copyright (C) 2001
# OEone Corporation. All Rights Reserved.
#
# Contributor(s): Steve Crouse <stevec@oeone.com>
#                 Garth Smedley <garths@oeone.com>
#                 Jody McIntyre <jodym@oeone.com>
#                 Mostafa Hosseini <mostafah@oeone.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#
# Generic XPCOM component makefile
#
# Authors: Steve Crouse, Garth Smedley, Jody McIntyre
#--------------------------------------------------------
# Usage:
#
#  make clean     - remove output files, does not install
#  make           - build the component and the test program, if not up to date
#  make reset     - delete registration files from mozilla. good idea to run this after install
#  make install   - install xpcom component into mozilla components directory
#  make uninstall - remove xpcom component from mozilla components directory
#  make dist      - tars the distribution files up.
#  make test      - run the test program
#
#  By setting the following environment variable before invoking make, you can control the directories
#  that install and uninstall use:
#
#     DESTDIR               - install/uninstall directory, ( Do not add a trailing slash )
#
#  This Makefile can be customized for use by any XPCOM component
#  by modifying some of the variables below.
#
#     MOZZY                 - Provide the path to your Mozilla installation
#     MOZZYDEV              - Provide the path to your Mozilla development installation
#     LIBRARY_NAME          - Name of the final shared object to be built
#     IDLFILE               - Components IDL file
#     IDL_OUTPUT_BASENAME   - Base name of the .xpt and .h files to be generated
#     SOURCE_FILES          - Source files needed to build component
#     HEADER_FILES          - Header files needed to build component, used for dependencies and distribution
#     EXTRA_DISTFILES       - extra files to be added to the tarball
#     TEST_PROGRAM          - Name of standalone test program
#     TEST_SOURCE_FILES     - Source files needed to build test program
#
#  You can also specify additional include paths and libraries to be linked
#  in using:
#
#     INCLUDES              - Specify any additional include paths here
#     EXTRA_LIBS            - Specify any additional libraries to be linked
#

CC = g++

# Path to src directory
#

SRC = .

# set PACKAGE and VERSION for distrbution tarball
PACKAGE=xpcomical
VERSION=0.1

# Distribution files
# Add any extra files to go in the tarball, by default .so, .idl, .xpt and Makefile are included.

EXTRA_DISTFILES= caltst.xul caltst.js

# Path to your Mozilla installation
#
MOZZY = /usr/local/mozilla0.9.5
MOZZYDEV = /usr/include/mozilla0.9.5

# Define the name for the final shared object file
#
LIBRARY_NAME = libxpical.so

# Define the name of the executable test program
#
TEST_PROGRAM = TestIcal
TEST_SOURCE_FILES = $(SRC)/TestIcal.cpp

# Provide the name for the components IDL file
#
IDLFILE = $(SRC)/oeIICal.idl

# Define the base name for the generated .h and .xpt files
#
IDL_OUTPUT_BASENAME = oeIICal

# List the source files needed for building the component
#
                                                                                            
SOURCE_FILES = $(SRC)/oeICalFactory.cpp $(SRC)/oeICalImpl.cpp $(SRC)/oeICalEventImpl.cpp 
HEADER_FILES = $(SRC)/oeICalImpl.h $(SRC)/oeICalEventImpl.h

# List any custom include paths here
#
#INCLUDES = -I ./ -I../libical/src/libicalss -I../libical/src/libical -DICAL_DEBUG
INCLUDES = -I ./ -I/usr/local/include -DICAL_DEBUG

# List any custom libraries to be linked here
#
EXTRA_LIBS = -lical -licalss

# Mozilla directories
#
MOZINCLUDES = -I $(MOZZYDEV) -I $(MOZZYDEV)/nspr -I $(MOZZYDEV)/xpcom
MOZIDLS = $(MOZZYDEV)/idl
MOZLIBS = $(MOZZY)
MOZCOMPONENTS = $(MOZZY)/components
MOZINSTALL = $(DESTDIR)/$(MOZCOMPONENTS)

XPIDL = /usr/bin/xpidl

# Standard Netscape Portable Runtime and XPCOM libraries to be linked in
#
NSPR_LIBS =
XPCOM_LIBS = -L $(MOZLIBS) -lxpcom

LIBS = $(NSPR_LIBS) $(XPCOM_LIBS) $(EXTRA_LIBS)
TESTLIBS = $(NSPR_LIBS) $(XPCOM_LIBS) 



# Make targets
#

all: $(LIBRARY_NAME) $(IDLFILE) $(TEST_PROGRAM)

clean:
	rm  -f *.obj *.pdb *.res *.map *.lib *.exp *.o *.so *.dll *.xpt $(IDL_OUTPUT_BASENAME).h $(TEST_PROGRAM)

reset:
	rm -f $(MOZZY)/component.reg
	rm -f $(MOZZY)/chrome/*.rdf

install:
	install -d $(MOZINSTALL)/
	install $(LIBRARY_NAME) $(MOZINSTALL)/
	install $(IDL_OUTPUT_BASENAME).xpt $(MOZINSTALL)/

uninstall:
	rm $(MOZINSTALL)/$(LIBRARY_NAME)
	rm $(MOZINSTALL)/$(IDL_OUTPUT_BASENAME).xpt

test:
	export NSPR_LOG_MODULES=nsComponentManager:5;\
         export NSPR_LOG_FILE=xpcom.log;\
         export MOZILLA_FIVE_HOME=$(MOZZY);\
         export LD_LIBRARY_PATH=$(MOZZY);\
          ./$(TEST_PROGRAM)


# Builds xpcom shared object

$(LIBRARY_NAME): $(SOURCE_FILES) $(HEADER_FILES) $(IDL_OUTPUT_BASENAME)
	$(CC)  -shared -o $(LIBRARY_NAME) $(SOURCE_FILES)  $(INCLUDES) $(MOZINCLUDES)  $(LIBS)


# Generates .xpt and .h files from the IDL

$(IDLFILE): $(IDL_OUTPUT_BASENAME)

$(IDL_OUTPUT_BASENAME):
	$(XPIDL) -m typelib -w -I $(MOZIDLS) -o $(IDL_OUTPUT_BASENAME) $(IDLFILE)
	$(XPIDL) -m header -w -I $(MOZIDLS) -o $(IDL_OUTPUT_BASENAME) $(IDLFILE)


# Build test program

$(TEST_PROGRAM) : $(TEST_SOURCE_FILES)
	$(CC) -g -o  $(TEST_PROGRAM) $(TEST_SOURCE_FILES) $(MOZINCLUDES)  $(INCLUDES) $(TESTLIBS)  -lplds4 -lplc4 -lnspr4
   



# distribution build

DISTFILES= $(SOURCE_FILES) $(HEADER_FILES) $(IDLFILE) $(TEST_SOURCE_FILES) Makefile $(EXTRA_DISTFILES)

distdir = $(PACKAGE)-$(VERSION)

dist: distdir
	chmod -R a+r $(distdir)
	tar czf $(distdir).tar.gz $(distdir)
	rm -rf $(distdir)

distdir:
	rm -rf $(distdir)
	mkdir $(distdir)
	for file in $(DISTFILES); do \
       cp -p $$file $(distdir) ;\
	done
